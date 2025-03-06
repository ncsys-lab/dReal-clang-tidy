//===--- PreventIbexFloatMathInSameLineCheck.cpp - clang-tidy -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "PreventIbexFloatMathInSameLineCheck.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang::tidy::readability {

void PreventIbexFloatMathInSameLineCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(callExpr(
    callee(
        functionDecl(
            matchesName(".*ibex.*")
        )
    ),
    hasAnyArgument(
        binaryOperator(
            anyOf(
                hasOperatorName("+"),
                hasOperatorName("-"),
                hasOperatorName("*"),
                hasOperatorName("/"),
                hasOperatorName("%")
            )
        )
    ),
    hasAnyArgument(
        hasType(
            realFloatingPointType()
        )
    )
).bind("overcomplicatedLine"), this);
}

void PreventIbexFloatMathInSameLineCheck::check(const MatchFinder::MatchResult &Result) {
    if (const auto *Using = Result.Nodes.getNodeAs<UsingDecl>("overcompilatedLine")) {
        diag(Using->getLocation(), "No float math inside an ibex call"
             "Please avoid doing ibex calls and float math simultaneously");
    }
}

} // namespace clang::tidy::readability
