//
// Created by maxim on 5/18/25.
//

#include "FirstPass.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "json.hpp"
#include "clang/AST/AST.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/CommandLine.h"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using json = nlohmann::json;

// AST Matcher callback to collect function names
class FunctionNameCollector : public MatchFinder::MatchCallback {
public:
    std::vector<std::string> functionNames;

    virtual void run(const MatchFinder::MatchResult &Result) override {
        if (const FunctionDecl *func = Result.Nodes.getNodeAs<FunctionDecl>("funcDecl")) {
            // Only collect functions defined in the main file
            if (Result.SourceManager->isInMainFile(func->getLocation())) {
                functionNames.push_back(func->getNameAsString());
            }
        }
    }
};

int main(int argc, const char **argv) {
    llvm::cl::OptionCategory ToolCategory("function-finder");
    CommonOptionsParser OptionsParser(argc, argv, ToolCategory);
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

    // Set up matcher for all function declarations (excluding implicit ones)
    FunctionNameCollector collector;
    MatchFinder finder;
    finder.addMatcher(functionDecl(isExpansionInMainFile(), unless(isImplicit())).bind("funcDecl"), &collector);

    // Run the tool
    int result = Tool.run(newFrontendActionFactory(&finder).get());

    // Write function names to JSON
    json j;
    j["functions"] = collector.functionNames;

    std::ofstream outFile("functions.json");
    outFile << j.dump(4); // Pretty print with 4 spaces
    outFile.close();

    std::cout << "Function names written to functions.json" << std::endl;

    return result;
}