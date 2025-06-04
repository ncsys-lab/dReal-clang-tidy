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

    virtual void run(const MatchFinder::MatchResult &Result) override {
        const FunctionDecl *enclosingFunc = nullptr;
        const Stmt *matchedStmt = nullptr;
        const Decl *matchedDecl = nullptr;
        std::string matchType;

        if (const CallExpr *call = Result.Nodes.getNodeAs<CallExpr>("ibexCall")) {
            matchedStmt = call;
            matchType = "ibex";
        } else if (const VarDecl *var = Result.Nodes.getNodeAs<VarDecl>("floatMath")) {
            matchedDecl = var;
            matchType = "double";
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
            for (const auto &parent : parents) {
                if (const FunctionDecl *fd = parent.get<FunctionDecl>()) {
                    enclosingFunc = fd;
                    break;
                }
            }
        } else if (matchedDecl) {
            auto parents = Result.Context->getParents(*matchedDecl);
            for (const auto &parent : parents) {
                if (const FunctionDecl *fd = parent.get<FunctionDecl>()) {
                    enclosingFunc = fd;
                    break;
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
    finder.addMatcher(
        callExpr(
            callee(functionDecl(matchesName(".*ibex.*")))
        ).bind("ibexCall"),
        &writer
    );
    finder.addMatcher(
        varDecl(
            hasInitializer(
                binaryOperator(
                    hasLHS(hasType(realFloatingPointType())),
                    hasRHS(hasType(realFloatingPointType()))
                )
            )
        ).bind("floatMath"),
        &writer
    );
    finder.addMatcher(
        callExpr(
            callee(functionDecl(matchesName("fesetround"))),
            hasAnyArgument(
                ignoringParenImpCasts(
                    declRefExpr(to(varDecl(hasName("FE_UPWARD"))))
                )
            )
        ).bind("SetUpward"),
        &writer
    );
    finder.addMatcher(
        callExpr(
            callee(functionDecl(matchesName("fesetround"))),
            hasAnyArgument(
                ignoringParenImpCasts(
                    declRefExpr(to(varDecl(hasName("FE_TONEAREST"))))
                )
            )
        ).bind("SetNearest"),
        &writer
    );
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

        // Add matches in order
        json matchArray = json::array();
        for (const auto &m : sortedMatches) {
            matchArray.push_back({{"type", m.type}, {"line", m.line}});
        }
        funcJson["matches"] = matchArray;



        j.push_back(funcJson);
    }

    std::ofstream outFile("functions.json");
    outFile << j.dump(4);
    outFile.close();

    std::cout << "Function matches written to functions.json" << std::endl;
    return result;
}