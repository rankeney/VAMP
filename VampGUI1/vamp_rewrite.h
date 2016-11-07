/*  Copyright 2016, Robert Ankeney

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/
*/

#ifndef VAMP_REWRITE_H
#define VAMP_REWRITE_H

//#define DEBUG_REWRITE_NODES

#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"

#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/Lexer.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include <iostream>

//#define REWRITER_DEBUG

#define ENDL "\n"

using namespace clang;
using namespace std;

// If I were clever, I might subclass this under Rewriter.

typedef struct {
  SourceLocation lhsLoc;
  string lhsString;
  SourceLocation rhsLoc;
  string rhsString;
  int occurrence;
  int count;
} REWRITE_NODE;

// Defines a left-hand-side SourceLocation sort routine for REWRITE_NODE.
struct rewriteLocLeftCompare {
  bool operator ()(REWRITE_NODE const& a, REWRITE_NODE const& b) const
  {
    if ((a.lhsLoc.getRawEncoding() < b.lhsLoc.getRawEncoding()) ||
        (((a.lhsLoc.getRawEncoding() == b.lhsLoc.getRawEncoding()) &&
          ((a.rhsLoc.getRawEncoding() > b.rhsLoc.getRawEncoding()) ||
           ((a.rhsLoc.getRawEncoding() == b.rhsLoc.getRawEncoding()) &&
           ((a.occurrence > b.occurrence)))))))
      return true;
    return false;
  }
};

// Defines a right-hand-side SourceLocation sort routine for REWRITE_NODE.
struct rewriteLocRightCompare {
  bool operator ()(REWRITE_NODE const& a, REWRITE_NODE const& b) const
  {
    if ((a.rhsLoc.getRawEncoding() < b.rhsLoc.getRawEncoding()) ||
        (((a.rhsLoc.getRawEncoding() == b.rhsLoc.getRawEncoding()) &&
         ((a.lhsLoc.getRawEncoding() > b.lhsLoc.getRawEncoding()) ||
          ((a.lhsLoc.getRawEncoding() == b.lhsLoc.getRawEncoding()) &&
           ((a.occurrence > b.occurrence)))))))
      return true;
    return false;
  }
};

class vampRewrite
{
public:
  vampRewrite();

  // Add a left/right pair rewriter node at specified left/right locations
  // with specified left/right rewrite strings
#ifdef DEBUG_REWRITE_NODES
  void showRewriteNodes();
#endif
  void addRewriteNode(SourceLocation lhsLoc,
                      string lhsString,
                      SourceLocation rhsLoc,
                      string rhsString);

  // Create a MCDC rewrite node
  // This implies we have the left side of the expression, so push
  // that information onto the stack and pop when we have the right
  // side of the expression
  void addMCDCLeftNode(SourceLocation lhsLoc, string lhsString);

  // Complete a MCDC rewrite node
  // This implies we have the right side of the expression, so pop
  // the left side information from the stack and and add the right
  // side information. Push the result onto the rewriter stack.
  void addMCDCRightNode(SourceLocation rhsLoc, string rhsString);

  void InsertText(SourceLocation loc, string text);

  void ReplaceText(SourceLocation loc, int count, string text);

  void ProcessRewriteNodes(Rewriter &Rewrite);

private:
  // When processing a MCDC expression for rewriting, build a set of
  // left and right hand side matching rewrite strings.
  // Add to rewriteNodes when each mcdcRewriteNodes is popped from stack.
  vector<REWRITE_NODE> mcdcRewriteNodes;

  // When processing a branch expression for rewriting, build a set of
  // left and right hand side matching rewrite strings.
  // Add to rewriteNodes when each branchRewriteNodes is popped from stack.
  vector<REWRITE_NODE> branchRewriteNodes;

  // Keep a list of rewrite locations for both left and right-hand side
  // matching expressions
  vector<REWRITE_NODE> rewriteNodes;
  int rewriteOccurrence;
};

#endif // VAMP_REWRITE_H

