//===--- PreventIbexFloatMathInSameLineCheck.h - clang-tidy -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_READABILITY_PREVENTIBEXFLOATMATHINSAMELINECHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_READABILITY_PREVENTIBEXFLOATMATHINSAMELINECHECK_H

#include "../ClangTidyCheck.h"

namespace clang::tidy::readability {

/// This aims to prevent float math inside ibex calls
///
/// For the user-facing documentation see:
/// http://clang.llvm.org/extra/clang-tidy/checks/readability/prevent-ibex-float-math-in-same-line.html
class PreventIbexFloatMathInSameLineCheck : public ClangTidyCheck {
public:
  PreventIbexFloatMathInSameLineCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
  bool isLanguageVersionSupported(const LangOptions &LangOpts) const override {
    return LangOpts.CPlusPlus;
  }
};

} // namespace clang::tidy::readability

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_READABILITY_PREVENTIBEXFLOATMATHINSAMELINECHECK_H
