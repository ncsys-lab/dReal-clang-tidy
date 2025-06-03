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
using json = nlohmann::json;

// Helper structure to store match information
struct MatchInfo {
    bool hasIbexCall = false;
    std::set<std::string> locations; // Source locations or function names where matches occur
};

// Matcher callback for matching calls to ibex functions with arithmetic arguments
class Write_Solved : public MatchFinder::MatchCallback {
public:
    std::map<std::string, MatchInfo> functionMatches; // Track matches per function

    virtual void run(const MatchFinder::MatchResult &Result) override {
        if (const CallExpr *call = Result.Nodes.getNodeAs<CallExpr>("ibexCall")) {
            const FunctionDecl *callee = call->getDirectCallee();
            if (callee) {
                std::string funcName = callee->getNameAsString();
                functionMatches[funcName].hasIbexCall = true;
                // Optionally, add location info for each match
                SourceManager &SM = *Result.SourceManager;
                SourceLocation Loc = call->getBeginLoc();
                if (Loc.isValid()) {
                    std::string locStr = SM.getFilename(Loc).str() + ":" +
                                        std::to_string(SM.getSpellingLineNumber(Loc));
                    functionMatches[funcName].locations.insert(locStr);
                }
            }
        }
    }
};

// Helper function to configure the ibex call matcher
void configureIbexCallMatcher(MatchFinder &finder, Write_Solved &writer) {
    finder.addMatcher(
        callExpr(
            callee(
                functionDecl(
                    matchesName(".*ibex.*")
                )
            )
        ).bind("ibexCall"),
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

    // Configure matcher logic in a separate function
    configureIbexCallMatcher(finder, writer);

    // Run the tool
    int result = Tool.run(newFrontendActionFactory(&finder).get());

    // Prepare JSON output with "ibex: true" and optional locations for each matched function
    json j = json::array();
    for (const auto &pair : writer.functionMatches) {
        json funcJson;
        funcJson["function"] = pair.first;
        funcJson["ibex"] = pair.second.hasIbexCall;
        // Add locations if needed
        if (!pair.second.locations.empty()) {
            funcJson["locations"] = pair.second.locations;
        }
        j.push_back(funcJson);
    }

    std::ofstream outFile("functions.json");
    outFile << j.dump(4); // Pretty print with 4 spaces
    outFile.close();

    std::cout << "Function matches with ibex calls written to functions.json" << std::endl;

    return result;
}
