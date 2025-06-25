#include "FirstPass.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include "json.hpp"
#include "clang/AST/AST.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using json = nlohmann::ordered_json;

struct MatchEntry {
    std::string type; // "ibex", "double", "setroundupper", "setroundlower"
    int line;
};

struct MatchInfo {
    std::string funcName;
    std::string fileName;
    int funcDefLine = -1;
    std::vector<MatchEntry> matches;
};

class Write_Solved : public MatchFinder::MatchCallback {
public:
    std::map<std::string, MatchInfo> functionMatches;
    std::set<std::string> allFunctions; // Track all functions encountered

    virtual void run(const MatchFinder::MatchResult &Result) override {
        // First, check if this is a function declaration match
        if (const FunctionDecl *funcDecl = Result.Nodes.getNodeAs<FunctionDecl>("allFunctions")) {
            // Skip if it's just a declaration without a body
            if (!funcDecl->hasBody()) return;

            SourceManager &SM = *Result.SourceManager;
            std::string fileName = SM.getFilename(funcDecl->getLocation()).str();
            std::string funcName = funcDecl->getNameAsString();
            int funcLine = SM.getSpellingLineNumber(funcDecl->getLocation());

            // Composite key: filename:functionname:line to handle overloaded functions
            std::string key = fileName + ":" + funcName + ":" + std::to_string(funcLine);

            // Store function information
            if (functionMatches.find(key) == functionMatches.end()) {
                functionMatches[key].funcName = funcName;
                functionMatches[key].fileName = fileName;
                functionMatches[key].funcDefLine = funcLine;
            }
            allFunctions.insert(key);
            return;
        }

        // Now handle the specific matches
        const FunctionDecl *enclosingFunc = nullptr;
        const Stmt *matchedStmt = nullptr;
        const Decl *matchedDecl = nullptr;
        std::string matchType;

        if (const CallExpr *call = Result.Nodes.getNodeAs<CallExpr>("ibexCall")) {
            matchedStmt = call;
            matchType = "ibexCall";
        } else if (const VarDecl *var = Result.Nodes.getNodeAs<VarDecl>("floatMath")) {
            matchedDecl = var;
            matchType = "floatMath";
        } else if (const CallExpr *call = Result.Nodes.getNodeAs<CallExpr>("SetUpward")) {
            matchedStmt = call;
            matchType = "setroundupper";
        } else if (const CallExpr *call = Result.Nodes.getNodeAs<CallExpr>("SetNearest")) {
            matchedStmt = call;
            matchType = "setroundlower";
        } else {
            return;
        }

        // Find enclosing function
        if (matchedStmt) {
            auto parents = Result.Context->getParents(*matchedStmt);
            while (!parents.empty() && !enclosingFunc) {
                for (const auto &parent : parents) {
                    if (const FunctionDecl *fd = parent.get<FunctionDecl>()) {
                        enclosingFunc = fd;
                        break;
                    }
                }
                if (!enclosingFunc && !parents.empty()) {
                    parents = Result.Context->getParents(parents[0]);
                }
            }
        } else if (matchedDecl) {
            auto parents = Result.Context->getParents(*matchedDecl);
            while (!parents.empty() && !enclosingFunc) {
                for (const auto &parent : parents) {
                    if (const FunctionDecl *fd = parent.get<FunctionDecl>()) {
                        enclosingFunc = fd;
                        break;
                    }
                }
                if (!enclosingFunc && !parents.empty()) {
                    parents = Result.Context->getParents(parents[0]);
                }
            }
        }
        if (!enclosingFunc) return;

        SourceManager &SM = *Result.SourceManager;
        std::string fileName = SM.getFilename(enclosingFunc->getLocation()).str();
        std::string funcName = enclosingFunc->getNameAsString();
        int funcLine = SM.getSpellingLineNumber(enclosingFunc->getLocation());

        // Composite key: filename:functionname:line to handle overloaded functions
        std::string key = fileName + ":" + funcName + ":" + std::to_string(funcLine);

        // Store function definition location only once
        if (functionMatches.find(key) == functionMatches.end()) {
            functionMatches[key].funcName = funcName;
            functionMatches[key].fileName = fileName;
            functionMatches[key].funcDefLine = funcLine;
        }

        // Store each match with its line number
        int line = 0;
        if (matchedStmt)
            line = SM.getSpellingLineNumber(matchedStmt->getBeginLoc());
        else if (matchedDecl)
            line = SM.getSpellingLineNumber(matchedDecl->getBeginLoc());

        functionMatches[key].matches.push_back({matchType, line});
    }
};

void configureMatchers(MatchFinder &finder, Write_Solved &writer) {
	//the parent matcher that lets us capture function definitions
    finder.addMatcher(
        functionDecl(isDefinition()).bind("allFunctions"),
        &writer
    );


    finder.addMatcher(
        callExpr(
            callee(functionDecl(matchesName(".*ibex.*")))
        ).bind("ibexCall"),
        &writer
    );

    // Float math variable declaration matcher
    finder.addMatcher(
        varDecl(
            hasType(realFloatingPointType()),
            hasInitializer(
                anyOf(
                    binaryOperator(
                        anyOf(
                            hasLHS(hasType(realFloatingPointType())),
                            hasRHS(hasType(realFloatingPointType()))
                        )
                    ),
                    callExpr(hasType(realFloatingPointType()))
                )
            )
        ).bind("floatMathDec"),
        &writer
    );

    // Float math assignment matcher
    finder.addMatcher(
        binaryOperator(
            hasOperatorName("="),
            hasLHS(hasType(realFloatingPointType())),
            hasRHS(
                anyOf(
                    binaryOperator(
                        anyOf(
                            hasLHS(hasType(realFloatingPointType())),
                            hasRHS(hasType(realFloatingPointType()))
                        )
                    ),
                    callExpr(hasType(realFloatingPointType()))
                )
            )
        ).bind("floatMathAsign"),
        &writer
    );

    //these matchers are outdated
	//FE_UPWARD is a macro for 0x800, so look for that instead
    finder.addMatcher(
        callExpr(
    		callee(functionDecl(matchesName("fesetround"))),
    		hasAnyArgument(
        		ignoringParenImpCasts(integerLiteral(equals(0x0800)))
    		)
		).bind("SetUpward"),
        &writer
    );
	//FE_TONEAREST is 0x000
    finder.addMatcher(
        callExpr(
    		callee(functionDecl(matchesName("fesetround"))),
    		hasAnyArgument(
        		ignoringParenImpCasts(integerLiteral(equals(0x0000)))
    		)
		).bind("SetNearest"),
        &writer
    );
}

std::string roundingModeToString(int mode) {
    switch(mode) {
        case 0: return "Nearest";
        case 1: return "Upper";
        case 2: return "Not Set";
        default: return "Unknown";
    }
}


// I forgot which is which and assumed upper for ibex and nearest for double math
json analyzeFunctionRoundingModes(const std::map<std::string, MatchInfo>& functionMatches) {
    json analysisResults = json::array();

    for (const auto &pair : functionMatches) {
        const MatchInfo &info = pair.second;

        json funcAnalysis;
        funcAnalysis["function"] = info.funcName;
        funcAnalysis["location"] = info.fileName.substr(info.fileName.find_last_of("/\\") + 1);
        funcAnalysis["line"] = info.funcDefLine;

        // Count match types
        bool hasIbex = false;
        bool hasDouble = false;

        for (const auto &match : info.matches) {
            if (match.type == "ibexCall") {
                hasIbex = true;
            } else if (match.type == "floatMath" || match.type == "floatMathDec" || match.type == "floatMathAsign") {
                hasDouble = true;
            }
        }

        // Case 1: No matches
        if (info.matches.empty()) {
            funcAnalysis["pre_condition"] = "No Requirement";
            funcAnalysis["post_condition"] = "Not Set";
            analysisResults.push_back(funcAnalysis);
            continue;
        }

        // Case 2: Ibex match and no double match
        if (hasIbex && !hasDouble) {
            // Look for setRound calls before ibex
            bool hasSetRoundUpper = false;
            bool hasSetRoundNearest = false;

            // Find first ibex match line
            int firstIbexLine = INT_MAX;
            for (const auto &match : info.matches) {
                if (match.type == "ibexCall" && match.line < firstIbexLine) {
                    firstIbexLine = match.line;
                }
            }

            // Check for rounding mode sets before first ibex
            for (const auto &match : info.matches) {
                if (match.line < firstIbexLine) {
                    if (match.type == "setroundupper") {
                        hasSetRoundUpper = true;
                    } else if (match.type == "setroundlower") {
                        hasSetRoundNearest = true;
                    }
                }
            }

            if (hasSetRoundUpper) {
                funcAnalysis["pre_condition"] = "No Requirement";
                funcAnalysis["post_condition"] = "Upper";
            } else if (hasSetRoundNearest) {
                funcAnalysis["error"] = "setRoundNearest before ibex match - this cannot be allowed!";
                funcAnalysis["pre_condition"] = "Error";
                funcAnalysis["post_condition"] = "Error";
            } else {
                funcAnalysis["pre_condition"] = "Upper";
                funcAnalysis["post_condition"] = "Upper";
            }
        }
        // Case 3: Double and no ibex
        else if (hasDouble && !hasIbex) {
            // Look for setRound calls before double math
            bool hasSetRoundUpper = false;
            bool hasSetRoundNearest = false;

            // Find first double match line
            int firstDoubleLine = INT_MAX;
            for (const auto &match : info.matches) {
                if ((match.type == "floatMath" || match.type == "floatMathDec" || match.type == "floatMathAsign")
                    && match.line < firstDoubleLine) {
                    firstDoubleLine = match.line;
                }
            }

            // Check for rounding mode sets before first double
            for (const auto &match : info.matches) {
                if (match.line < firstDoubleLine) {
                    if (match.type == "setroundupper") {
                        hasSetRoundUpper = true;
                    } else if (match.type == "setroundlower") {
                        hasSetRoundNearest = true;
                    }
                }
            }

            if (hasSetRoundUpper) {
                funcAnalysis["error"] = "setRoundUpper before double match - this cannot be allowed!";
                funcAnalysis["pre_condition"] = "Error";
                funcAnalysis["post_condition"] = "Error";
            } else if (hasSetRoundNearest) {
                funcAnalysis["pre_condition"] = "No Requirement";
                funcAnalysis["post_condition"] = "Nearest";
            } else {
                funcAnalysis["pre_condition"] = "Nearest";
                funcAnalysis["post_condition"] = "Nearest";
            }
        }
        // Case 4: Both double and ibex
        else if (hasDouble && hasIbex) {
            // 0 means nearest, 1 means upper, 2 means not set
            int requiredRoundingMode = 2; // rrm
            int currentRoundingMode = 2;  // crm
            bool requiredSet = false;
            bool errorOccurred = false;
            json warnings = json::array();

            // Sort matches by line number
            auto sortedMatches = info.matches;
            std::sort(sortedMatches.begin(), sortedMatches.end(),
                     [](const MatchEntry &a, const MatchEntry &b) { return a.line < b.line; });

            // Process matches in order
            for (const auto &match : sortedMatches) {
                if (errorOccurred) break;

                if (match.type == "setroundupper") {
                    currentRoundingMode = 1;
                    requiredSet = true;
                } else if (match.type == "setroundlower") {
                    currentRoundingMode = 0;
                    requiredSet = true;
                } else if (match.type == "ibexCall") {
                    if (currentRoundingMode == 2) {
                        if (!requiredSet) {
                            requiredRoundingMode = 1;
                            requiredSet = true;
                        }
                        currentRoundingMode = 1;
                    } else if (currentRoundingMode == 0) {
                        funcAnalysis["error"] = "ibex was called with an incorrect rounding mode (line " + std::to_string(match.line) + ")";
                        errorOccurred = true;
                        break;
                    }
                    // If currentRoundingMode == 1, ibex is called correctly, so crm stays 1
                } else if (match.type == "floatMath" || match.type == "floatMathDec" || match.type == "floatMathAsign") {
                    if (currentRoundingMode == 2) {
                        if (!requiredSet) {
                            requiredRoundingMode = 0;
                            requiredSet = true;
                        }
                        currentRoundingMode = 0;
                    } else if (currentRoundingMode == 1) {
                        funcAnalysis["error"] = "Double math with upper rounding mode - this cannot be allowed! (line " + std::to_string(match.line) + ")";
                        errorOccurred = true;
                        break;
                    }
                    // If currentRoundingMode == 0, double math is done correctly, so crm stays 0
                }
            }

            if (!errorOccurred) {
                funcAnalysis["pre_condition"] = roundingModeToString(requiredRoundingMode);
                funcAnalysis["post_condition"] = roundingModeToString(currentRoundingMode);
            } else {
                funcAnalysis["pre_condition"] = "Error";
                funcAnalysis["post_condition"] = "Error";
            }

            if (!warnings.empty()) {
                funcAnalysis["warnings"] = warnings;
            }
        }

        analysisResults.push_back(funcAnalysis);
    }

    return analysisResults;
}

int main(int argc, const char **argv) {
    llvm::cl::OptionCategory ToolCategory("first-pass");
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, ToolCategory);
    auto& OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

    Write_Solved writer;
    MatchFinder finder;
    configureMatchers(finder, writer);

    int result = Tool.run(newFrontendActionFactory(&finder).get());

    // Prepare JSON output
    json j = json::array();
    for (const auto &pair : writer.functionMatches) {
        const MatchInfo &info = pair.second;

		// Skip functions without any matches
        if (info.matches.empty()) {
            continue;
        }

        json funcJson;

        // Combine function name and definition line
        std::string funcDescription = info.funcName + " (defined or overloaded) at: " +
                                      info.fileName.substr(info.fileName.find_last_of("/\\") + 1) +
                                      " - " + std::to_string(info.funcDefLine);
        funcJson["function"] = funcDescription;
        // Put file at the end with indent prefix
        funcJson["file"] = "    " + info.fileName;

        // Sort matches by line number
        auto sortedMatches = info.matches;
        std::sort(sortedMatches.begin(), sortedMatches.end(),
                  [](const MatchEntry &a, const MatchEntry &b) { return a.line < b.line; });

        // Add matches in order (empty array if no matches)
        json matchArray = json::array();
        for (const auto &m : sortedMatches) {
            matchArray.push_back({{"type", m.type}, {"line", m.line}});
        }
        funcJson["matches"] = matchArray;

        j.push_back(funcJson);
    }

    std::ofstream outFile("dump.json");
    outFile << j.dump(4);
    outFile.close();

    std::cout << "Function matches written to dump.json" << std::endl;

    std::cout << "Now doing function analysis" << std::endl;

    // Add the new function analysis logic and write to functions.json
    json analysisResults = analyzeFunctionRoundingModes(writer.functionMatches);

    std::ofstream functionsFile("functions.json");
    functionsFile << analysisResults.dump(4);
    functionsFile.close();

    std::cout << "Function analysis written to functions.json" << std::endl;

    return result;
}