#include "FirstPass.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
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

struct FunctionCallEntry {
    std::string calledFunction;
    int line;
};

struct MatchInfo {
    std::string funcName;
    std::string fileName;
    int funcDefLine = -1;
    std::vector<MatchEntry> matches;
    std::vector<FunctionCallEntry> functionCalls; // All function calls within this function
};

// Helper function to extract path from dReal-clang-tidy/ onwards
std::string extractPathFromDRealClangTidy(const std::string& fullPath) {
    size_t pos = fullPath.find("dReal-clang-tidy/");
    if (pos != std::string::npos) {
        return fullPath.substr(pos);
    }
    // If "dReal-clang-tidy/" is not found, return just the filename
    return fullPath.substr(fullPath.find_last_of("/\\") + 1);
}

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

        // Handle function calls (for tracking all function calls within functions)
        if (const CallExpr *call = Result.Nodes.getNodeAs<CallExpr>("functionCall")) {
            // Find enclosing function
            const FunctionDecl *enclosingFunc = nullptr;
            auto parents = Result.Context->getParents(*call);
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

            if (enclosingFunc) {
                SourceManager &SM = *Result.SourceManager;
                std::string fileName = SM.getFilename(enclosingFunc->getLocation()).str();
                std::string funcName = enclosingFunc->getNameAsString();
                int funcLine = SM.getSpellingLineNumber(enclosingFunc->getLocation());
                std::string key = fileName + ":" + funcName + ":" + std::to_string(funcLine);

                // Initialize function info if not exists
                if (functionMatches.find(key) == functionMatches.end()) {
                    functionMatches[key].funcName = funcName;
                    functionMatches[key].fileName = fileName;
                    functionMatches[key].funcDefLine = funcLine;
                }

                // Get called function name
                if (const FunctionDecl *calledFunc = call->getDirectCallee()) {
                    std::string calledFuncName = calledFunc->getNameAsString();
                    int callLine = SM.getSpellingLineNumber(call->getBeginLoc());

                    functionMatches[key].functionCalls.push_back({calledFuncName, callLine});
                }
            }
            return;
        }

        // Now handle the specific matches for primary nodes
        const FunctionDecl *enclosingFunc = nullptr;
        const Stmt *matchedStmt = nullptr;
        const Decl *matchedDecl = nullptr;
        std::string matchType;

        if (const CallExpr *call = Result.Nodes.getNodeAs<CallExpr>("ibexCall")) {
            matchedStmt = call;
            matchType = "ibexCall";
        } else if (const VarDecl *var = Result.Nodes.getNodeAs<VarDecl>("floatMathDec")) {
            matchedDecl = var;
            matchType = "floatMathDec";
        } else if (const BinaryOperator *op = Result.Nodes.getNodeAs<BinaryOperator>("floatMathAsign")) {
            matchedStmt = op;
            matchType = "floatMathAsign";
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
    // The parent matcher that lets us capture function definitions
    finder.addMatcher(
        functionDecl(isDefinition()).bind("allFunctions"),
        &writer
    );

    // Matcher for all function calls (for tracking function calls within functions)
    finder.addMatcher(
        callExpr().bind("functionCall"),
        &writer
    );

    // Ibex call matcher
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

    // FE_UPWARD is a macro for 0x800, so look for that instead
    finder.addMatcher(
        callExpr(
            callee(functionDecl(matchesName("fesetround"))),
            hasAnyArgument(
                ignoringParenImpCasts(integerLiteral(equals(0x0800)))
            )
        ).bind("SetUpward"),
        &writer
    );

    // FE_TONEAREST is 0x000
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

// Analyze primary functions (functions with double math and/or ibex calls)
json analyzePrimaryFunctions(const std::map<std::string, MatchInfo>& functionMatches) {
    json analysisResults = json::array();

    for (const auto &pair : functionMatches) {
        const MatchInfo &info = pair.second;

        // Check if this is a primary function (has ibex calls or float math)
        bool hasIbex = false;
        bool hasDouble = false;

        for (const auto &match : info.matches) {
            if (match.type == "ibexCall") {
                hasIbex = true;
            } else if (match.type == "floatMathDec" || match.type == "floatMathAsign") {
                hasDouble = true;
            }
        }

        // Skip non-primary functions
        if (!hasIbex && !hasDouble) {
            continue;
        }

        json funcAnalysis;
        funcAnalysis["function"] = info.funcName;
        funcAnalysis["location"] = extractPathFromDRealClangTidy(info.fileName);
        funcAnalysis["line"] = info.funcDefLine;

        // Case 1: Ibex match and no double match
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
                funcAnalysis["pre-requirement"] = "No Requirement";
                funcAnalysis["output-rounding-mode"] = "Upper";
            } else if (hasSetRoundNearest) {
                funcAnalysis["error"] = "setRoundNearest before ibex match - this cannot be allowed!";
                funcAnalysis["pre-requirement"] = "Error";
                funcAnalysis["output-rounding-mode"] = "Error";
            } else {
                funcAnalysis["pre-requirement"] = "Upper";
                funcAnalysis["output-rounding-mode"] = "Upper";
            }
        }
        // Case 2: Double and no ibex
        else if (hasDouble && !hasIbex) {
            // Look for setRound calls before double math
            bool hasSetRoundUpper = false;
            bool hasSetRoundNearest = false;

            // Find first double match line
            int firstDoubleLine = INT_MAX;
            for (const auto &match : info.matches) {
                if ((match.type == "floatMathDec" || match.type == "floatMathAsign")
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
                funcAnalysis["pre-requirement"] = "Error";
                funcAnalysis["output-rounding-mode"] = "Error";
            } else if (hasSetRoundNearest) {
                funcAnalysis["pre-requirement"] = "No Requirement";
                funcAnalysis["output-rounding-mode"] = "Nearest";
            } else {
                funcAnalysis["pre-requirement"] = "Nearest";
                funcAnalysis["output-rounding-mode"] = "Nearest";
            }
        }
        // Case 3: Both double and ibex
        else if (hasDouble && hasIbex) {
            // 0 means nearest, 1 means upper, 2 means not set
            int requiredRoundingMode = 2; // rrm
            int currentRoundingMode = 2;  // crm
            bool requiredSet = false;
            bool errorOccurred = false;

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
                } else if (match.type == "floatMathDec" || match.type == "floatMathAsign") {
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
                funcAnalysis["pre-requirement"] = roundingModeToString(requiredRoundingMode);
                funcAnalysis["output-rounding-mode"] = roundingModeToString(currentRoundingMode);
            } else {
                funcAnalysis["pre-requirement"] = "Error";
                funcAnalysis["output-rounding-mode"] = "Error";
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

    // 1. Create PrimaryFunctions.json for functions with double math and/or ibex calls
    json primaryAnalysis = analyzePrimaryFunctions(writer.functionMatches);
    std::ofstream primaryFile("PrimaryFunctions.json");
    primaryFile << primaryAnalysis.dump(4);
    primaryFile.close();
    std::cout << "Primary function analysis written to PrimaryFunctions.json" << std::endl;

    // 2. Create functionsFull.json - all functions with their function calls (duplicates included)
    json fullFunctions = json::array();
    for (const auto &pair : writer.functionMatches) {
        const MatchInfo &info = pair.second;

        json funcJson;
        funcJson["function"] = info.funcName;
        funcJson["location"] = extractPathFromDRealClangTidy(info.fileName);
        funcJson["line"] = info.funcDefLine;

        // Add all function calls (with duplicates)
        json callArray = json::array();
        for (const auto &call : info.functionCalls) {
            callArray.push_back({{"called_function", call.calledFunction}, {"line", call.line}});
        }
        funcJson["function_calls"] = callArray;

        fullFunctions.push_back(funcJson);
    }

    std::ofstream fullFile("functionsFull.json");
    fullFile << fullFunctions.dump(4);
    fullFile.close();
    std::cout << "Full function analysis written to functionsFull.json" << std::endl;

    // 3. Create functionsParentList.json - unique list of functions called per function
    json parentList = json::array();
    for (const auto &pair : writer.functionMatches) {
        const MatchInfo &info = pair.second;

        json funcJson;
        funcJson["function"] = info.funcName;
        funcJson["location"] = extractPathFromDRealClangTidy(info.fileName);
        funcJson["line"] = info.funcDefLine;

        // Create unique set of called functions
        std::set<std::string> uniqueCalls;
        for (const auto &call : info.functionCalls) {
            uniqueCalls.insert(call.calledFunction);
        }

        // Convert set to array
        json uniqueCallArray = json::array();
        for (const auto &call : uniqueCalls) {
            uniqueCallArray.push_back(call);
        }
        funcJson["unique_function_calls"] = uniqueCallArray;

        parentList.push_back(funcJson);
    }

    std::ofstream parentFile("functionsParentList.json");
    parentFile << parentList.dump(4);
    parentFile.close();
    std::cout << "Parent list analysis written to functionsParentList.json" << std::endl;

    return result;
}