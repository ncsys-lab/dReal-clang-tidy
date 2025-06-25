//===--- PreventUsingIbexCheck.cpp - clang-tidy ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "PreventUsingIbexCheck.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang::tidy::readability {

  void PreventUsingIbexCheck::registerMatchers(MatchFinder *Finder) {
    Finder->addMatcher(
        usingDecl(
            hasAnyUsingShadowDecl(
                hasTargetDecl(
                    namedDecl(
                        matchesName(".*ibex.*")
                    )
                )
            )
        ).bind("usingDecl"),
    this);

    Finder->addMatcher(
        usingDecl(
            hasAnyUsingShadowDecl(
                hasTargetDecl(
                    namedDecl(
                        hasParent(
                            namespaceDecl(matchesName(".*ibex.*")
                            )
                        )
                    )
                )
            )
        ).bind("usingDirective"),
    this);

    Finder->addMatcher(typeAliasDecl(hasType(
        hasUnqualifiedDesugaredType(recordType(hasDeclaration(
          cxxRecordDecl(matchesName(".*ibex.*"))))))).bind("alias"),
      this);

	Finder->addMatcher(
        callExpr(
    		callee(functionDecl(matchesName("round_upward")))

		).bind("SetUpward"),
        this
    );

    Finder->addMatcher(
        callExpr(
    		callee(functionDecl(matchesName("round_nearest")))

		).bind("SetNearest"),
        this
    );
  }

  void PreventUsingIbexCheck::check(const MatchFinder::MatchResult &Result) {
    if (const auto *Using = Result.Nodes.getNodeAs<UsingDecl>("usingDecl")) {
      diag(Using->getLocation(), "No using declaration allowed for ibex. "
           "Directly reference all ibex functions and types.");
    }

    if (const auto *UsingDir = Result.Nodes.getNodeAs<UsingDirectiveDecl>("usingDirective")) {
      diag(UsingDir->getLocation(), "No using directive allowed for ibex namespace. "
           "Directly reference all ibex functions and types.");
    }

    if (const auto *Alias = Result.Nodes.getNodeAs<TypeAliasDecl>("alias")) {
      diag(Alias->getLocation(), "Type alias for ibex types is not allowed. "
           "Directly reference all ibex types.");
    }

	if (const auto *Alias = Result.Nodes.getNodeAs<TypeAliasDecl>("SetUpward")) {
      diag(Alias->getLocation(), "Set call "
           "test");
    }

	if (const auto *Alias = Result.Nodes.getNodeAs<TypeAliasDecl>("SetNearest")) {
      diag(Alias->getLocation(), "Set call "
           "test");
    }
  }

} // namespace clang::tidy::readability
