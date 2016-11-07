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

#ifndef MCDCEXPRTREE_H
#define MCDCEXPRTREE_H
#include <iostream>

#ifndef NULL
#define NULL 0
#endif

/*
 *#ifdef __VAMP_INCLUDE_BINARY_OPERATOR
typedef clang::BinaryOperator binOp;
#define ERROR_OUT llvm::errs()
#else
typedef int binOp;
#define ERROR_OUT std::cout
#endif
*/
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/Lexer.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Parse/ParseAST.h"

typedef clang::BinaryOperator binOp;
#define ERROR_OUT std::cout

struct mcdcNode
{
  int type;                  // 1 = AND, 0 = OR
  int num;                   // Node number
  binOp *E;                  // For access to source code ranges
                             // for use with rewriter
  int lhsMinLine, lhsMinCol; // Left-hand-side source line/column
  int lhsMaxLine, lhsMaxCol;
  int rhsMinLine, rhsMinCol; // Right-hand-side source line/column
  int rhsMaxLine, rhsMaxCol;
  mcdcNode *lhs;             // Pointer to left-hand-side node
  mcdcNode *rhs;             // Pointer to right-hand-side node
};

class mcdcExprTree
{
public:
    mcdcExprTree();
    ~mcdcExprTree();

    mcdcNode *root;

    bool insertLeaf(int type, int num,
                    binOp *E,
                    int lhsMinLine, int lhsMinCol,
                    int lhsMaxLine, int lhsMaxCol,
                    int rhsMinLine, int rhsMinCol,
                    int rhsMaxLine, int rhsMaxCol);
    void destroyTree();
    int nodeCount(mcdcNode *node);
    void getFalse(mcdcNode *node, std::vector<std::string> *str);
    void getTrue(mcdcNode *node, std::vector<std::string> *str);

private:
    bool insertLeaf(mcdcNode **leaf, int type, int num,
                    binOp *E,
                    int lhsMinLine, int lhsMinCol,
                    int lhsMaxLine, int lhsMaxCol,
                    int rhsMinLine, int rhsMinCol,
                    int rhsMaxLine, int rhsMaxCol);
    void destroyTree(mcdcNode *leaf);
    bool isWithin(int valLine, int valCol,
                  int rangeMinLine, int rangeMinCol,
                  int rangeMaxLine, int rangeMaxCol);
    void mergeStr(std::vector<std::string> *leftStr,
                  std::vector<std::string> *rightStr,
                  std::vector<std::string> *result);
};

#endif // MCDCEXPRTREE_H

