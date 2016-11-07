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

#ifndef VAMP_PROCESS_H
#define VAMP_PROCESS_H

//#define NDEBUG

#ifdef USE_QT
#define VAMP_ERR_STREAM ostringstream
#else
#define VAMP_ERR_STREAM llvm::raw_ostream
#endif

#define ENDL "\n"

// reading a text file
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <bitset>
#include <iterator>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif

#include "mcdcExprTree.h"
#include "configfile.h"
#include "json.h"

// Attributes for source code
#define STMT_CODE         0x01   // Character is part of a statement
#define STMT_COVERED      0x02   // Code has coverage from combined runs
#define STMT_COVERED_OLD  0x04   // Code has coverage from old history
#define STMT_COVERED_NEW  0x08   // Code has coverage from new run
#define STMT_NEW_STMT     0x10   // Move new statement to new line
#define STMT_NEW_LINE     0x20   // Continue statement on new line
#define STMT_BR_COV_FALSE 0x40   // Branch covered with only FALSE result
#define STMT_BR_COV_TRUE  0x80   // Branch covered with only TRUE result
#define STMT_BR_COV_FULL  0xC0   // Branch covered both TRUE & FALSE results

#define BG_GREEN    0x60ff60     // Color for background text green
#define BG_LT_GREEN 0xb0ffb0     // Color for background text light green
#define BG_RED      0xff6060     // Color for background text red
#define BG_LT_RED   0xffb0b0     // Color for background text light red
#define BG_YELLOW   0xffff40     // Color for background text light yellow
#define BG_ORANGE   0xffc000     // Color for background text light orange

#define MAX_COLUMN 128           // Continue statement on new line here
#define MAX_BITSET 2048          // Max number of MC/DC conditions for expr

// Coverage options from history file
#define DO_STATEMENT_SINGLE 0x01
#define DO_STATEMENT_COUNT  0x02
#define DO_STATEMENT        (DO_STATEMENT_SINGLE | DO_STATEMENT_COUNT)
#define DO_BRANCH           0x04
#define DO_MCDC             0x08
#define DO_CONDITION        0x10

#define DIRECTORY_SEPARATOR "/"

using namespace std;

#ifdef USE_IND_PAIR_CLASS
class indexPairType {
public:
  int index;
  int usageCount;

  bool operator<(const indexPairType& rhs)
  {
    return this->usageCount > rhs.usageCount;
  }
};
#else
typedef struct {
  int index;
  int usageCount;
} indexPairType;

// Defines a sort routine for indexPairType.  Sort on highest usage count.
struct indexPairCompare {
  bool operator ()(indexPairType const& a, indexPairType const& b) const
  {
    return (a.usageCount > b.usageCount);
  }
};
#endif


typedef struct {
  int lhsLine, lhsCol, rhsLine, rhsCol;
} sourceLocationType;

typedef struct {
  string function;
  sourceLocationType loc;
} functionInfoType;

typedef struct {
  int line, col;
} instInfoType;

typedef struct {
  bool andOp;
  sourceLocationType lhsLoc;
  sourceLocationType rhsLoc;
} mcdcOpInfoType;

typedef struct {
  vector<mcdcOpInfoType> opInfo;
} mcdcExprInfoType;

typedef struct {
  sourceLocationType lhsLoc;
  sourceLocationType rhsLoc;
  string condition;
  int condNum;
} condInfoType;

typedef struct {
  string statementType;
  int index;
} branchInfoType;

typedef struct {
  sourceLocationType ifLoc;
  sourceLocationType elseLoc;
  sourceLocationType exprLoc;
  int branchNum;
} ifElseInfoType;

typedef struct {
  sourceLocationType whileLoc;
  sourceLocationType exprLoc;
  int branchNum;
} whileInfoType;

typedef struct {
  sourceLocationType forLoc;
  sourceLocationType exprLoc;
  int branchNum;
} forInfoType;

typedef struct {
  sourceLocationType caseLoc;
  int instNum;
} caseInfoType;

typedef struct {
  sourceLocationType switchLoc;
  sourceLocationType switchExprLoc;
  vector<caseInfoType> caseInfo;
} switchInfoType;


// MC/DC independent pair
typedef struct {
  unsigned int f, t;
} mcdcPair;

typedef struct {
    int ppSrcLine;
    int srcLine;
    string fileName;
} mapType;

// Class to hold Vamp history information
class History
{
public :
  History();
  ~History();
  bool loadHistory(string histName, VAMP_ERR_STREAM *vampErr);

  unsigned char coverageOptions; // Coverage options
  unsigned char *coveredInfo;    // Instrumented statements covered info
                                 // with single bit per statement
  unsigned int *covCntInfo;      // Instrumented statements covered info
                                 // with count for each statement
  unsigned char *brInfo;         // Branches covered info
  unsigned char *mcdcInfo;       // MC/DC expressions covered info
  unsigned char *condInfo;       // Conditions covered info
  unsigned short stackOverflow;  // MC/DC expression stack overflow
                                 // = Expression number causing overflow + 1
  string modTime;                // Time of creation for history file

  int instCount;                 // Number of instrumented lines in source
  int branchCount;               // Number of branches in source
  int mcdcCount;                 // Number of MC/DC expressions in source
  int condCount;                 // Number of conditions in source
};


// Class to parse JSON file and build local database
class VampDB
{
public :
  VampDB();

  void SetVampOptions(VAMP_REPORT_CONFIG &vo);
  void parseJsonDb(Json &n, VAMP_ERR_STREAM *errStr);

  // Data extracted from .json file
  vector<functionInfoType> functionInfo;
  vector<instInfoType> instInfo;
  vector<mcdcExprInfoType> mcdcExprInfo;
  vector<sourceLocationType> statementInfo;
  vector<ifElseInfoType> ifElseInfo;
  vector<whileInfoType> whileInfo;
  vector<forInfoType> forInfo;
  vector<switchInfoType> switchInfo;
  vector<branchInfoType> branchInfo;
  vector<condInfoType> condInfo;
  vector<sourceLocationType> mcdcOverflowInfo;

  vector<int> functionStmtCount; // # statements in each function
  int totalStmtCount;            // Total # statements in source code
  string fileName;               // Name of preprocessed source file
  string srcFileName;            // Name of original source file
  string pathName;               // Path to preprocessed source file
  string modTime;                // Modification time of original source file
  string instrFileName;          // Name of instrumented file
  string instrPathName;          // Path to instrumented file
  string instrModTime;           // Modification time of instrumented file
  string histDirectory;          // Directory where history files are found
  bool combineHistory;           // Combine history files
  bool outputCombinedHistory;    // Output combined history files
  bool showTestCases;            // Show recommended test cases for coverage
  string htmlDirectory;          // Save directory for html files
  string htmlSuffix;             // Suffix for html files
  bool generateReport;           // Generate report summary
  string reportSeparator;        // String seperating report fields

private:
  sourceLocationType getLoc(vector<JsonNode> &n);
  instInfoType getInstLoc(vector<JsonNode> &n);
  void parseFuncInfo(Json &json, vector<JsonNode> &n);
  void parseStatementInfo(Json &json, vector<JsonNode> &n);
  void parseInstInfo(Json &json, vector<JsonNode> &n);
  mcdcExprInfoType getMCDCExprInfo(Json &json, vector<JsonNode> &n);
  void parseMCDCExprInfo(Json &json, vector<JsonNode> &n);
  ifElseInfoType parseIfElseInfo(Json &json, vector<JsonNode> &n);
  whileInfoType parseWhileInfo(Json &json, vector<JsonNode> &n);
  forInfoType parseForInfo(Json &json, vector<JsonNode> &n);
  switchInfoType parseSwitchInfo(Json &json, vector<JsonNode> &n);
  void parseBranchInfo(Json &json, vector<JsonNode> &n);
  condInfoType parseConditionElement(Json &json, vector<JsonNode> &n);
  void parseConditionInfo(Json &json, vector<JsonNode> &n);
  void parseMcdcOverflow(Json &json, vector<JsonNode> &n);

  int functionCount;     // Number of functions in source
  int ifInfoIndex;       // Index into branchInfo for each if statement
  int whileInfoIndex;    // Index into branchInfo for each while statement
  int forInfoIndex;      // Index into branchInfo for each for statement
  int switchInfoIndex;   // Index into branchInfo for each switch statement

  VAMP_ERR_STREAM *vampErr;
};



// Class to process .json file and .hist file and create .html file
class VampProcess
{
public:
  VampProcess(VAMP_ERR_STREAM *outStr, VAMP_ERR_STREAM *errStr) : vampOut(outStr), vampErr(errStr)
  {
      gotPPMap = false;
  }
  mapType getMapInfo(vector<JsonNode> &n);
  void parseLineMarkers(Json &json, vector<JsonNode> &n);
  void parseJsonMap(Json &n);
  void processLineMarkers(char *preProcFileName);
  bool processFile(char *jsonName, VAMP_REPORT_CONFIG &vo);
  bool readSource(string fileName);
  string sourceText(sourceLocationType &loc);
  void genHtmlSourceText(sourceLocationType &loc, int indent, vector<string> &strings);
  string htmlSourceText(sourceLocationType &loc, int indent);
  void walkTreeNode(int &nodeCnt, mcdcNode *node, bool doRHS,
                    vector<sourceLocationType> &nodeInfo);
  void walkTree(int &nodeCnt, mcdcNode *node,
                vector<sourceLocationType> &nodeInfo);
  int checkMatch(string &mcdcStr,
                 vector<string> &testStr,
                 bool checkBoth = false);
  void setAttrib(unsigned char attrib, sourceLocationType &range);
  void processStmt(void);
  string htmlBgColor(int color);
  string htmlPercentageStyle(string name, int percent);
  //int getMinCond(vector<mcdcPair> mcdcPairs[32],
  int getMinCond(vector<mcdcPair> *mcdcPairs,
                 int mcdcPairIndex,
                 bitset<MAX_BITSET> &falseConditions,
                 bitset<MAX_BITSET> &trueConditions);
                 //unsigned int &falseConditions,
                 //unsigned int &trueConditions);
  int genCoverageHTML(int covered,
                      bool isBranch,
                      sourceLocationType &s,
                      ostringstream &funcHTML);
  void processBranch(void);
  void processCondition(void);
  void processMCDC(void);
  void genHTML(string htmlName);

private:
  // Source code instrumented to produce .json file
  vector<string> source;
  // Attributes associated with source
  vector<string> attribs;

  // Collected MC/DC HTML Information per function
  vector<string> mcdcHTMLInfo;
  // Collected Branch HTML Information per function
  vector<string> branchHTMLInfo;
  // Collected Condition HTML Information per function
  vector<string> condHTMLInfo;

  // Coverage options
  bool doStmtSingle;
  bool doStmtCount;
  bool doBranch;
  bool doMCDC;
  bool doCC;

  // MCDC expression tree
  mcdcExprTree exprTree;
  vector<int> mcdcByteCnt;   // Number of bytes used (1 or 2) to store
                                  // a MCDC result
  vector<int> mcdcOpCnt;     // Number of operands in expression * number
                                  // of bytes used

  int totalMcdcCombCnt;

  int sourceCount;           // Number of lines in source

  vector<int> functionStmtCoverageCount; // # covered stmts in each function
  int totalStmtCoveredCount;                  // Total # stmts covered in source
  int newStmtCoveredCount;                    // # new stmts covered in current run

  vector<int> mcdcFunctionOperandCount;  // # MCDC operands in each function
  vector<int> mcdcFunctionCoverageCount; // # operands covered in each func
  int mcdcTotalOperandCount;                  // # MCDC operands in source code
  int mcdcTotalCoverageCount;                 // # MCDC operands covered in source
  int mcdcNewCoverCount;                      // # new MCDC ops covered in current run

  vector<int> branchFunctionOperandCount;  // # Branch operands in each function
  vector<int> branchFunctionCoverageCount; // # operands covered in each func
  int branchTotalOperandCount;                  // # Branch operands in source code
  int branchTotalCoverageCount;                 // # Branch operands covered in source
  int newBranchCoveredCount;                    // # new branches covered in current run
  vector<int> condFunctionOperandCount;    // # conditions in each function
  vector<int> condFunctionCoverageCount;   // # conditions covered in each func
  int condTotalOperandCount;                    // # conditions in source code
  int condTotalCoverageCount;                   // # conditions covered in source
  int newCondCoveredCount;                      // # new conditions covered in current run

  VAMP_ERR_STREAM *vampOut;
  VAMP_ERR_STREAM *vampErr;

  bool gotPPMap;
  string mapFileName;
  string mapPPFileName;
  vector<mapType> mapInfo;

  VampDB db;
  History oldHist;   // Old (combined previous) history info
  History newHist;   // Latest run of history info
  History hist;      // Combination of oldHist and newHist if combineHistory set
                     // and .cmbhist file found. Otherwise just latest run
};

#endif

