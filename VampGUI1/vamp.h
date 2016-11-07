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

#ifndef VAMP_H
#define VAMP_H

#ifdef USE_QT
#define VAMP_ERR_STREAM ostringstream
#else
#define VAMP_ERR_STREAM llvm::raw_ostream
#endif

#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <exception>

#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"

#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
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
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Frontend/Utils.h"
#include "json.h"
#include "configfile.h"
#include "mcdcExprTree.h"
#include "vamp_rewrite.h"

/*
#ifdef _WIN32
#define DIRECTORY_SEPARATOR "\\"
#else
#define DIRECTORY_SEPARATOR "/"
#endif
*/
#define DIRECTORY_SEPARATOR "/"

#define ENDL "\n"


#ifdef _WIN32
#define realpath(N, R) _fullpath(R, N, 65536)
#endif

#define DO_STATEMENT_SINGLE 0x01
#define DO_STATEMENT_COUNT  0x02
#define DO_STATEMENT        (DO_STATEMENT_SINGLE | DO_STATEMENT_COUNT)
#define DO_BRANCH           0x04
#define DO_MCDC             0x08
#define DO_CONDITION        0x10

// Default value for mcdcStackSize
// REQ# CONF019
#define MCDC_STACK_SIZE     4


using namespace clang;
using namespace std;

// Defines structure describing start and end of a statement.
// These are placed in a VAMP_VECTOR called stmtVec.
typedef struct {
  int startLine;
  int startCol;
  int endLine;
  int endCol;
} STMT_LOC;

// Defines a sort routine for STMT_LOC.  Else clauses inside stmtVec are
// sadly placed before their associated if and then statement info.
struct stmtLocCompare {
  bool operator ()(STMT_LOC const& a, STMT_LOC const& b) const
  {
    if ((a.startLine < b.startLine) ||
        ((a.startLine == b.startLine) && (a.startCol < b.startCol)))
      return true;
    return false;
  }
};

typedef struct {
  int line;
  int col;
  int atLine;
  int atCol;
  string prefix;
  string suffix;
  bool inCase;
  SourceLocation Loc;
} INST_DATA;

// Defines a sort routine for INST_DATA.
struct instLocCompare {
  bool operator ()(INST_DATA const& a, INST_DATA const& b) const
  {
#ifdef SORT_BY_LINE_COL
    if ((a.line < b.line) ||
        ((a.line == b.line) && (a.col < b.col)))
      return true;
    return false;
#else
    return a.Loc.getRawEncoding() < b.Loc.getRawEncoding();
#endif
  }
};

// To instrument a line following a certain SourceLocation, the following
// structure is used.
// AfterLoc means place the statement instrumentation just before the
// first statement encountered following AfterLoc.
// If checkBetween is set to true, if the first statement encountered is
// after BeforeLoc, place the statement instrumentation at AfterLoc.
// This handles the case where statement instrumentation is to be placed
// within an empty compound statement - e.g.:
// if (cond)
// {
//   // Do nothing
// }
// makes sure the instrumentation is placed within the braces.
// MarkLoc is the SourceLocation from which the source code is considered
// to have begun.
typedef struct {
  SourceLocation AfterLoc;
  bool checkBetween;
  SourceLocation BeforeLoc;
  SourceLocation MarkLoc;
  string prefix;
} INST_BETWEEN;


enum StmtType { STMT_FOR, STMT_WHILE, STMT_DO, STMT_IF, STMT_SWITCH };

typedef struct {
  int cnt;
  int stLine, stCol;
  int endLine, endCol;
  int stLineExpr, stColExpr;
  int endLineExpr, endColExpr;
  int line;
  int col;
  ostringstream *oss;
} SWITCH_INFO;

typedef struct {
  unsigned locEnd;
  StmtType type;
} BLOCK_LEVEL;

class Vamp
{
public :
  Vamp(VAMP_ERR_STREAM *outStr, VAMP_ERR_STREAM *errStr) : vampOut(outStr), vampErr(errStr)
  {
  }
  bool Instrument(int argc, char *argv[], string outName, VAMP_CONFIG &vampOptions);

private:
  VAMP_ERR_STREAM *vampOut;
  VAMP_ERR_STREAM *vampErr;
};

// RecursiveASTVisitor is is the big-kahuna visitor that traverses
// everything in the AST.
class MyRecursiveASTVisitor
    : public RecursiveASTVisitor<MyRecursiveASTVisitor>
{
 public:
  MyRecursiveASTVisitor(Rewriter &R, VAMP_ERR_STREAM *outStr, VAMP_ERR_STREAM *errStr)
   : Rewrite(R), vampOut(outStr), vampErr(errStr)
  {
    recoverOnError = true;  // Attempt to recover on instrumentation error

    mcdcStackSize = MCDC_STACK_SIZE; // Init max size of MC/DC stack
    funcCnt = 0;        // Init count of functions found
    instCnt = 0;        // Init count of instrumentation statements inserted
    cmpCnt = 0;         // Init count of instrumentation logical comps inserted
    branchCnt = 0;      // Init count of instrumentation branches inserted
    condCnt = 0;        // Init count of comparison instrumentation inserted
    stmtCnt = 0;        // Init count of statements in source code
    logicalOpCnt = 0;   // Init count of && and || in MCDC expression
    logicalOpLine = 0;
    logicalOpCol = 0;
    mcdcCnt = 0;        // Init count of MCDC expressions
    gotBranchInfo = false;
    inMain = false;
    firstStmt = false;
    inStmt = false;
    inFunction = false;
    inFuncDecl = false;
    inConditional = false;
    inDoWhile = false;
    declWithInit = false; // Function has declaration with initializer
    addFuncInst = false;
    stmtEndLine = 0;
    stmtEndCol = 0;

    inDecl = false;
    inLogicalOp = false;
    gotCase = false;

    addStmt = false;
    inIfStmt = false;
    lastBreak = false;

    mcdcMultiByte = 1; // Assume only 1 byte needed for MCDC expression

    labelNum = 1;
    varNum = 1;

    // Set defaults
    // REQ# CONF012
    doStmtSingle = true;
    // REQ# CONF013
    doStmtCount = false;
    // REQ# CONF014
    doBranch = true;
    // REQ# CONF015
    doMCDC = true;
    // REQ# CONF016
    doCC = false;
    // REQ# CONF017
    saveDirectory = "VAMP_INST";
    // REQ# CONF018
    saveSuffix = "";
    // REQ# CONF019
    mcdcStackSize = MCDC_STACK_SIZE;
    // REQ# CONF020
    langStandard = clang::LangStandard::lang_c89;

    forceStmtInst = false;
  }

  void SetVampOptions(VAMP_CONFIG &vo);
  void stripPath(string fileName, string &returnPath, string &returnName);
  bool PrepareResults(string &dirName, string outName, const char *f);
  bool ProcessResults(string &dirName, time_t &modTime);

  void CheckMCDCExprEnd();
  Expr *VisitBinaryOperator(BinaryOperator *op);
  bool VisitStmt(Stmt *s);
  bool VisitFunctionDecl(FunctionDecl *f);
  void processVampRewrites() { vampRewriter.ProcessRewriteNodes(Rewrite); }
  void FunctionEnd(SourceLocation Loc);

private:
  void AddStmtInfo(int startLine, int startCol, int endLine, int endCol);
  SourceLocation GetLocAfterToken(SourceLocation loc, tok::TokenKind token);
  SourceLocation GetLocAfter(SourceLocation loc);
  bool GetTokPos(SourceLocation st, int *line, int *col);
  bool GetTokStartPos(Stmt *s, int *line, int *col);
  bool GetTokEndPos(Stmt *s, int *line, int *col);

  bool CheckLoc(SourceLocation Loc);
  bool AddInstrumentStmt(SourceLocation Loc, SourceLocation At, string prefix, string suffix);
  SourceLocation GetLocForNextToken(SourceLocation loc, string &tk, int offset = 0);
  SourceLocation GetTokenAfterStmt(Stmt *s, string &tk);
  SourceLocation GetLocForEndOfStmt(Stmt *s);
  void AddInstAfterLoc(SourceLocation Loc, SourceLocation MarkLoc, string prefix);
  void AddInstBetweenLocs(SourceLocation AfterLoc, SourceLocation BeforeLoc, SourceLocation MarkLoc, string prefix);
  void CheckStmtInst(SourceLocation Loc, bool withinFunc);
  void AddInstEndOfStatement(Stmt *s);
  void InstrumentStmt(Stmt *s);
  void InstrumentWhile(StmtType type, Stmt *s, Stmt *body, Expr *expr);
  void NewBlock(StmtType type, SourceLocation loc);
  void InstTree(int *nodeCnt, mcdcNode *node);
  void InstTreeNode(int *nodeCnt, mcdcNode *node, bool doRHS);

  bool recoverOnError;
  llvm::raw_fd_ostream *outFile;
  llvm::raw_fd_ostream *infoFile;
  llvm::raw_fd_ostream *vinfFile;
  ostringstream branchInfo;
  ostringstream cmpInfo;
  ostringstream mcdcInfo;
  ostringstream funcInfo;
  string srcFileName;      // Name of input file
  string fileName;         // Name of input file minus extension
  string indexName;        // Upper case copy of fileName in form "_VAMP_fileName_INDEX"
  string extension;        // File extension, normally ".c"
  string instrFileName;    // Name of instrumented output file
  string instrPathName;    // Path to instrumented output file
  string instName;
  string stmtName;
  string branchName;
  string condName;
  string branchesName;
  string condsName;
  string mcdcName;
  string mcdcValName;
  string mcdcSetFirstBitName;
  string mcdcSetBitName;
  string mcdcStackName;
  string mcdcStackOffsetName;
  string mcdcStackOverflowName;
  string mcdcValSaveName;
  string mcdcValOffsetName;
  string varName;
  string infoName;
  int funcCnt;            // Number of functions found
  int instCnt;            // Number of statements instrumented
  int cmpCnt;             // Number of logical operators instrumented
  int branchCnt;          // Number of branches instrumented
  int condCnt;            // Number of conditionals instrumented
  int stmtCnt;            // Number of statements in source
  bool gotBranchInfo;
  bool inMain;
  SourceLocation stmtStartLoc;   // SourceLocation of start of current statement
  SourceLocation stmtEndLoc;     // SourceLocation of end of current statement
  ////SourceLocation stmtPastEndLoc; // SourceLocation past end of current statement
  int stmtStartLine;             // Starting line of current statement
  int stmtStartCol;              // Ending line of current statement
  bool gotCase;                  // Found case or default in switch statement
  string labelName;
  string funcName;               // Name of current function
  int labelNum;           // Vamp-generated label number
  int varNum;             // Vamp-generated variable count
  bool firstStmt;
  bool inStmt;
  bool addFuncInst;
  int stmtEndLine;
  int stmtEndCol;
  bool inFunction;        // Inside a function
  bool inFuncDecl;        // Inside a function declaration
  unsigned funcStartPos;  // Starting SourceLocation of current function
  bool inConditional;     // Inside an if/while/for/switch conditional
  bool inDoWhile;         // Inside the "do" of a an do-while statement
/*
  vector<int> doStartLine;// Start of do-while expression
  vector<int> doStartCol;
  vector<int> doEndLine;  // End of do-while expression
  vector<int> doEndCol;
*/
  vector<SourceLocation> doStartPos;// Start of do-while expression
  vector<SourceLocation> doEndPos;  // End of do-while expression
  SourceLocation condEndPos;        // End of if/while/for/switch conditional
  bool inDecl;            // Within declaration part of function
  bool declWithInit;      // Function has declaration with initializer
  bool inLogicalOp;
  int  logicalOpCnt;
  int  logicalOpLine;
  int  logicalOpCol;
  int  mcdcCnt;
  bool lastBreak;         // Last statement was a break statement

  // List of SourceLocations that had statement instrumentation added
  vector<SourceLocation> instList;
  // List of SourceLocations that will have statement instrumetation added
  // at the first statement encountered after instAfterLoc
#ifdef INST_FUNKY
  vector<SourceLocation> instAfterLoc;
#else
  vector<INST_BETWEEN> instAfterLoc;
#endif
  // instAfterLoc[i] means place the statement instrumentation just before the
  // first statement encountered following AfterLoc.
  // If checkBetween[i] is set to true, if the first statement encountered is
  // after instBeforeLoc[i], place the statement instrumentation at instAfterLoc.
  // This handles the case where statement instrumentation is to be placed
  // within an empty compound statement - e.g.:
  // if (cond)
  // {
  //   // Do nothing
  // }
  // makes sure the instrumentation is placed within the braces.
#ifdef INST_FUNKY
  vector<bool> checkBetween;
  vector<SourceLocation> instBeforeLoc;
#endif

  vector<INST_DATA> instData;
  bool addStmt;
  bool inIfStmt;
  string ifRepl;
  // SourceLocations for start end end of an if expression
  SourceLocation ifStartLoc;
  SourceLocation ifEndLoc;

  bool doStmtSingle;      // Perform individual Statement Coverage
  bool doStmtCount;       // Perform Statement Coverage counting each occurrence
  bool doBranch;          // Perform Branch Coverage
  bool doCC;              // Perform Condition Coverage
  bool doMCDC;            // Perform Modified Condition Decision Coverage
  bool forceStmtInst;     // Force statement coverage (for case statements)
  string saveDirectory;   // Save directory for vamp-generated files
  string saveSuffix;      // Suffix for vamp-generated files
  int mcdcStackSize;      // Size of MC/DC stack
  clang::LangStandard::Kind langStandard; // Selected language standard
  int mcdcMultiByte;      // Number of bytes used to handle MCDC
                          // expression:
                          // 1 for <= 7 operands
                          // 2 for > 7 && <= 15 operands
                          // 4 for > 15 && <= 31 operands
#ifdef NEED_BYTE_COUNT
  ostringstream mcdcByteStr;
#endif
  vector<int> mcdcOpCnt;
  vector<string> falseStr;
  vector<string> trueStr;

  // Statements with more than 31 MCDC operators
  // are not instrumented (error condition)
  vector<STMT_LOC> mcdcOverflow;

  vector<STMT_LOC> stmtVec;

  mcdcExprTree exprTree;
  int exprTreeLeafCnt;

  vampRewrite vampRewriter;
  Rewriter &Rewrite;

  string outFileName;
  string infoFileName;
  string vinfFileName;

  VAMP_ERR_STREAM *vampOut;
  VAMP_ERR_STREAM *vampErr;

  vector<SWITCH_INFO> switchInfo;
  vector<BLOCK_LEVEL> blockLevel;  // End SourceLocation for current block
  bool nextCase;                   // Next statement is case preceded by
                                   // a flow control change like break;
};

class MyASTConsumer : public ASTConsumer
{
 public:
  MyASTConsumer(Rewriter &Rewrite, VAMP_ERR_STREAM *outStr, VAMP_ERR_STREAM *errStr) : rv(Rewrite, outStr, errStr)
  {
  }

  virtual bool HandleTopLevelDecl(DeclGroupRef d);

  MyRecursiveASTVisitor rv;
};

#endif // VAMP_H
