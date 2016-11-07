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

//#define VAMP_SAFETY
//#define VAMP_DEBUG_STMT
//#define VAMP_DEBUG_SWITCH
#define CDBG cout

#define LHS_AFTER true
#define RHS_AFTER true

/***   vamp.cpp   ******************************************************
 * Copyright 2012-2014 by Robert Ankeney
 * All rights reserved
 *
 * PURPOSE:
 * Instruments C code for statement and branch coverage.
 *
 * Also creates JSON database file for later processing.
 * For file <file>.c, generates instrumented file <file>_vamp.c
 * and info file <file>_vamp.json.
 * Format of info file is in JSON database format as follows:
 *
 * {
 *   "filename": "<file>.c",     // Name of original source file
 *   "src_filename": "<file>.c", // Name of source file to be instrumented
 *   "pathname": "<path>",       // Directory where source file is found
 *   "modtime": "<time>",        // Date/Time of creation of source file
 *   "instr_filename": "<time>", // Name of instrumented source
 *   "instr_pathname": "<path>", // Directory where instrumented source is stored
 *   "instr_modtime": "<time>",  // Date/Time of creation of instrumented source
 *   "function_info":
 *   [
 *     // Function name for each C function, start/end of function
 *     "<func1>", [<l1>,<c1>,<l2>,<c2>],
 *             ...
 *     "<funcn>", [<l1>,<c1>,<l2>,<c2>]
 *   ],
 *   "statement_info":
 *   [
 *     // Start/end of each C statement
 *     // First statement
 *     [<l1>,<c1>,<l2>,<c2>],
 *             ...
 *     // Nth statement
 *     [<l1>,<c1>,<l2>,<c2>]
 *   ],
 *   "instr_info":
 *   [
 *     // Location for each instrumentation statement added, start/end of function
 *     [<l1>,<c1>],
 *         ...
 *     [<ln>,<cn>]
 *   ],
 *   "branch_info":
 *   [
 *     // Type of and start/end for each C branch and start/end for expression
 *     "if", [ [<if l1>,<if c1>,<if l2>,<if c2>], [<el l1>,<el c1>,<el l2>,<el c2>], [<ex l1>,<ex c1>,<ex l2>,<ex c2>], <br 1>],
 *     "while", [ [<l1>,<c1>,<l2>,<c2>], [<ex l1>,<ex c1>,<ex l2>,<ex c2>], <br 1>],
 *     "switch", [ [<sw l1>,<sw c1>,<sw l2>,<sw c2>], [<ex l1>,<ex c1>,<ex l2>,<ex c2>], <case cnt>, \
 *                 [<case l1>,<case c1>,<case l2>,<case c2>], \
 *                                    ...
 *                 [<case ln>,<case cn>,<case ln>,<case cn>] ],
 *   // Note case handles both case and default statements
 *   "mcdc_expr_info":
 *   [
 *     [ n, // Number of operands in first MCDC expression
 *      // Note LHS = left-hand expression, RHS = right-hand expression
 *      [<LHS l1>,<LHS c1>,<LHS l2>,<LHS c2>], \
 *      [<RHS l1>,<RHS c1>,<RHS l2>,<RHS c2>], "<op>",
 *                       ...
 *      [<LHS ln>,<LHS cn>,<LHS ln>,<LHS cn>], \
 *      [<RHS ln>,<RHS cn>,<RHS ln>,<RHS cn>], "<op>",
 *     ],
 *                       ...
 *     [ n, // Number of operands in nth MCDC expression
 *      [<LHS l1>,<LHS c1>,<LHS l2>,<LHS c2>], \
 *      [<RHS l1>,<RHS c1>,<RHS l2>,<RHS c2>], "<op>",
 *                       ...
 *      [<LHS ln>,<LHS cn>,<LHS ln>,<LHS cn>], \
 *      [<RHS ln>,<RHS cn>,<RHS ln>,<RHS cn>], "<op>",
 *     ]
 *   ],
 *   "condition_info":
 *   [
 *      // Note LHS = left-hand expression, RHS = right-hand expression
 *     [ [<LHS l1>,<LHS c1>,<LHS l2>,<LHS c2>], \
 *       [<RHS l1>,<RHS c1>,<RHS l2>,<RHS c2>], "<op>", <num> ],
 *                       ...
 *     [ [<LHS ln>,<LHS cn>,<LHS ln>,<LHS cn>], \
 *       [<RHS ln>,<RHS cn>,<RHS ln>,<RHS cn>], "<op>", <num> ]
 *   ],
 *   "mcdc_overflow":
 *   [
 *     // First MCDC overflow expression starting and ending line/column
 *     [<l1>,<c1>,<l2>,<c2>],
 *             ...
 *     // Nth MCDC overflow expression starting and ending line/column
 *     [<l1>,<c1>,<l2>,<c2>]
 *   ]
 * }
 * Note the quad <ln>,<cn>,<l(n+1)>,<c(n+1)> denotes:
 * <ln>     Starting line number
 * <cn>     Starting column number
 * <l(n+1)> Ending line number
 * <c(n+1)> Ending column number
 *
 * For each group of statements.
 *
 * NOTES:
 * When instrumenting, statements of the type:
 *   if (expr)
 *      xxx;
 *   else
 *      yyy;
 *
 * are converted to:
 *   if (_vamp_<file>_cond(expr, n))
 *   {
 *   _vamp_<file>_stmt(n);
 *      xxx;
 *   }
 *   else
 *   {
 *   _vamp_<file>_stmt(n+1);
 *      yyy;
 *   }
 *   _vamp_<file>_stmt(n+2);
 *
 * where _vamp_<file>_stmt(n) is for statement instrumentation and
 * _vamp_<file>_cond(expr, n) is for branch instrumentation.
 *
 * And similarly for while and for statements.

 * MC/DC expressions of the form:
 *     (expr1 && expr2) || expr3
 * are rewritten as:
 *     _vamp_<file>_mcdc((_vamp_<file>_mcdc_set_first(expr1, 0) &&
 *                         _vamp_<file>_mcdc_set(expr2, 1)) ||
 *                        (_vamp_<file>_mcdc_set(expr3, 2)), n, 1)
 *
 * where _vamp_<file>_mcdc() collects the result,
 *       _vamp_<file>_mcdc_set_first() initializes MC/DC collection and
 *       _vamp_<file>_mcdc_set() collects any additional results.
 *       n = MC/DC expression number.
 *
 * Usage:
 * vamp <file>.c
 *
 *****************************************************************************/

#include "vamp.h"
#include "vamp_ostream.h"
#include "path.h"

void MyRecursiveASTVisitor::SetVampOptions(VAMP_CONFIG &vo)
{
  doStmtSingle = vo.doStmtSingle;
  doStmtCount = vo.doStmtCount;
  doBranch = vo.doBranch;
  doMCDC = vo.doMCDC;
  doCC = vo.doCC;
  saveDirectory = vo.saveDirectory;
  saveSuffix = vo.saveSuffix;
  mcdcStackSize = vo.mcdcStackSize;
  langStandard = vo.langStandard;
}

void MyRecursiveASTVisitor::stripPath(string fileName, string &returnPath, string &returnName)
{
    size_t path = fileName.rfind("/");
  #ifdef _WIN32
      size_t path1 = fileName.rfind("\\");
      if ((path1 != string::npos) && (path1 > path))
          path = path1;
  #endif
    if (path == string::npos)
    {
      path = 0;
    }
    else
      ++path;

    // Strip path from filename
    returnName = fileName.substr(path);
    // Also return filename
    returnPath = fileName.substr(0, path);
}

bool MyRecursiveASTVisitor::PrepareResults(string &dirName, string outName, const char *f)
{
  fileName.assign(f);
  string srcFilePath;
  stripPath(fileName, srcFilePath, srcFileName);

/*  size_t path = outName.rfind("/");
#ifdef _WIN32
    size_t path1 = outName.rfind("\\");
    if ((path1 != string::npos) && (path1 > path))
        path = path1;
#endif
  if (path == string::npos)
  {
    path = 0;
  }
  else
    ++path;

  // Strip path from filename
  fileName = outName.substr(path);
  string savePath = outName.substr(0, path);
  */
  string savePath;
  stripPath(outName, savePath, fileName);

  size_t ext = fileName.rfind(".");
  size_t len = fileName.length();
  if (ext == string::npos)
     ext = len;
  extension = fileName.substr(ext);

  // Remove instrumentation suffix
  fileName = fileName.substr(0, ext);
  // Remove suffix
  fileName = fileName.substr(0, fileName.length() - saveSuffix.length());
//  srcFileName = fileName + extension;

  indexName = "_VAMP_";
  for (int i = 0; i < fileName.size(); ++i)
      indexName += toupper(fileName[i]);
  indexName += "_INDEX";

  // REQ# STMT006
  instName = "_vamp_" + fileName + "_inst";
  // REQ# STMT002
  stmtName = "_vamp_" + fileName + "_stmt";
  // REQ# BRCH003
  branchName = "_vamp_" + fileName + "_branch";
  // REQ# COND002
  condName = "_vamp_" + fileName + "_cond";
  // REQ# BRCH006
  branchesName =  "_vamp_" + fileName + "_branches";
  // REQ# COND005
  condsName = "_vamp_" + fileName + "_conds";
  // REQ# MCDC009
  mcdcName =  "_vamp_" + fileName + "_mcdc";
  // REQ# MCDC010
  mcdcValName = "_vamp_" + fileName + "_mcdc_val";
  // REQ# MCDC027
  mcdcSetFirstBitName = "_vamp_" + fileName + "_mcdc_set_first";
  // REQ# MCDC026
  mcdcSetBitName = "_vamp_" + fileName + "_mcdc_set";
  // REQ# MCDC011
  mcdcStackName = "_vamp_" + fileName + "_mcdc_stack";
  // REQ# MCDC015
  mcdcStackOffsetName = "_vamp_" + fileName + "_mcdc_stack_offset";
  // REQ# MCDC017
  mcdcStackOverflowName = "_vamp_" + fileName + "_mcdc_stack_overflow";
  // REQ# MCDC002
  mcdcValSaveName = "_vamp_" + fileName + "_mcdc_val_save";
  // REQ# MCDC007
  mcdcValOffsetName = "_vamp_" + fileName + "_mcdc_val_offset";
  // REQ# STMT019
  varName = "_vamp_" + fileName + "_var";

#ifdef VAMP_DEBUG
  CDBG << "File '" << fileName << "'" << ENDL;
  //CDBG << "Directory: " << dirName << ENDL;
  //CDBG << "Modified: " << modTimeAsc << ENDL;
#endif

/*
 *#ifdef _WIN32
  if ((saveDirectory[0] == '\\') ||
      (saveDirectory[0] == '/') ||
      (saveDirectory[1] == ':'))
#else
  if (saveDirectory[0] == '/')
#endif
  {
      // saveDirectory is an absoulte path -
      // add subdirectory to saveDirectory
      saveDirectory = saveDirectory + "/" + dirName;
  }
  else
  {
      // Make saveDirectory a full path from where source resides
      saveDirectory = dirName + "/" + saveDirectory;
  }
*/
  // See if directory exists
  struct stat statInfo;

  if (stat(savePath.c_str(), &statInfo) == -1)
  {
    // No - try to create it
    *vampOut << "Creating directory " << savePath.c_str() << ENDL;
/*
#ifdef _WIN32
    if (mkdir(savePath.c_str()) == -1)
#else
    if (mkdir(savePath.c_str(), 0777) == -1)
#endif
*/
    Path path;
    if (!path.makePath(savePath))
    {
      *vampErr << "Failed to create directory " << savePath.c_str() << ENDL;
      return false;
    }
  }

  ////string savePath = saveDirectory + DIRECTORY_SEPARATOR;
  outFileName = savePath + fileName + saveSuffix;
  infoFileName = outFileName;
  vinfFileName = outFileName;

  instrPathName = savePath;

  outFileName += extension;
  // REQ# DTBS002
  infoFileName += ".json";
  vinfFileName += ".vinf";
  instrFileName = fileName + saveSuffix + extension;

  return true;
}

bool MyRecursiveASTVisitor::ProcessResults(string &dirName, time_t &modTime)
{
  struct tm *modTm = gmtime(&modTime);
  char modTimeAsc[64];
  strftime(modTimeAsc, sizeof(modTimeAsc), "%Y-%m-%d %H:%M:%S", modTm);

  *vampOut << "Output to: " << outFileName << ENDL;
  // Create instrumentation output file
  std::error_code outErrorInfo;
  //llvm::raw_fd_ostream outFile(outFileName.c_str(), outErrorInfo, 0);
  outFile = new llvm::raw_fd_ostream(outFileName.c_str(), outErrorInfo, llvm::sys::fs::F_Text);

  std::error_code infoErrorInfo;
  std::error_code vinfErrorInfo;

  // REQ# DTBS001
  infoFile = new llvm::raw_fd_ostream(infoFileName.c_str(), infoErrorInfo, llvm::sys::fs::F_Text);
  vinfFile = new llvm::raw_fd_ostream(vinfFileName.c_str(), vinfErrorInfo, llvm::sys::fs::F_Text);

  if (outErrorInfo)
  {
    *vampErr << "Cannot open " << outFileName << " for writing" << ENDL;
    return EXIT_FAILURE;
  }
  else
  if (infoErrorInfo)
  {
    *vampErr << "Cannot open " << infoFileName << " for writing" << ENDL;
    return EXIT_FAILURE;
  }
  else
  if (vinfErrorInfo)
  {
    *vampErr << "Cannot open " << vinfFileName << " for writing" << ENDL;
    return EXIT_FAILURE;
  }

  // Sort statement info, as else statements are placed before
  // the if and then statements they are associated with.
  sort(stmtVec.begin(), stmtVec.end(), stmtLocCompare());

  vector<STMT_LOC>::iterator sv = stmtVec.begin();

  ostringstream instrInfo;

  if (doStmtSingle || doStmtCount || doBranch)
  {
    // REQ# DTBS012
    instrInfo << "  \"instr_info\":\n  [\n";

    // Sort instrumented statement info.
    sort(instData.begin(), instData.end(), instLocCompare());

    // Recompute instCnt
    instCnt = 0;
    int svIndex = 0;

    // Search for duplicate statement instrumentation requests
    // Look between each line of code (stmtVec) and only use the
    // last request
    vector<INST_DATA>::iterator instStmt;
    for (int instIndex = 0; instIndex < instData.size(); ++instIndex)
    {
      // Determine which index to add. Normally use last of duplicate
      // statements, unless one of the statements is a case or default stmt,
      // in which case use it and ignore remaining.
      int useIndex = 0;

      string prefix = instData[instIndex].prefix;
      string suffix = instData[instIndex].suffix;

// FIXME - Is any of this duplicate checking needed any more?
      // Skip duplicates
      while (((instIndex + 1) < instData.size()) &&
             (instData[instIndex].Loc.getRawEncoding() ==
              instData[instIndex + 1].Loc.getRawEncoding()))
      {
// FIXME - Do we need to check for and preserve earlier atLine/atCol?
// In case of duplicate where one was an else or case statement?
        if ((instData[instIndex].atLine > instData[instIndex + 1].atLine) ||
            ((instData[instIndex].atLine == instData[instIndex + 1].atLine) &&
             (instData[instIndex].atCol > instData[instIndex + 1].atCol)))
        {
#ifdef VAMP_DEBUG_STMT
CDBG << "Preserving duplicate atLine/AtCol, was " <<
instData[instIndex].atLine << ", " << instData[instIndex].atCol <<
"; now is" << 
instData[instIndex + 1].atLine << ", " << instData[instIndex + 1].atCol << ENDL;
#endif

          instData[instIndex].atLine = instData[instIndex + 1].atLine;
          instData[instIndex].atCol = instData[instIndex + 1].atCol;
        }

        // Since there's a chance that the prefix or suffix of
        // the line being skipped could have something other than NULL or EOL
        // in it, it should be copied forward.
        if ((prefix == "") || (instData[instIndex + 1].prefix != "\n"))
        {
          prefix += instData[instIndex + 1].prefix;
        }

        if ((suffix == "") || (instData[instIndex + 1].suffix != "\n"))
        {
          suffix += instData[instIndex + 1].suffix;
        }

        if (instData[instIndex].inCase)
        {
          // Use this line as it's a necessary case or default statement
          // which requires the instrumentation location in the JSON database
          useIndex = instIndex;
        }
#ifdef VAMP_DEBUG_STMT
        else
        {
          // Skip this duplicate line

CDBG << "Skipping inst at " << instData[instIndex].atLine << ", " << instData[instIndex].atCol << ":" << instData[instIndex].prefix << stmtName << "(" << instCnt << ");" << instData[instIndex].suffix << ENDL;
        }
#endif

        ++instIndex;

#ifdef VAMP_DEBUG_STMT
CDBG << "Now instData[" << instIndex+1 << "]=" << instData[instIndex+1].line << "," << instData[instIndex+1].col << ENDL;
#endif
      }

      if (useIndex == 0)
      {
        useIndex = instIndex;
      }

#ifdef VAMP_DEBUG_STMT
CDBG << "Adding inst at " << instData[useIndex].atLine << ", " << instData[useIndex].atCol << ENDL;
#endif
      // REQ# STMT002
      ostringstream repl;
      repl << prefix << stmtName << "(" << instCnt << ");" << suffix;

      string str = repl.str();
      vampRewriter.InsertText(instData[useIndex].Loc, str);

      if (instCnt++ > 0)
        instrInfo << ",\n";
      instrInfo << "    [" << instData[useIndex].atLine << "," <<
                              instData[useIndex].atCol << "]";
    }

    instrInfo << "\n  ]";
  }

  vampRewriter.ProcessRewriteNodes(Rewrite);

  *outFile << "// Instrumented by VAMP\n\n";

  // Output definitions for instrumentation
  // REQ# STMT005
  int myInstCnt = (instCnt + 7) / 8;
  // REQ# BRCH005
  int myBranchCnt = (branchCnt + 6) / 8;
  // REQ# COND004
  int myCondCnt = (condCnt + 6) / 8;
#ifdef OLD_VAMP_PROCESS_RESULTS
  if (doStmtCount && instCnt)
  {
    // REQ# STMT005
    *outFile << "static unsigned int " << instName << "[" <<
                  instCnt << "];\n";
  }
  else
  if (myInstCnt)
  {
    // REQ# STMT005
    // Output for doStmtSingle or doBranch (myInstCnt > 0)
    *outFile << "static unsigned char " << instName << "[" <<
                  myInstCnt << "];\n";
  }

  if (myBranchCnt)
  {
    // REQ# BRCH005
    // REQ# BRCH006
    *outFile << "static unsigned char " << branchesName << "[" << myBranchCnt << "];\n";
  }

  if (doCC)
  {
    //int myCondCnt = (condCnt + 3) / 4;
    if (myCondCnt)
    {
      // REQ# COND004
      // REQ# COND005
      *outFile << "static unsigned char " << condsName << "[" << myCondCnt << "];\n";
    }
  }

  int myMcdcCnt = 0;
  string mcdcValType;

  if (doMCDC && (mcdcOpCnt.size() > 0))
  {
    if (mcdcMultiByte == 1)
    {
      mcdcValType = "static unsigned char ";
    }
    else
    if (mcdcMultiByte == 2)
    {
      mcdcValType = "static unsigned short ";
    }
    else
    {
      mcdcValType = "static unsigned int ";
    }
    // REQ# MCDC010
    *outFile << mcdcValType << mcdcValName << " = 1;\n";
    // REQ# MCDC011
    // REQ# MCDC012
    // REQ# MCDC013
    *outFile << mcdcValType << mcdcStackName <<
                "[" << mcdcStackSize << "];\n";

    // REQ# MCDC015
    *outFile << "static unsigned char " << mcdcStackOffsetName << ";\n";
    // REQ# MCDC017
    *outFile << "static unsigned short " << mcdcStackOverflowName << ";\n";

    // Build list of offsets into MCDC save list for each expression
    ostringstream offsetList;
    for (int i = 0; i < mcdcOpCnt.size(); i++)
    {
      offsetList << myMcdcCnt << ((i != mcdcCnt - 1) ? ", " : " };\n");
      myMcdcCnt += mcdcOpCnt[i];
    }

    // REQ# MCDC002
    // REQ# MCDC003
    // REQ# MCDC004
    *outFile << "static unsigned char " << mcdcValSaveName << "[" <<
                   myMcdcCnt << "];\n";

    if (mcdcCnt > 1)
    {
      // Make array size based on maximum value of array element to save space
      // REQ# MCDC008
      if (myMcdcCnt < 256)
        *outFile << "static unsigned char ";
      else
      if (myMcdcCnt < 65536)
        *outFile << "static unsigned short ";
      else
        *outFile << "static unsigned int ";
      // REQ# MCDC006
      // REQ# MCDC007
      *outFile << mcdcValOffsetName << "[" << mcdcCnt <<
                 "] = \n               { " << offsetList.str();
    }
  }

  if (doStmtCount && instCnt)
  {
// FIXME : Handle 16-bit array (add [2])
//         Watch for overflow?
    // REQ# STMT004
    *outFile << "#define " << stmtName << "(i) ++" << instName << "[i]\n";
  }
  else
  if (myInstCnt)
  {
    // REQ# STMT003
    // Output for doStmtSingle or doBranch (myInstCnt > 0)
    *outFile << "#define " << stmtName << "(i) " << instName <<
                  "[i >> 3] |= (1 << (i & 7))\n";
  }

  if (myBranchCnt)
  {
    // REQ# BRCH004
    *outFile << "#define " << branchName << "(c, i) ((c) ? ((" <<
                 branchesName << "[i >> 3] |= (2 << (i & 7))), 1) : ((" <<
                 branchesName << "[i >> 3] |= (1 << (i & 7))), 0))\n";
  }


  if (doCC && (myCondCnt > 0))
  {
   // REQ# COND003
    *outFile << "#define " << condName << "(c, i) ((c) ? ((" <<
                 condsName << "[i >> 3] |= (2 << (i & 7))), 1) : ((" <<
                 condsName << "[i >> 3] |= (1 << (i & 7))), 0))\n";
  }

  if (doMCDC && (myMcdcCnt > 0))
  {
    // Insert MC/DC definitions
    // REQ# MCDC027
    *outFile << "#define " << mcdcSetFirstBitName << "(val, bit) " <<
                mcdcSetFirstBitName << "_func((val) != 0, bit)\n";
    // REQ# MCDC018
    // REQ# MCDC026
    *outFile << "#define " << mcdcSetBitName << "(val, bit) ((val) ? (" <<
                 mcdcValName << " |= (2 << bit)), 1 : 0)\n";
    if (mcdcMultiByte > 1)
    {
      // REQ# MCDC009
      // REQ# MCDC019
      *outFile << "#define " << mcdcName << "(result, exprNum, byteCnt) " <<
                  mcdcName << "_collect(result, exprNum, byteCnt)\n";
    }
    else
    {
      // Third argument (byteCnt) not needed
      // REQ# MCDC009
      // REQ# MCDC020
      *outFile << "#define " << mcdcName << "(result, exprNum, byteCnt) " <<
                  mcdcName << "_collect(result, exprNum)\n";
    }
  }

  *outFile << "extern void _vamp_output();\n";
  *outFile << "extern void _vamp_send(unsigned char data);\n";
  *outFile << "\n";

  if (doMCDC && (myMcdcCnt > 0))
  {
    // Insert MC/DC collection functions
    // REQ# MCDC029
    *outFile << "static unsigned char " << mcdcSetFirstBitName <<
                 "_func(unsigned char val, unsigned char bit)\n";
    *outFile << "{\n";
    //*outFile << "  if (" << mcdcValName << ")\n";
    *outFile << "  if (" << mcdcStackOffsetName << " < " <<
                mcdcStackSize << ")\n";
    *outFile << "  {\n";
    // REQ# MCDC016
    *outFile << "    " << mcdcStackName << "[" << mcdcStackOffsetName <<
                 "++] = " << mcdcValName << ";\n";
    *outFile << "    " << mcdcValName << " = 1;\n";
    *outFile << "    " << mcdcSetBitName << "(val, bit);\n";
    *outFile << "  }\n";
    *outFile << "  else\n";
    *outFile << "  {\n";
    *outFile << "    " << mcdcStackOverflowName << " = 1;\n";
    *outFile << "  }\n";
    *outFile << "  return val;\n";
    *outFile << "}\n\n";

// If mcdcMultiByte == 1, skip the byteCnt parameter.

    if (mcdcMultiByte > 1)
    {
      // REQ# MCDC019
      // REQ# MCDC021
      *outFile << "static unsigned char " << mcdcName << "_collect" <<
                  "(unsigned char result, unsigned short exprNum, unsigned char byteCnt)\n";
      *outFile << "{\n";
      *outFile << "  " << mcdcValType << "testval;\n";
      *outFile << "  int i, j;\n";
      // REQ# MCDC022
      *outFile << "  if (" << mcdcStackOverflowName << " == 0)\n";
      *outFile << "  {\n";
      //*outFile << "    " << mcdcValName << " |= 0x01;\n";
      if (mcdcCnt > 1)
      {
        *outFile << "    i = " << mcdcValOffsetName << "[exprNum];\n";
      }
      else
      {
        *outFile << "    i = 0;\n";
      }
      // REQ# MCDC024
      *outFile << "    do {\n";
      *outFile << "      testval = 0;\n";
// FIXME - I suspect this doesn't need to account for endianness
      *outFile << "      for (j = byteCnt - 1; j >= 0; j--)\n";
      *outFile << "      {\n";
      *outFile << "        testval = (testval << 8) | " << mcdcValSaveName <<
                  "[i + j];\n";
      *outFile << "      }\n";
      *outFile << "      i += byteCnt;\n";
      *outFile << "    } while ((testval != 0) && (testval != " <<
                  mcdcValName << "));\n";
      *outFile << "    if (testval == 0)\n";
      *outFile << "    {\n";
      *outFile << "      i -= byteCnt;\n";
#ifdef VAMP_SAFETY
      if (mcdcCnt > 1)
      {
        *outFile << "      if (i >= sizeof(" << mcdcValSaveName << ") ||\n" <<
                    "         (((exprNum + 1) < sizeof(" << mcdcValOffsetName << ") /\n" <<
                    "            sizeof(" << mcdcValOffsetName << "[0])) &&\n" <<
                    "            (i >= " << mcdcValOffsetName << "[exprNum + 1])))\n";
      }
      else
      {
        *outFile << "      if (i >= byteCnt)\n";
      }
      *outFile << "      {\n";
      *outFile << "        printf(\"MCDC expression buffer overflow\\n\");\n";
      *outFile << "      }\n";
      *outFile << "      else\n";
#endif
// FIXME - I suspect this doesn't need to account for endianness
      // REQ# MCDC025
      *outFile << "      for (j = 0; j < byteCnt; j++)\n";
      *outFile << "      {\n";
      *outFile << "        " << mcdcValSaveName << "[i++] = " <<
                  mcdcValName << " & 0xff;\n";
      *outFile << "        " << mcdcValName << " >>= 8;\n";
      *outFile << "      }\n";
      *outFile << "    }\n";
    }
    else
    {
      // REQ# MCDC020
      // REQ# MCDC022
      *outFile << "static unsigned char " << mcdcName << "_collect" <<
                  "(unsigned char result, unsigned short exprNum)\n";
      *outFile << "{\n";

      *outFile << "  int i;\n";
      //*outFile << "    " << mcdcValName << " |= 0x01;\n";
      *outFile << "  if (" << mcdcStackOverflowName << " == 0)\n";
      *outFile << "  {\n";

      // REQ# MCDC024
      *outFile << "    for (i = ";
      if (mcdcCnt > 1)
      {
        *outFile << mcdcValOffsetName << "[exprNum]";
      }
      else
      {
        *outFile << "0";
      }

      *outFile << ";\n       " <<
                  mcdcValSaveName << "[i] &&\n";
      *outFile << "         (" << mcdcValName << " != " <<
                  mcdcValSaveName << "[i]); i++)  ;\n";
#ifdef VAMP_SAFETY
/* Don't know what I was thinking here...
      *outFile << "    if (i >= sizeof(" << mcdcValSaveName << ") ||\n" <<
                  "       (((exprNum + 1) < sizeof(" << mcdcValOffsetName << ") /\n" <<
                  "          sizeof(" << mcdcValOffsetName << "[0])) &&\n" <<
                  "          (i >= " << mcdcValOffsetName << "[exprNum + 1])))\n";
*/
      *outFile << "    if (i >= " << myMcdcCnt << ")\n";
      *outFile << "    {\n";
      *outFile << "      printf(\"MCDC expression buffer overflow\\n\");\n";
      *outFile << "    }\n";
      *outFile << "    else\n  ";
#endif
      // REQ# MCDC025
      *outFile << "    " << mcdcValSaveName <<
                  "[i] = " << mcdcValName << ";\n";
    }

    // REQ# MCDC026
    *outFile << "    if (" << mcdcStackOffsetName << ")\n";
    *outFile << "    {\n";
    *outFile << "      " << mcdcValName << " = " << mcdcStackName <<
                 "[--" << mcdcStackOffsetName << "];\n";
    *outFile << "    }\n";
    *outFile << "  }\n";
    *outFile << "  else\n";
    *outFile << "  {\n";
    // REQ# MCDC023
    *outFile << "    " << mcdcStackOverflowName << " = exprNum + 1;\n";
    *outFile << "  }\n\n";
    //*outFile << "    " << mcdcValName << " = 0;\n";
    *outFile << "  return result;\n";
    *outFile << "}\n\n";
  }

  // Insert instrumentation data output routine
  unsigned char opts = (doStmtSingle ? DO_STATEMENT_SINGLE : 0) |
                       (doStmtCount ? DO_STATEMENT_COUNT : 0) |
                       (doBranch ? DO_BRANCH : 0) |
                       (doMCDC ? DO_MCDC : 0) |
                       (doCC ? DO_CONDITION : 0);
  char optsStr[8];
  sprintf(optsStr, "%#02x", opts);

  // REQ# MISC003
  *outFile << "void _vamp_" << fileName << "_output()\n";
  *outFile << "{\n";
  *outFile << "  int i;\n";
  *outFile << "  char *filename = \"" << fileName << "\";\n\n";

  // Output filename data is collected from
  *outFile << "  for (i = 0; filename[i]; i++)\n";
  *outFile << "    _vamp_send(filename[i]);\n";
  *outFile << "  _vamp_send(0);\n\n";

  // Output options
  *outFile << "  _vamp_send(" << optsStr << ");\n\n";
  // FIXME: Should " || doBranch" belong here?
  if (doStmtCount)
  {
    // Output instrumented statement data
    *outFile << "  _vamp_send(" << (instCnt >> 8) << ");\n";
    *outFile << "  _vamp_send(" << (instCnt & 0xff) << ");\n";

    if (instCnt)
    {
      if (instCnt == 1)
      {
        *outFile << "  _vamp_send(" << instName << "[0] >> 24);\n";
        *outFile << "  _vamp_send((" << instName << "[0] >> 16) & 0xff);\n";
        *outFile << "  _vamp_send((" << instName << "[0] >> 8) & 0xff);\n";
        *outFile << "  _vamp_send(" << instName << "[0] & 0xff);\n";
      }
      else
      {
        *outFile << "  for (i = 0; i < " << instCnt << "; i++)\n";
        *outFile << "  {\n";
        *outFile << "    _vamp_send(" << instName << "[i] >> 24);\n";
        *outFile << "    _vamp_send((" << instName << "[i] >> 16) & 0xff);\n";
        *outFile << "    _vamp_send((" << instName << "[i] >> 8) & 0xff);\n";
        *outFile << "    _vamp_send(" << instName << "[i] & 0xff);\n";
        *outFile << "  }\n";
      }
    }
  }
  else
  if (doStmtSingle || doBranch)
  {
    // Output instrumented statement data
    *outFile << "  _vamp_send(" << (myInstCnt >> 8) << ");\n";
    *outFile << "  _vamp_send(" << (myInstCnt & 0xff) << ");\n";

    if (myInstCnt)
    {
      if (myInstCnt == 1)
      {
        *outFile << "  _vamp_send(" << instName << "[0]);\n";
      }
      else
      {
        *outFile << "  for (i = 0; i < " << myInstCnt << "; i++)\n";
        *outFile << "    _vamp_send(" << instName << "[i]);\n";
      }
    }
  }

  if (doBranch)
  {
    // Output instrumented branch data
    *outFile << "  _vamp_send(" << (myBranchCnt >> 8) << ");\n";
    *outFile << "  _vamp_send(" << (myBranchCnt & 0xff) << ");\n";

    if (myBranchCnt)
    {
      if (myBranchCnt == 1)
      {
        *outFile << "  _vamp_send(" << branchesName << "[0]);\n";
      }
      else
      {
        *outFile << "  for (i = 0; i < " << myBranchCnt << "; i++)\n";
        *outFile << "    _vamp_send(" << branchesName << "[i]);\n";
      }
    }
  }

  if (doMCDC)
  {
    *outFile << "  _vamp_send(" << (myMcdcCnt >> 8) << ");\n";
    *outFile << "  _vamp_send(" << (myMcdcCnt & 0xff) << ");\n";

    if (myMcdcCnt)
    {
      *outFile << "  for (i = 0; i < " << myMcdcCnt << "; i++)\n";
      *outFile << "    _vamp_send(" << mcdcValSaveName << "[i]);\n";
    }

    // Output stack overflow status
    if (mcdcOpCnt.size() > 0)
    {
      // Output stack overflow info
      *outFile << "  _vamp_send((" << mcdcStackOverflowName << " >> 8) & 0xff);\n";
      *outFile << "  _vamp_send(" << mcdcStackOverflowName << " & 0xff);\n";
    }
    else
    {
      *outFile << "  _vamp_send(0);\n";
      *outFile << "  _vamp_send(0);\n";
    }
  }

  if (doCC)
  {
    // Output instrumented condition data
// FIXME - This could have endianness problems - change to / 256 and % 256?
// Check others above for similar issues.
    *outFile << "  _vamp_send(" << (myCondCnt >> 8) << ");\n";
    *outFile << "  _vamp_send(" << (myCondCnt & 0xff) << ");\n";
    if (myCondCnt)
    {
      if (myCondCnt == 1)
      {
        *outFile << "  _vamp_send(" << condsName << "[0]);\n";
      }
      else
      {
        *outFile << "  for (i = 0; i < " << myCondCnt << "; i++)\n";
        *outFile << "    _vamp_send(" << condsName << "[i]);\n";
      }
    }
  }

  *outFile << "}\n\n";
#else
  ostringstream macros;

  // FIXME - Create ostringstreams for macros and externs, so we
  // don't need to repeat the "if (doXXX)", then append them to *outFile.
  *vinfFile << "{\n";

  bool gotFirst = false;

  if (doStmtSingle || doBranch)
  {
      *vinfFile << "  \"stmt_size\": " << myInstCnt;
      gotFirst = true;

      if (myInstCnt)
      {
          // REQ# STMT003
          // Output for doStmtSingle or doBranch (myInstCnt > 0)
          macros << "#define " << stmtName << "(i) _vamp_stmt_array[_vamp_stmt_index[" << indexName << "] + (i >> 3)] |= (1 << (i & 7))\n";
      }
  }
  else
  if (doStmtCount)
  {
      if (gotFirst)
          *vinfFile << ",\n";
      else
          gotFirst = true;
      *vinfFile << "  \"stmt_size\": " << instCnt;

      if (instCnt)
      {
          // FIXME : Handle 16-bit array (add [2])
          //         Watch for overflow?
          // REQ# STMT004
          macros << "#define " << stmtName << "(i) ++_vamp_stmt_array[_vamp_stmt_index[" << indexName << "] + i]\n";
      }
  }

  if (doBranch)
  {
      if (gotFirst)
          *vinfFile << ",\n";
      else
          gotFirst = true;
      *vinfFile << "  \"branch_size\": " << myBranchCnt;

      if (myBranchCnt)
      {
        // REQ# BRCH004
        macros << "#define " << branchName << "(c, i) ((c) ? ((_vamp_branch_array[_vamp_branch_index[" <<
                  indexName << "] + (i >> 3)] |= (2 << (i & 7))), 1) : ((_vamp_branch_array[_vamp_branch_index[" <<
                  indexName << "] + (i >> 3)] |= (1 << (i & 7))), 0))\n";
      }
  }

  if (doCC)
  {
      if (gotFirst)
          *vinfFile << ",\n";
      else
          gotFirst = true;
      *vinfFile << "  \"cond_size\": " << myCondCnt;

      if (myCondCnt > 0)
      {
        // REQ# COND003
        macros << "#define " << condName << "(c, i) ((c) ? ((_vamp_cond_array[_vamp_cond_index[" <<
                  indexName << "] + (i >> 3)] |= (2 << (i & 7))), 1) : ((_vamp_cond_array[_vamp_cond_index[" <<
                  indexName << "] + (i >> 3)] |= (1 << (i & 7))), 0))\n";
      }
  }

  if (doMCDC)
  {
      if (gotFirst)
          *vinfFile << ",\n";
      else
          gotFirst = true;

      *vinfFile << "  \"mcdc_offsets\": [ ";
      int myMcdcCnt = 0;

      if (mcdcOpCnt.size() > 0)
      {
          for (int i = 0; i < mcdcOpCnt.size(); ++i)
          {
              if (i != 0)
              {
                  *vinfFile << ", ";
              }

              *vinfFile << mcdcOpCnt[i];
              myMcdcCnt += mcdcOpCnt[i];
          }
// FIXME: Consider directly calling _vamp_mcdc_set_first with val, and returning (val != 0)
        // Insert MC/DC definitions
        // REQ# MCDC027
        macros << "#define " << mcdcSetFirstBitName <<
                  "(val, bit) _vamp_mcdc_set_first(" << indexName <<
                  ", (val) != 0, bit)\n";
        // REQ# MCDC018
        // REQ# MCDC026
        macros << "#define " << mcdcSetBitName <<
                  "(val, bit) ((val) ? (_vamp_mcdc_val[" <<
                  indexName << "] |= (2 << bit)), 1 : 0)\n";
        if (mcdcMultiByte > 1)
        {
          // REQ# MCDC009
          // REQ# MCDC019
          macros << "#define " << mcdcName << "(result, exprNum, byteCnt) " <<
                    "_vamp_mcdc_collect(" << indexName <<
                    ", result, exprNum, byteCnt)\n";
        }
        else
        {
          // Third argument (byteCnt) not needed
          // REQ# MCDC009
          // REQ# MCDC020
          macros << "#define " << mcdcName << "(result, exprNum, byteCnt) " <<
                    "_vamp_mcdc_collect(" << indexName <<
                    ", result, exprNum, 1)\n";
        }
      }

      *vinfFile << " ],\n";

      *vinfFile << "  \"mcdc_size\": " << myMcdcCnt << "\n";
  }

  *vinfFile << "}\n";
  vinfFile->close();

  *outFile << "#include \"vamp_output.h\"\n\n";
  *outFile << macros.str() << "\n";
#endif

  SourceManager &sourceMgr = Rewrite.getSourceMgr();
  // Insert comment to make sure something is in rewrite buffer
  SourceLocation st = sourceMgr.getLocForStartOfFile(sourceMgr.getMainFileID());
  Rewrite.InsertText(st, "/***** End VAMP definitions *****/\n\n", true, true);

  // Now output rewritten source code
  string srcLine;
  const RewriteBuffer *RewriteBuf =
      Rewrite.getRewriteBufferFor(sourceMgr.getMainFileID());

// FIXME - if NULL, nothing rewritten, so use original source!
  if (RewriteBuf != NULL)
  {
      srcLine = string(RewriteBuf->begin(), RewriteBuf->end());
  }

  // Output corrected file
  *outFile << srcLine;
  outFile->close();

// FIXME: Get modTime of outFile and add to infoFile
  struct stat fileStat;
  string instrName = instrPathName + "/" + instrFileName;
  stat(instrName.c_str(), &fileStat);
  struct tm *instrModTm = gmtime(&fileStat.st_mtime);
  char instrModTimeAsc[64];
  strftime(instrModTimeAsc, sizeof(instrModTimeAsc),
           "%Y-%m-%d %H:%M:%S", instrModTm);

  // Output info file
  // REQ# DTBS003
  *infoFile << "{\n";

  // Output filename
  // REQ# DTBS004
  *infoFile << "  \"filename\": \"" << fileName << extension << "\",\n";
  *infoFile << "  \"src_filename\": \"" << srcFileName << "\",\n";
  // REQ# DTBS005
  *infoFile << "  \"pathname\": \"" << dirName << "\",\n";
  // REQ# DTBS006
  *infoFile << "  \"modtime\": \"" << modTimeAsc << "\",\n";
  // REQ# DTBS007
  *infoFile << "  \"instr_filename\": \"" << instrFileName << "\",\n";
  // REQ# DTBS008
  *infoFile << "  \"instr_pathname\": \"" << instrPathName << "\",\n";
  // REQ# DTBS009
  *infoFile << "  \"instr_modtime\": \"" << instrModTimeAsc << "\",\n";

  // Output function info
  // REQ# DTBS010
  *infoFile << "  \"function_info\":\n  [\n";
  *infoFile << funcInfo.str() << "\n  ],\n";

  // REQ# DTBS011
  *infoFile << "  \"statement_info\":\n  [\n";

  //vector<STMT_LOC>::iterator sv;
  for (sv = stmtVec.begin(); sv < stmtVec.end(); sv++)
  {
    if (sv != stmtVec.begin())
      *infoFile << ",\n";

      *infoFile << "    [" << sv->startLine << "," << sv->startCol << ",";
      *infoFile << sv->endLine << "," << sv->endCol << "]";
  }
  //*infoFile << stmtInfo.str() << "\n  ],\n";
  *infoFile << "\n  ]";

  if (doStmtSingle || doStmtCount || doBranch)
  {
    *infoFile << ",\n";
    // REQ# DTBS012
    *infoFile << instrInfo.str();
  }

  // Unnecessary:
  // if (doBranch)
  {
    *infoFile << ",\n";
    // REQ# DTBS013
    // REQ# DTBS014
    // REQ# DTBS015
    // REQ# DTBS016
    *infoFile << "  \"branch_info\":\n  [\n";
    *infoFile << branchInfo.str();
    *infoFile << "\n  ]";
  }

  if (doMCDC)
  {
    *infoFile << ",\n";

    // REQ# DTBS017
    // REQ# DTBS018
    *infoFile << "  \"mcdc_expr_info\":\n  [\n";
    *infoFile << cmpInfo.str();
    *infoFile << "\n  ]";
  }
  
  if (doCC)
  {
    *infoFile << ",\n";
#ifdef OUTPUT_COUNT
    *infoFile << "  \"condition_count\": " << condCnt << ",\n";
#endif
    // REQ# DTBS019
    // REQ# DTBS020
    // REQ# DTBS021
    *infoFile << "  \"condition_info\":\n  [\n";
    *infoFile << cmpInfo.str();
    *infoFile << "\n  ]";
  }

  if (mcdcOverflow.size())
  {
    // REQ# DTBS022
    *infoFile << ",\n  \"mcdc_overflow\":\n  [\n";

    for (int i = 0; i < mcdcOverflow.size(); i++)
    {
      if (i)
        *infoFile << ",\n";
      *infoFile << "   [" << mcdcOverflow[i].startLine << "," <<
                             mcdcOverflow[i].startCol  << "," <<
                             mcdcOverflow[i].endLine << "," <<
                             mcdcOverflow[i].endCol  << "]";
    }
    *infoFile << "\n  ]";
  }

  *infoFile << "\n}\n";

  *vampOut << "Statements counted: " << stmtCnt << ENDL;

  infoFile->close();

  return true;
}


void MyRecursiveASTVisitor::AddStmtInfo(int startLine, int startCol,
                                        int endLine, int endCol)
{
  // REQ# DTBS011
  STMT_LOC stmt;
  stmt.startLine = startLine;
  stmt.startCol  = startCol;
  stmt.endLine = endLine;
  stmt.endCol  = endCol;
  stmtVec.push_back(stmt);
  ++stmtCnt;
}

// Compute SourceLocation following loc and specified token
SourceLocation MyRecursiveASTVisitor::GetLocAfterToken(SourceLocation loc,
                                                       tok::TokenKind token)
{
  return Lexer::findLocationAfterToken(loc,
                                       token,
                                       Rewrite.getSourceMgr(),
                                       Rewrite.getLangOpts(),
                                       true);
}

// Compute SourceLocation following loc
SourceLocation MyRecursiveASTVisitor::GetLocAfter(SourceLocation loc)
{
  // Skip past any non-tokens (spaces, etc)
  int tokOffset;
  do {
    tokOffset = Lexer::MeasureTokenLength(loc,
                                          Rewrite.getSourceMgr(),
                                          Rewrite.getLangOpts());
    if (tokOffset == 0)
      loc = loc.getLocWithOffset(1);
  } while (tokOffset == 0);

  return loc.getLocWithOffset(tokOffset);
}

// Compute source line and column based on given SourceLocation
bool MyRecursiveASTVisitor::GetTokPos(SourceLocation st, int *line, int *col)
{
  bool INV;
  //if (CheckLoc(st))
  {
    *line = Rewrite.getSourceMgr().getPresumedLineNumber(st, &INV);
    *col =  Rewrite.getSourceMgr().getPresumedColumnNumber(st, &INV);
  }
  return INV;
}
 
// Compute starting source line and column based on given Stmt
bool MyRecursiveASTVisitor::GetTokStartPos(Stmt *s, int *line, int *col)
{
  SourceLocation st = s->getLocStart();
  return GetTokPos(st, line, col);
}

// Compute ending source line and column based on given Stmt
bool MyRecursiveASTVisitor::GetTokEndPos(Stmt *s, int *line, int *col)
{
  //SourceLocation end = GetLocAfter(s->getLocEnd());
  //return GetTokPos(end, line, col);
  SourceLocation end = s->getLocEnd();
  return GetTokPos(end, line, col);
}

// Check SourceLocation to see if it is from either an include file or
// a macro expansion, and print error if so.
// If so, either exit with failure or return false depending on
// configuration setting.
bool MyRecursiveASTVisitor::CheckLoc(SourceLocation Loc)
{
  if (Loc.isMacroID())
  {
    *vampErr << "Attempt to instrument macro at line " <<
      Rewrite.getSourceMgr().getExpansionLineNumber(Loc) << ENDL;

    // FIXME - check for return false
    if (recoverOnError)
      return false;
    else
      throw(1);
  }
  else
  if (!Rewrite.getSourceMgr().isInMainFile(Loc))
  {
    FileID fid = Rewrite.getSourceMgr().getFileID(Loc);
    SourceLocation sl = Rewrite.getSourceMgr().getIncludeLoc(fid);
    *vampErr << "Attempt to instrument statement from file included at line " <<
                 Rewrite.getSourceMgr().getSpellingLineNumber(sl) << ";\n";
    StringRef f = Rewrite.getSourceMgr().getFilename(Loc);
    *vampErr << "include file \"" << f.data() << "\", line " <<
                 Rewrite.getSourceMgr().getSpellingLineNumber(Loc) << "\n";

    // FIXME - check for return false
    if (recoverOnError)
      return false;
    else
      throw(1);
  }

  return true;
}

// AddInstrumentStmt - Add Statement instrumentation
// Loc - Source location to place statement
// At - Marks start of source where code coverage begins
// prefix - text to place before instrumentation statement
// suffix - text to place after instrumentation statement
// Normally At and Loc are the same. But to show coverage of an 'else'
// statement for example, At points to the start of 'else', and Loc points
// to the first statement within the else where the instrumentation takes place.
// Thus if the 'else' branch is taken, the 'else' will be highlighted in the
// results.
bool MyRecursiveASTVisitor::AddInstrumentStmt(SourceLocation Loc, SourceLocation At, string prefix, string suffix)
{
#ifdef VAMP_DEBUG_STMT
if (At.getRawEncoding() != Loc.getRawEncoding())
{
CDBG << "+++Diff+++";
}
int line, col;
GetTokPos(At, &line, &col);
CDBG << "AddInstrumentStmt At " << At.getRawEncoding() << " = " << line << ", " << col;
GetTokPos(Loc, &line, &col);
CDBG << ", Loc " << Loc.getRawEncoding() << " = " << line << ", " << col << ENDL;
#endif
  if (doStmtSingle || doStmtCount || forceStmtInst)
  {
    if (CheckLoc(Loc))
    {
// FIXME: remove any outstanding requests for future statement instrumentation
      // See if we've instrumented this Loc before
      int fnd;
      for (fnd = 0; (fnd < instList.size()) &&
                    (instList[fnd].getRawEncoding() != Loc.getRawEncoding()); ++fnd)
           ;
      if (fnd == instList.size())
      {
        INST_DATA instDat;
        GetTokPos(Loc, &instDat.line, &instDat.col);
        GetTokPos(At, &instDat.atLine, &instDat.atCol);
        instDat.prefix = prefix;
        instDat.suffix = suffix;
        instDat.Loc = Loc;
        instDat.inCase = forceStmtInst;  // Flag this is case or default stmt
        instData.push_back(instDat);
        instCnt++;

        // Remember we used this Loc
        instList.push_back(Loc);
      }
#ifdef VAMP_DEBUG_STMT
else
{
CDBG << "Found!\n";
}
#endif
    }
    else
      return false;
  }

  return true;
}

// GetLocForNextToken - Returns SourceLocation after desired offset.
// Default value for offset is 0.
// Starting at loc + offset, search tokens until one is found that is not a
// comment. Return token spelling and SourceLocation.
SourceLocation MyRecursiveASTVisitor::GetLocForNextToken(SourceLocation loc, string &tk, int offset)
{
  SourceManager &sm = Rewrite.getSourceMgr();
  const LangOptions &lo = Rewrite.getLangOpts();
  bool INV;
  const char *cPtr;
  Token token;
  do {
    loc = loc.getLocWithOffset(offset);
    cPtr = sm.getCharacterData(loc, &INV);
    // Skip past any whitespace (it may be part of a comment)
    // For some reason when a token is returned that is a comment,
    // any newlines following it are not returned. Nor is any whitespace
    // following that.
    while (*cPtr && isspace(*cPtr))
    {
      ++cPtr;
      loc = loc.getLocWithOffset(1);
    };

    // Get a token
    Lexer::getRawToken(loc, token, sm, lo);
    tk = Lexer::getSpelling(token, sm, lo);
    if (tk == "")
    {
      // Skip SourceLocation for whatever was found
      offset = 1;
    }
    else
    {
      offset = Lexer::MeasureTokenLength(loc, sm, lo);
    }
  } while ((token.getKind() == tok::comment) || (tk == ""));

  return loc;
}

// GetTokenAfterStmt - Returns SourceLocation for first token after
// current statement. Return token spelling and SourceLocation.
SourceLocation MyRecursiveASTVisitor::GetTokenAfterStmt(Stmt *s, string &tk)
{
  SourceLocation loc = s->getLocEnd();
  int offset = 0;
  do {
    loc = GetLocForNextToken(loc, tk, offset);
    if (tk == "")
    {
      // Skip SourceLocation for whatever was found
      offset = 1;
    }
    else
    {
      offset = Lexer::MeasureTokenLength(loc,
                                         Rewrite.getSourceMgr(),
                                         Rewrite.getLangOpts());
    }
  } while (tk != ";");

  loc = GetLocForNextToken(loc, tk, offset);
  return loc;
}

// GetLocForEndOfStmt - Returns SourceLocation after end of statement s
// (i.e. after the ';' or '}' ).
// Note that s->getLocEnd() will point directly at the '}', or at the token
// before ';'.  This is implemented in an inelegant way by examining the
// character pointed to by s->getLocEnd() to see if it's a '}', and if not,
// by finding the ';' after s->getLocEnd().
SourceLocation MyRecursiveASTVisitor::GetLocForEndOfStmt(Stmt *s)
{
  SourceLocation end;
  SourceLocation e = s->getLocEnd();
  SourceManager &sm = Rewrite.getSourceMgr();
  bool INV;
  const char *cPtr = sm.getCharacterData(e, &INV);

  if (*cPtr == ';')
  {
    // The check for ';' here is to deal with the case of an
    // empty statement (e.g. if (expr) ; )
    // GetLocAfterToken returns NULL (clang bug?)
    // Return end same as start
    end = e;
  }
  else
  if (*cPtr == '}')
  {
    end = e.getLocWithOffset(1);
  }
  else
  {
    end = GetLocAfterToken(e, tok::semi);
/*
if (!end.isValid())
// Empty statement (e.g. if (expr) ; )
end = e.getLocWithOffset(1);
*/
  }
  return end;
}

// AddInstAfterLoc - Add Statement instrumentation after specified
// SourceLocation. When the next statement occurs in the AST, instrumentation
// will be placed immediately before it.
// MarkLoc is the SourceLocation from where the source code is considered to
// have started. Normally this is the same as Loc, except when used to mark
// the start of an else clause, or a case statement.
void MyRecursiveASTVisitor::AddInstAfterLoc(SourceLocation Loc,
                                            SourceLocation MarkLoc,
                                            string prefix)
{
#ifdef VAMP_DEBUG_STMT
int line, col;
GetTokPos(Loc, &line, &col);
CDBG << "AddInstAfterLoc " << Loc.getRawEncoding() << " line,col: " <<
                              line << ", " << col << "\n";
#endif
  if (doStmtSingle || doStmtCount)
  {
    int fnd;
    for (fnd = 0; (fnd < instAfterLoc.size()) &&
                    (instAfterLoc[fnd].AfterLoc.getRawEncoding() !=
                     Loc.getRawEncoding()); ++fnd)
           ;
    if (fnd == instAfterLoc.size())
    {
      INST_BETWEEN inst;
      inst.AfterLoc = Loc;
      inst.checkBetween = false;
      inst.BeforeLoc = Loc;   // This will be ignored
      inst.MarkLoc = MarkLoc;
      inst.prefix = prefix;
      instAfterLoc.push_back(inst);
#ifdef VAMP_DEBUG_STMT
CDBG << "Added\n";
#endif
    }
#ifdef VAMP_DEBUG_STMT
else
CDBG << "Skipped\n";
#endif
  }
}

// AddInstBetweenLocs - Add Statement instrumentation after specified
// SourceLocation. When the next statement occurs in the AST, if the next
// statement is before BeforeLoc, instrumentation will be placed immediately
// before it. Otherwise instrumentation will be placed at AfterLoc.
// MarkLoc is the SourceLocation from where the source code is considered to
// have started. Normally this is the same as Loc, except when used to mark
// the start of an else clause, or a case statement.
void MyRecursiveASTVisitor::AddInstBetweenLocs(SourceLocation AfterLoc,
                                               SourceLocation BeforeLoc,
                                               SourceLocation MarkLoc,
                                               string prefix)
{
#ifdef VAMP_DEBUG_STMT
int line, col;
GetTokPos(AfterLoc, &line, &col);
CDBG << "AddInstBetweenLocs " << AfterLoc.getRawEncoding() << " line,col: " <<
  line << ", " << col << "; " << BeforeLoc.getRawEncoding() << " line,col: ";
GetTokPos(BeforeLoc, &line, &col);
CDBG << line << ", " << col << ")\n";
#endif

  if (doStmtSingle || doStmtCount)
  {
    int fnd;
    for (fnd = 0; (fnd < instAfterLoc.size()) &&
                    (instAfterLoc[fnd].AfterLoc.getRawEncoding() !=
                     AfterLoc.getRawEncoding()); ++fnd)
           ;
    if (fnd == instAfterLoc.size())
    {
      INST_BETWEEN inst;
      inst.AfterLoc = AfterLoc;
      inst.checkBetween = true;
      inst.BeforeLoc = BeforeLoc;   // This will be ignored
      inst.MarkLoc = MarkLoc;
      inst.prefix = prefix;
      instAfterLoc.push_back(inst);
#ifdef VAMP_DEBUG_STMT
CDBG << "Added\n";
#endif
    }
    else
    {
      // Found a copy - see if there's no checkBetween
      if (!instAfterLoc[fnd].checkBetween)
      {
        // There is - update it
#ifdef VAMP_DEBUG_STMT
CDBG << "Modified\n";
#endif
        instAfterLoc[fnd].checkBetween = true;
        instAfterLoc[fnd].BeforeLoc = BeforeLoc;
      }
#ifdef VAMP_DEBUG_STMT
CDBG << "Skipped\n";
#endif
    }
  }
}

// CheckStmtInst - See if any SourceLocation in vector instAfterLoc is before
// specified Loc. If so, add it to instList.
// If withinFunc is true, all statements in instAfterLoc and before Loc will be
// instrumented. If false, only those with the checkBetween flag set will be
// instrumented.
void MyRecursiveASTVisitor::CheckStmtInst(SourceLocation Loc, bool withinFunc)
{
#ifdef VAMP_DEBUG_STMT
int l, c;
GetTokPos(Loc, &l, &c);
CDBG << "CheckStmtInst(" << Loc.getRawEncoding() << ", " << withinFunc << ") at "
     << l << ", " << c << " against " << instAfterLoc.size() << " entries\n";
#endif
  vector<INST_BETWEEN>::iterator it = instAfterLoc.begin();
  while (it != instAfterLoc.end())
  {
#ifdef VAMP_DEBUG_STMT
CDBG << "CheckStmtInst compare " << it->AfterLoc.getRawEncoding() << " with " <<
        Loc.getRawEncoding() << "=" <<
        (it->AfterLoc.getRawEncoding() < Loc.getRawEncoding()) << "\n";
#endif

    bool instOK = withinFunc;

    if (it->AfterLoc.getRawEncoding() < Loc.getRawEncoding())
    {
      SourceLocation instLoc = Loc;
#ifdef VAMP_DEBUG_STMT
int line, col;
GetTokPos(instLoc, &line, &col);
CDBG << "CheckStmtInst added " << line << ", " << col << "\n";
#endif
      if (it->checkBetween && (Loc.getRawEncoding() >
                                  it->BeforeLoc.getRawEncoding()))
      {
        // Revert to instBeforeLoc
        instLoc = it->BeforeLoc;
        instOK = true;
#ifdef VAMP_DEBUG_STMT
GetTokPos(instLoc, &line, &col);
CDBG << "Reverted to " << line << ", " << col << "\n";
#endif
      }

      if (instOK)
      {
        // Add to list of statements to instrument
        // FIXME: Is (Loc, Loc, ...) correct? Or maybe (*it, Loc, ...)?
        if (it->AfterLoc == it->MarkLoc)
          // Use the instrumentation location as the marked location
          it->MarkLoc = instLoc;
        AddInstrumentStmt(instLoc, it->MarkLoc, it->prefix,  "\n");
      }

      // Remove from list
      instAfterLoc.erase(it);
    }
    else
    {
      // Haven't reached it yet - leave it in the vector
      ++it;
    }
  }
}

// AddInstEndOfStatement - Add Statement instrumentation at end of statement
// If statement is compound, add inst at start of block
// Add inst statement at end of given statement (i.e. after the ';' or '}' )
// s - Stmt to instrument
void MyRecursiveASTVisitor::AddInstEndOfStatement(Stmt *s)
{
  SourceLocation end = GetLocForEndOfStmt(s);
  AddInstAfterLoc(end, end, "");
}

// InstrumentStmt - Add Statement instrumentation to line/block of code
// If statement is compound, add inst at start of block
// Add inst statement at end of given statement (i.e. after the ';' or '}' )
// s - Stmt to instrument
void MyRecursiveASTVisitor::InstrumentStmt(Stmt *s)
{
  if (doStmtSingle || doStmtCount || forceStmtInst)
  {
    if (isa<CompoundStmt>(s))
    {
      // Add inst statement at start of block
      SourceLocation st = s->getLocStart().getLocWithOffset(1);
      AddInstBetweenLocs(st, s->getLocEnd(), st, "");
    }
    else
    {
      // REQ# STMT011
      // Add brace and inst statement at start of statement
      SourceLocation st = s->getLocStart();

      // Check for empty statement
      SourceManager &sm = Rewrite.getSourceMgr();
      bool INV;
      const char *cPtr = sm.getCharacterData(st, &INV);

      if (*cPtr == ';')
      {
        // REQ# BRCH009
        // Have empty statement ";"
        int line, col;
        GetTokPos(st, &line, &col);

        // Replace ";" by "{ <inst> }"
        // First remove ';'
        vampRewriter.ReplaceText(st, 1, "");
        AddInstrumentStmt(st, st, "{\n", "\n}");
      }
      else
      {
        SourceLocation st = s->getLocStart().getLocWithOffset(-1);
        vampRewriter.InsertText(st, "{\n");
        AddInstAfterLoc(st, st, "");
        SourceLocation loc = GetLocForEndOfStmt(s);

        // Add brace at end of statement
        vampRewriter.InsertText(loc, "\n}");
      }
    }
  }
}

// Instrument the expression of a While or Do-While or For statement
// Convert:
//   while (expr)
// to:
//   while (_vamp_xxx_cond(expr, <condition count>))
//
// type is "while" or "do" or "for"
// s is pointer to Stmt (DoStmt or WhileStmt or ForStmt)
// body is pointer to body of Do or While (i.e.  { ... } )
// expr is pointer to expression of Do or While (i.e.  (<expr>) )
void MyRecursiveASTVisitor::InstrumentWhile(StmtType type, Stmt *s, Stmt *body, Expr *expr)
{
  static string typeName[] = { "for", "while", "do" };
  SourceRange sr;
  SourceLocation locStart, locEnd;

  if (expr != NULL)
  {
    sr = expr->getSourceRange();
  }
  else
  {
    // Something without an expression like for (;;)
    sr = s->getSourceRange();
  }

  locStart = sr.getBegin();
  locEnd = GetLocAfter(sr.getEnd());
  condEndPos = locEnd;

  if (doBranch && (expr != NULL))
  {
    if (CheckLoc(locStart))
    {
      int line, col;

      // Insert range for body of Do/While/For statement
      GetTokStartPos(s, &line, &col);
      if (gotBranchInfo)
        branchInfo << ",\n";
      // REQ# DTBS013
      // REQ# DTBS014
      branchInfo << "    \"" << typeName[type] << "\", [ [" << line << "," << col;
      GetTokEndPos(s, &line, &col);
      branchInfo << "," << line << "," << col << "], ";

      // Add the range used by the expression
      GetTokPos(locStart, &line, &col);
      branchInfo << "[" << line << "," << col << ",";
#ifdef REWRITER_DEBUG
*vampOut << "BR LHS:" << locStart.getRawEncoding() << "; " << line << ", " << col << " = " << branchName << "(" << ENDL;
#endif
      GetTokPos(locEnd, &line, &col);
      branchInfo << line << "," << --col << "], " << branchCnt << "]";

//CDBG << "While bool =" << expr->isKnownToHaveBooleanValue() << ENDL;
      ostringstream repl;
      repl << ", " << branchCnt << ")";
      branchCnt += 2;
      gotBranchInfo = true;

      // REQ# BRCH001
      vampRewriter.addRewriteNode(locStart, branchName + "(", locEnd, repl.str());
    }
  }

  if (type != STMT_DO)
  {
    // REQ# STMT009
    InstrumentStmt(body);
  }

  // Add inst statement at end of while statement
  // REQ# STMT010
  AddInstEndOfStatement(s);

  // Add new block level
  NewBlock(type, s->getLocEnd());

  if (type == STMT_DO)
  {
    // Wait till we're inside condition of do statement
    // Occurs when we're at or beyond doStartPos
    inDoWhile = true;
    doStartPos.push_back(locStart);
    doEndPos.push_back(locEnd);
  }
  else
  {
    inConditional = true;
  }
}

// Add new block to vector blockLevel
// SourceLocation loc marks end of designated for/while/do/if/switch block
// When a statement is found past the ending SourceLocation, the block is
// removed from the vector
void MyRecursiveASTVisitor::NewBlock(StmtType type, SourceLocation loc)
{
  BLOCK_LEVEL lev;

  lev.locEnd = loc.getRawEncoding();
  lev.type = type;
  blockLevel.push_back(lev);
}

void MyRecursiveASTVisitor::InstTree(int *nodeCnt, mcdcNode *node)
{
  if (node->lhs == NULL)
  {
    InstTreeNode(nodeCnt, node, false);
  }
  else
  {
    InstTree(nodeCnt, node->lhs);
  }

  if (node->rhs == NULL)
  {
    InstTreeNode(nodeCnt, node, true);
  }
  else
  {
    InstTree(nodeCnt, node->rhs);
  }
}

void MyRecursiveASTVisitor::InstTreeNode(int *nodeCnt, mcdcNode *node, bool doRHS)
{
  int minLine, minCol,
      maxLine, maxCol;

  Expr *E = doRHS ? node->E->getRHS() : node->E->getLHS();

  if (CheckLoc(E->getLocStart()))
  {
    if (!doRHS && (*nodeCnt == 0))
    {
      if ((node->lhsMinLine == exprTree.root->lhsMinLine) &&
          (node->lhsMinCol  == exprTree.root->lhsMinCol))
      {
        // REQ# MCDC009
        vampRewriter.addMCDCLeftNode(exprTree.root->E->getLocStart(), 
                                     mcdcName + "(");

        vampRewriter.addMCDCLeftNode(E->getLocStart(),
                                     mcdcSetFirstBitName + "(");
      }
      else
      {
        // REQ# MCDC009
        // Insert expr collector of initial left-hand expression
        vampRewriter.addMCDCLeftNode(exprTree.root->E->getLocStart(),
                                     mcdcName + "(");

        // Insert LHS at start of left-hand expression
        vampRewriter.addMCDCLeftNode(E->getLocStart(),
                                     mcdcSetFirstBitName + "(");
      }
    }
    else
    {
      // Insert LHS at start of left-hand expression
      vampRewriter.addMCDCLeftNode(E->getLocStart(), mcdcSetBitName + "(");
    }

    // Insert count and closing paren at end of left-hand expression
    if (doRHS && (*nodeCnt == (exprTreeLeafCnt - 1)))
    {
      ostringstream endRepl;

      // If mcdcMultiByte ends up equal to 1, we will end up ignoring the
      // *nodeCnt value. It will be removed in the _vamp_<file ID>_mcdc macro.
      if ((node->rhsMaxLine == exprTree.root->lhsMaxLine) &&
          (node->rhsMaxCol  == exprTree.root->lhsMaxCol))
      {
        endRepl << ", " << *nodeCnt << ")";
      }
      else
      {
        ostringstream tmpRepl;
        tmpRepl << ", " << *nodeCnt << ")";
        SourceLocation loc = GetLocAfter(E->getLocEnd());
        vampRewriter.addMCDCRightNode(loc, tmpRepl.str());
      }

      // Complete call to _vamp_<file>_mcdc(result, mcdcCnt)
      // Where result is result of boolean expression
      // mcdcCnt is the current count of MCDC expressions encountered

#ifdef VAMP_DEBUG_MCDC
cout << "nodeCnt == " << *nodeCnt << "\n";
cout << "trueStr == \n";
for (int i = 0; i < trueStr.size(); ++i)
cout << trueStr[i] << "\n";
cout << "falseStr == \n";
for (int i = 0; i < falseStr.size(); ++i)
cout << falseStr[i] << "\n";
#endif

      // Node *nodeCnt = length of longer of trueStr[0] and falseStr[0] minus 1
      int bytes;
      if (*nodeCnt > 6)
      {
        if (*nodeCnt > 14)
        {
          mcdcMultiByte = 4;
          bytes = 4;
          mcdcOpCnt.push_back(4 * (falseStr.size() + trueStr.size()));
        }
        else
        {
          if (mcdcMultiByte < 2)
            mcdcMultiByte = 2;
          bytes = 2;
          mcdcOpCnt.push_back(2 * (falseStr.size() + trueStr.size()));
        }
      }
      else
      {
        bytes = 1;
        mcdcOpCnt.push_back(falseStr.size() + trueStr.size());
      }

      endRepl << ", " << mcdcCnt++ << ", " << bytes << ")";

      vampRewriter.addMCDCRightNode(GetLocAfter(exprTree.root->E->getLocEnd()),
                                    endRepl.str());
    }
    else
    {
      // Insert count and closing paren at end of left-hand expression
      ostringstream endRepl;
      endRepl << ", " << *nodeCnt << ")";
      vampRewriter.addMCDCRightNode(GetLocAfter(E->getLocEnd()), endRepl.str());
    }

    condCnt++;
    ++*nodeCnt;
  }
}

// Code to do cleanup at end of function. Loc is current SourceLocation.
void MyRecursiveASTVisitor::FunctionEnd(SourceLocation Loc)
{
  // Instrument any remaining statements with checkBetween flag set
  CheckStmtInst(Loc, false);

  // Remove any left-over block level information from previous functions
  blockLevel.clear();

  // Remove any left-over requests for statement instrumentation from
  // previous functions. End of function should accomodate them.
  instAfterLoc.clear();

  // Reached the end of the function
  inFunction = false;
  firstStmt = false;
}

void MyRecursiveASTVisitor::CheckMCDCExprEnd()
{
  if (inLogicalOp)
  {
    // End of MC/DC expression
    inLogicalOp = false;

    if (mcdcCnt)
      cmpInfo << ",\n";
    // REQ# DTBS017
    // REQ# DTBS018
    cmpInfo << "   [ " << cmpCnt << ",\n" << mcdcInfo.str() << "\n   ]";

    // Clear accumulated MCDC info
    mcdcInfo.str("");
    cmpCnt = 0;

    if (exprTree.root != NULL)
    {
      int nodeCnt = 0;
      exprTreeLeafCnt = exprTree.nodeCount(exprTree.root);
      if (exprTreeLeafCnt < 32)
      {
        exprTree.getFalse(exprTree.root, &falseStr);
        exprTree.getTrue(exprTree.root, &trueStr);

#ifdef VAMP_DEBUG
CDBG << "Leaf Count=" << exprTreeLeafCnt << ENDL;
CDBG << "False:" << ENDL;
for (int i = 0; i < falseStr.size(); i++)
CDBG << falseStr[i] << ENDL;
CDBG << "True:" << ENDL;
for (int i = 0; i < trueStr.size(); i++)
CDBG << trueStr[i] << ENDL;
#endif

        InstTree(&nodeCnt, exprTree.root);
      }
      else
      {
        *vampErr << "MC/DC expression too complex at line " << logicalOpLine << ENDL;
        *vampErr << exprTreeLeafCnt << " operands exceeds maximum of 32" << ENDL;
        *vampErr << "Instrumentation skipped for this expression" << ENDL;

// FIXME - Decide whether to abort or set a fail flag somewhere
        // REQ# DTBS022
        STMT_LOC stmtLoc;
        stmtLoc.startLine = exprTree.root->lhsMinLine;
        stmtLoc.startCol = exprTree.root->lhsMinCol;
        stmtLoc.endLine = exprTree.root->rhsMaxLine;
        stmtLoc.endCol = exprTree.root->rhsMaxCol;
        mcdcOverflow.push_back(stmtLoc);

#ifdef VAMP_DEBUG
mcdcNode *r = exprTree.root;
CDBG << "mcdcNode.root lhs = " << r->lhsMinLine << ", " <<
                                  r->lhsMinCol << "; " <<
                                  r->lhsMaxLine << ", " <<
                                  r->lhsMaxCol << ENDL;
CDBG << "mcdcNode.root rhs = " << r->rhsMinLine << ", " <<
                                  r->rhsMinCol << "; " <<
                                  r->rhsMaxLine << ", " <<
                                  r->rhsMaxCol << ENDL;
binOp *E = r->E;
#endif
        exprTreeLeafCnt = 0;
      }

      exprTree.destroyTree();
      trueStr.clear();
      falseStr.clear();
    }
  }
}

// Override Binary Operator expressions
Expr *MyRecursiveASTVisitor::VisitBinaryOperator(BinaryOperator *E)
{
  // Determine type of binary operator
  if (inFunction && E->isLogicalOp() && CheckLoc(E->getLHS()->getLocStart()))
  {
    if (doCC)
    {
      // REQ# COND002
      // Insert opening paren at start of left-hand expression
      string cmp = condName + "(";
      vampRewriter.InsertText(E->getLHS()->getLocStart(), cmp);

      ostringstream rRepl;
      rRepl << ", " << condCnt << ")";
      condCnt += 2;

      // Insert count and closing paren at end of right-hand expression
      //Rewrite.InsertTextAfterToken(E->getRHS()->getLocEnd(), rRepl.str());
      SourceLocation loc = GetLocAfter(E->getRHS()->getLocEnd());
      vampRewriter.InsertText(loc, rRepl.str());

      int line, col;
      // Insert position of LHS
      if (cmpCnt++ > 0)
        cmpInfo << ",\n";

      // REQ# DTBS019
      // REQ# DTBS020
      // REQ# DTBS021
      GetTokStartPos(E->getLHS(), &line, &col);
      cmpInfo << "    [ [" << line << "," << col << ",";

      GetTokEndPos(E->getLHS(), &line, &col);
      cmpInfo << line << "," << col << "], ";

      // Insert position of RHS
      GetTokStartPos(E->getRHS(), &line, &col);
      cmpInfo << "[" << line << "," << col << ",";

      GetTokEndPos(E->getRHS(), &line, &col);
      cmpInfo << line << "," << col << "], ";

      // Insert Opcode
      //cmpInfo << "\"" << E->getOpcodeStr().str() << "\" ]";
      cmpInfo << "\"" << E->getOpcodeStr().str() << "\", ";
      // Insert condition number
      cmpInfo << (condCnt - 2) << " ]";
    }
    else
    if (doMCDC)
    {
      int lhsMinLine, lhsMinCol,
          lhsMaxLine, lhsMaxCol,
          rhsMinLine, rhsMinCol,
          rhsMaxLine, rhsMaxCol;
      GetTokStartPos(E->getLHS(), &lhsMinLine, &lhsMinCol);
      GetTokEndPos(E->getLHS(), &lhsMaxLine, &lhsMaxCol);
      GetTokStartPos(E->getRHS(), &rhsMinLine, &rhsMinCol);
      GetTokEndPos(E->getRHS(), &rhsMaxLine, &rhsMaxCol);

      // Insert position of LHS
      if (cmpCnt++ > 0)
        mcdcInfo << ",\n";

      // REQ# DTBS018
      mcdcInfo << "    [" << lhsMinLine << "," << lhsMinCol << ",";
      mcdcInfo << lhsMaxLine << "," << lhsMaxCol << "], ";

      // Insert position of RHS
      mcdcInfo << "[" << rhsMinLine << "," << rhsMinCol << ",";
      mcdcInfo << rhsMaxLine << "," << rhsMaxCol << "], ";

      // Insert Opcode
      mcdcInfo << "\"" << E->getOpcodeStr().str() << "\"";

      int andOp = E->getOpcodeStr().str() == "&&";

      if (!inLogicalOp)
      {
        logicalOpCnt = 0;
        inLogicalOp = true;
        logicalOpLine = rhsMaxLine;
        logicalOpCol = rhsMaxCol;
      }
      else
      {
        logicalOpCnt++;
      }

      condCnt++;

#ifdef REWRITER_DEBUG
*vampOut << "insertLeaf, " << E->getLocStart().getRawEncoding() << ", " << GetLocAfter(E->getLocEnd()).getRawEncoding() << ":" << ENDL;
*vampOut << "LHS: " << lhsMinLine << "," << lhsMinCol << "," <<
                   lhsMaxLine << "," << lhsMaxCol << "; ";
*vampOut << "RHS: " << rhsMinLine << "," << rhsMinCol << "," <<
                   rhsMaxLine << "," << rhsMaxCol << ENDL;
#endif
      if (!exprTree.insertLeaf(andOp, logicalOpCnt, E,
                               lhsMinLine, lhsMinCol,
                               lhsMaxLine, lhsMaxCol,
                               rhsMinLine, rhsMinCol,
                               rhsMaxLine, rhsMaxCol))
      {
        *vampErr << "Expression too complex at line " << lhsMinLine <<
                    " - aborting" << ENDL;
        throw(1);
      }
    }
  }

  return E;
}

// Override Statements which includes expressions and more
bool MyRecursiveASTVisitor::VisitStmt(Stmt *s)
{
  int line, col;
//CDBG << "Class: " << s->getStmtClassName() << ENDL;
  unsigned startPos = s->getLocStart().getRawEncoding();

  GetTokStartPos(s, &line, &col);

  if (inFuncDecl && (startPos > funcStartPos))
  {
    // Reset variables for next function
    // REQ# STMT019
    firstStmt = true;
    inDecl = false;
    declWithInit = false;
    inFuncDecl = false;
  }

  if (!inFunction)
  {
    // REQ# STMT001
    // REQ# BRCH002
    // REQ# MCDC001
    // REQ# COND001
    // Nothing to do outside of a function
    return true;
  }

  while ((blockLevel.size() > 0) && (startPos > blockLevel.back().locEnd))
  {
    // Reached past end of block - remove from vector
    blockLevel.pop_back();
  }

  if (inDoWhile && (startPos >= doStartPos.back().getRawEncoding()))
  {
    // Now inside conditional of do-while, flag as such
    inConditional = true;
    // Place end of while statement into conditional end location
    // Needed in case a conditional occurred within the do-while body
    condEndPos = doEndPos.back();

    // Remove from stack
    doStartPos.pop_back();
    doEndPos.pop_back();
    inDoWhile = (doStartPos.size() > 0);
  }

  if (inConditional && (s->getLocStart().getRawEncoding() >
                              condEndPos.getRawEncoding()))
  {
    // Outside the if/while/for/switch conditional
    inConditional = false;
  }

  addStmt = false;

  if (!isa<CompoundStmt>(s))
  {
    if (inLogicalOp && ((line > logicalOpLine) || (col > logicalOpCol)))
    {
      CheckMCDCExprEnd();
    }

    // See if we reached end of current declaration
    if (inDecl)
    {
      // REQ# STMT019
      int line, col;
      GetTokEndPos(s, &line, &col);
      if ((line > stmtEndLine) ||
          ((line == stmtEndLine) && (col >= stmtEndCol)))
      {
        // Yes - flag we're done
        inDecl = false;
      }
    }

    if (!inDecl)
    {
      if (isa<DeclStmt>(s))
      {
        stmtStartLoc = s->getLocStart();
        stmtEndLoc = GetLocAfter(s->getLocEnd());
        GetTokStartPos(s, &stmtStartLine, &stmtStartCol);
        GetTokPos(GetLocForEndOfStmt(s), &stmtEndLine, &stmtEndCol);

        bool hasInit = false;
        DeclStmt *DS = dyn_cast<DeclStmt>(s);
        for (DeclStmt::const_decl_iterator I = DS->decl_begin(),
               E = DS->decl_end(); I != E; ++I)
        {
          Decl *CurDecl = *I;
          VarDecl *VD = dyn_cast<VarDecl>(CurDecl);
#ifdef VAMP_DEBUG
NamedDecl *ND = dyn_cast<NamedDecl>(VD);
CDBG << "Variable declaration " << ND->getName() <<
                " has initializer = " << VD->hasInit() << ENDL;
#endif
          // VD is NULL for typedefs
          if ((VD != NULL) && VD->hasInit())
          {
//            if (VD->>getStorageClassAsWritten() != SC_Static)
            // REQ# STMT007
            if (VD->getStorageClass() != SC_Static)
            {
              // Skip initializers that are static
              hasInit = true;
            }
#ifdef VAMP_DEBUG
            else
            {
              NamedDecl *ND = dyn_cast<NamedDecl>(VD);
              CDBG << "Variable declaration " << ND->getName().str() <<
                      " has static initializer" << ENDL;
            }
#endif
          }
        }

        // If this is the first declaration in function with an initializer,
        // and also before first statement that is not a declaration,
        // add an instrumentation statement ahead of it in the form of:
        // unsigned char _vamp_<function name> = _vamp_<filename>_stmt(n);
        // This will be used in lieu of an instrumentation statement placed
        // ahead of the first line of code (declWithInit == true).
        // REQ# STMT007
        if (hasInit)
        {
          if (firstStmt && !declWithInit)
          {
            SourceLocation st = s->getLocStart();
            /*** If we need _vamp_init...
             *** Probably need to put it at start of repl, as using
             *** 2 AddInstrumentStmt calls doesn't work
            if (inMain)
              AddInstrumentStmt(st, st, "\n_vamp_init();\n", "\n");
            */

            if (doStmtSingle || doStmtCount)
            {
              string repl;
              if (doStmtCount)
                repl = "unsigned int _vamp_" + funcName + " = ";
              else
                repl = "unsigned char _vamp_" + funcName + " = ";
              AddInstrumentStmt(st, st, repl, "\n");
            }

            declWithInit = true;
          }

          // This declaration has an initializer
          // Count initializer as a statement
          // REQ# DTBS011
          AddStmtInfo(stmtStartLine, stmtStartCol, stmtEndLine, stmtEndCol);
        }

        // Set flag to do nothing till we're past end of declaration
        // REQ# STMT019
        inDecl = true;
      }
      else
      {
        if (inStmt)
        {
          // FIXME - For an if statement, stmtEndLine/Col points to the
          // closing parenthesis of the if expression. Is this what we
          // want here for inIfStmt? Call it inIfExpr?
          // Are we past current statement?
          if ((line > stmtEndLine) ||
              ((line == stmtEndLine) && (col >= stmtEndCol)))
          {
             // Yes - flag it
             inStmt = false;

// FIXME - Does this work for nested if statements?
// It should as it goes till we enter statement past end of the if expression.
             // Did we just complete an if statement?
             if (inIfStmt)
             {
               // Yes - insert end of conditional instrumentation now
               // with ", <condition number>)"
               if (doBranch && CheckLoc(ifEndLoc))
               {
                 // REQ# BRCH001
                 // REQ# BRCH007
                 vampRewriter.addRewriteNode(ifStartLoc, branchName + "(",
                                             ifEndLoc, ifRepl);
               }

               inIfStmt = false;
             }
          }
        }

        // Are we past initial declarations and into code?
        if (firstStmt &&
            ((line > stmtEndLine) ||
             ((line == stmtEndLine) && (col >= stmtEndCol))))
        {
          // Yes - See if we need to add first statement instrumentation
          if (!declWithInit)
          {
            SourceLocation st = s->getLocStart();
/*** If we need _vamp_init...
            if (inMain)
              AddInstrumentStmt(st, st, "\n_vamp_init();\n", "\n");
            else
*/
              // REQ# STMT008
              AddInstrumentStmt(st, st, "\n", "\n");
          }

          firstStmt = false;
        }
      }
    }

    if (!inStmt && !inDecl &&
        Rewrite.getSourceMgr().isInMainFile(s->getLocStart()))
    {
      if (!inConditional)
      {
        // At start of statement - see if we need to add any
        // statement instrumentation here
        SourceLocation st = s->getLocStart();
        CheckStmtInst(st, true); 
      }

      // Save starting and ending SourceLocations for statement
      stmtStartLoc = s->getLocStart();

      stmtEndLoc = s->getLocEnd();
      // If stmtStartLoc == stmtEndLoc, we have a null statement (;)
      if (stmtEndLoc != stmtStartLoc)
        stmtEndLoc = GetLocAfter(stmtEndLoc);

      // Start of new statement - flag addStmt to add to statement info
      GetTokStartPos(s, &stmtStartLine, &stmtStartCol);

#ifdef OLD_FUNC_INST
      if (addFuncInst)
      {
        // Place statement instrumentation next line after function call
        // as there is no guarantee function will return
        AddInstrumentStmt(stmtStartLoc, stmtStartLoc, "\n", "\n");
        addFuncInst = false;
      }
#endif

#ifdef VAMP_DEBUG
CDBG << "stmtStartLine " << stmtStartLine << " is from main file: " << Rewrite.getSourceMgr().isInMainFile(s->getLocStart()) << ENDL;
SourceLocation e = s->getLocEnd();
CDBG << "Valid: " << e.isValid() << ENDL;
CDBG << "Macro: " << e.isMacroID() << ENDL;
CDBG << "File: " << e.isFileID() << ENDL;
if (e.isMacroID())
{
int l, c;
std::pair<SourceLocation, SourceLocation> sr;
sr = Rewrite.getSourceMgr().getExpansionRange(e);
GetTokPos(sr.first, &l, &c);
CDBG << "getExpansionRange First = " << l << ", " << c << ENDL;
GetTokPos(sr.second, &l, &c);
CDBG << "getExpansionRange Second = " << l << ", " << c << ENDL;
}
#endif
      // Following cures problem with return EXIT_SUCCESS;
      GetTokPos(stmtEndLoc, &stmtEndLine, &stmtEndCol);
      addStmt = true;
      inStmt = true;
    }
  }

  // Did we just pass the end of a switch statement?
  if ((switchInfo.size() > 0) && !isa<CompoundStmt>(s))
  {
    int line, col;
    SWITCH_INFO swInfo;

    swInfo = switchInfo.back();
    GetTokEndPos(s, &line, &col);
    if ((line > swInfo.line) ||
        ((line == swInfo.line) &&
         (col >= swInfo.col)))
    {
      // Yes - remove switch info
      switchInfo.pop_back();

      if (doBranch)
      {
        if (gotBranchInfo)
          branchInfo << ",\n";
        // REQ# DTBS016
        branchInfo << "    \"switch\", [ [" ;
        branchInfo << swInfo.stLine << "," <<
                      swInfo.stCol << "," <<
                      swInfo.endLine << "," <<
                      swInfo.endCol << "], [";
        branchInfo << swInfo.stLineExpr << "," <<
                      swInfo.stColExpr << "," <<
                      swInfo.endLineExpr << "," <<
                      swInfo.endColExpr << "], ";
        branchInfo << swInfo.cnt <<
                      swInfo.oss->str() << "]";

        gotBranchInfo = true;
      }

      delete swInfo.oss;
    }
  }

  // Check for the use of the ternary operator (:?)
  if (doBranch && (s->getStmtClass() == Stmt::ConditionalOperatorClass))
  {
    // REQ# BRCH008
    ConditionalOperator *CondOp = cast<ConditionalOperator>(s);
    // (cond) ? LHS : RHS
    Expr *cond = CondOp->getCond(); // Get the condition before the ?
    Expr *LHS = CondOp->getLHS();   // Get expression to left of :
    Expr *RHS = CondOp->getRHS();   // Get expression to right of :

    int line, col;

    if (gotBranchInfo)
      branchInfo << ",\n";
    SourceRange sr = LHS->getSourceRange();

    // Add the range used by the LHS expression
    GetTokPos(sr.getBegin(), &line, &col);
    branchInfo << "    \"cond\", [ [" << line << "," << col;
    GetTokPos(GetLocAfter(sr.getEnd()), &line, &col);
    branchInfo << "," << line << "," << --col << "], ";

    sr = RHS->getSourceRange();

    // Add the range used by the RHS expression
    GetTokPos(sr.getBegin(), &line, &col);
    branchInfo << "[" << line << "," << col << ",";
    GetTokPos(GetLocAfter(sr.getEnd()), &line, &col);
    branchInfo << line << "," << --col << "], ";

    // Add the range used by the expression
    sr = cond->getSourceRange();
    GetTokPos(sr.getBegin(), &line, &col);
    branchInfo << "[" << line << "," << col << ",";
#ifdef REWRITER_DEBUG
*vampOut << "BR2 LHS:" << sr.getBegin().getRawEncoding() << "; " << line << ", " << col << " = " << branchName << "(" << ENDL;
#endif
    GetTokPos(GetLocAfter(sr.getEnd()), &line, &col);
    branchInfo << line << "," << --col << "], " << branchCnt << "]";

    if (CheckLoc(sr.getBegin()))
    {
      ostringstream repl;
      repl << ", " << branchCnt << ")";
      branchCnt += 2;
      gotBranchInfo = true;
      // REQ# BRCH001
      vampRewriter.addRewriteNode(sr.getBegin(), branchName + "(",
                                  GetLocAfter(sr.getEnd()), repl.str());
    }
  }

  // FIXME: - Consider making adding _vamp_output() before exit() optional
  if (isa<CallExpr>(s))
  {
    nextCase = false;

    // Check if exit() called
    CallExpr *e = dyn_cast<CallExpr>(s);
    FunctionDecl *f = e->getDirectCallee();
    if (f)
    {
      DeclarationNameInfo d = f->getNameInfo();
      string str = d.getAsString();
//cout << str << " called, inLogicalOp = " << (int) inLogicalOp << "\n";
      if (str == "exit")
      {
        string tk;
        SourceLocation stmt = GetTokenAfterStmt(s, tk);
        nextCase = ((tk == "case") || (tk == "default")) &&
                   ((blockLevel.size() == 0) ||
                    (blockLevel.back().type == STMT_SWITCH));

#ifdef VAMP_DEBUG_SWITCH
CDBG << "exit() nextCase = " << nextCase << "; type = " << tk << endl;
#endif

        SourceLocation st = s->getLocStart();

        // Rewrite the 'e' in "exit" to guarantee _vamp_output is placed.
        // REQ# MISC001
        vampRewriter.ReplaceText(stmtStartLoc, 1, "\n_vamp_output();\ne");
      }
/*
      else
      {
        AddInstrumentStmt(stmtEndLoc, stmtEndLoc, "", "\n");
      }
*/
    }

    if (doStmtSingle || doStmtCount)
    {
//FIXME: Deal with function call within a declaration; e.g.:
// int z = foo();
// Add an initializer ahead of the statement instrumentation
// Create a counter and do int _vamp_<file>_init<cnt> = ...

////FIXME : Won't work for switch(func_call()) { ... }
/*
      ostringstream repl;
      if (inDecl)
      {
        if (doStmtCount)
          repl << "\nunsigned int ";
        else
          repl << "\nunsigned char ";
        repl << varName << instCnt << " = ";
      }
      else
      {
        repl << "\n";
      }
      string replStr = repl.str();
      // Place a statement instrumentation after any line with a function call,
      // as there is no guarantee the called function will return.
////Maybe set flag for first new statement encountered and place inst before it
////      AddInstrumentStmt(stmtPastEndLoc, stmtPastEndLoc, replStr, "\n");
*/
      GetTokEndPos(s, &line, &col);
      SourceLocation e = s->getLocEnd();
      SourceManager &sm = Rewrite.getSourceMgr();
      bool INV;
      const char *cPtr = sm.getCharacterData(e, &INV);

      // Make sure it's a simple function call as opposed to a switch(func()), etc.
      if (!inConditional && (*(cPtr + 1) == ';'))
      {
        // Add inst statement at end of function call statement
        // as there is no guarantee the called function will return.
        // REQ# STMT018
//        SourceLocation end = e.getLocWithOffset(2);

        // Only if !nextCase which is computed later!
        if (!nextCase)
        {
          ostringstream prefix;
          prefix << "\n";

          // REQ# STMT019
          if (inDecl)
          {
            // Inside declaration - add Vamp-generated variable as initializer
            if (doStmtCount)
              prefix << "unsigned int ";
            else
              prefix << "unsigned char ";

            prefix << varName << varNum++ << " = ";

// FIXME - Using AddInstAfterLoc would be better if it was willing to
// insert before a declaration.
//   int foo = bar();
//   int y = 12;
//   y += 42;
// will place instrumentation *after* the int y = 12;
            SourceLocation end = e.getLocWithOffset(2);
            AddInstrumentStmt(end, end, prefix.str(), "\n");
          }
          else
          {
            // REQ# STMT018
            AddInstAfterLoc(e, e, prefix.str());
          }
        }
      }
    }
  }
  else
  if (isa<IfStmt>(s))
  {
    // Add new block level
    NewBlock(STMT_IF, s->getLocEnd());

    int line, col;

    GetTokStartPos(s, &line, &col);
    if (doBranch)
    {
      if (gotBranchInfo)
        branchInfo << ",\n";
      // REQ# DTBS015
      branchInfo << "    \"if\", [ [" << line << "," << col;
    }

    // Cast s to IfStmt to access the "then" and "else" clauses
    IfStmt *ifStmt = cast<IfStmt>(s);
    Stmt *th = ifStmt->getThen();

    // Point to right parenthesis in if statement as end of statement
    Expr *expr = ifStmt->getCond();
    SourceRange sr = expr->getSourceRange();
    SourceLocation st = sr.getEnd();
    ifStartLoc = sr.getBegin();
    ifEndLoc = GetLocAfter(st);
    GetTokPos(ifEndLoc, &stmtEndLine, &stmtEndCol);


    // Flag we're in a conditional to avoid instrumentation of function
    // calls within
    inConditional = true;
    condEndPos = ifEndLoc;

    if (CheckLoc(sr.getBegin()))
    {
      if (doBranch)
      {
        // Add instrumentation of expression inside if (xxx)
#ifdef REWRITER_DEBUG
*vampOut << "BR3 LHS:" << sr.getBegin().getRawEncoding() << "; " << line << ", " << col << " = " << branchName << "(" << ENDL;
#endif
        ostringstream repl;
        repl << ", " << branchCnt << ")";
        branchCnt += 2;
        gotBranchInfo = true;

        ifRepl = repl.str();
//FIXME: New rewriter should handle this OK
        //Rewrite.InsertText(ifEndLoc, ifRepl.str(), true, true);
        // Flag statement so that ifRepl is placed at the end of the statement
        // in case expression is MCDC, in which case other info is placed by
        // the Rewriter.
        // REQ# BRCH007
        inIfStmt = true;

        // REQ# DTBS015
        GetTokEndPos(th, &line, &col);
        branchInfo << "," << line << "," << col << "], ";
      }

      // Add braces if needed to then clause
      // REQ# STMT012
      InstrumentStmt(th);

      Stmt *el = ifStmt->getElse();
      if (el)
      {
        if (doBranch)
        {
          // REQ# DTBS015
          // Save off location of else clause
          GetTokStartPos(el, &line, &col);
//CDBG << "Else at " << line << ", " << col << ENDL;
          branchInfo << "[" << line << "," << col;
          GetTokEndPos(el, &line, &col);
          branchInfo << "," << line << "," << col << "], ";
        }

        // Get SourceLocation of start of "else"
        SourceLocation elLoc = ifStmt->getElseLoc();
        GetTokPos(elLoc, &line, &col);

        // Save off statement info for "else"
        // REQ# DTBS011
        AddStmtInfo(line, col, line, col + 3);

        // REQ# STMT013
        // This is a kludge copy of InstrumentStmt()
        // It uses start of "else" as start of statement
        if (doStmtSingle || doStmtCount || forceStmtInst)
        {
          if (isa<CompoundStmt>(el))
          {
            // Add inst statement at start of block
            AddInstBetweenLocs(el->getLocStart(), el->getLocEnd(), elLoc, "");
          }
          else
          {
            // Add brace and inst statement at start of statement
            SourceLocation st = el->getLocStart();

            // Check for empty statement
            SourceManager &sm = Rewrite.getSourceMgr();
            bool INV;
            const char *cPtr = sm.getCharacterData(st, &INV);

            if (*cPtr == ';')
            {
              // Have empty statement ";"
              int line, col;
              GetTokPos(st, &line, &col);

              // Replace ";" by "{ <inst> }"
              // First remove ';'
              vampRewriter.ReplaceText(st, 1, "");
              AddInstrumentStmt(st, elLoc, "{\n", "\n}");
            }
            else
            {
              // Add brace and inst statement at start of statement
              SourceLocation st = el->getLocStart();
              AddInstrumentStmt(st, elLoc, "{\n", "\n");

              // Add brace at end of statement
              //Rewrite.InsertText(GetLocAfterToken(el->getLocEnd(), tok::semi), "\n}", true, true);
              SourceLocation end = GetLocForEndOfStmt(el);
              vampRewriter.InsertText(end, "\n}");
            }
          }
        }
      }
      else
      if (doBranch)
      {
        // No else clause
        // REQ# DTBS015
        branchInfo << "[0,0,0,0], ";
      }

      // Add the range used by the expression
      GetTokPos(sr.getBegin(), &line, &col);
      branchInfo << "[" << line << "," << col << ",";
      GetTokPos(GetLocAfter(sr.getEnd()), &line, &col);
      branchInfo << line << "," << --col << "], " << branchCnt - 2 << "]";

      // Add inst statement at end of if statement
      // REQ# STMT014
      AddInstEndOfStatement(s);
    }
  }
  else
  if (isa<WhileStmt>(s))
  {
    WhileStmt *While = cast<WhileStmt>(s);
    Stmt *body = While->getBody();
    Expr *expr = While->getCond();

    // Point to right parenthesis in while statement as end of statement
    SourceRange sr = expr->getSourceRange();
    SourceLocation st = sr.getEnd();
    SourceLocation st1 = GetLocAfter(st);
    GetTokPos(st1, &stmtEndLine, &stmtEndCol);

    // Count statements within while
    inStmt = true;

    // REQ# BRCH007
    InstrumentWhile(STMT_WHILE, s, body, expr);
  }
  else
  if (isa<DoStmt>(s))
  {
    // Count statements within do
    inStmt = false;

    // Don't count the do, as the while is counted
    //stmtCnt--;

    DoStmt *Do = cast<DoStmt>(s);
    Stmt *body = Do->getBody();

    // Point to the "o" in "do" as end of statement
    stmtEndLine = stmtStartLine;
    stmtEndCol = stmtStartCol + 1;

    Expr *expr = Do->getCond();
    // REQ# BRCH007
    InstrumentWhile(STMT_DO, s, body, expr);
  }
  else
  if (isa<ForStmt>(s))
  {
    ForStmt *For = cast<ForStmt>(s);
    Stmt *body = For->getBody();

    // Point to right parenthesis in for statement as end of statement
    Expr *expr = For->getCond();

    // Count statements within for
    inStmt = true;

    // REQ# BRCH007
    InstrumentWhile(STMT_FOR, s, body, expr);

    SourceLocation rParen = For->getRParenLoc();
    GetTokPos(rParen, &stmtEndLine, &stmtEndCol);

    //inConditional = true;
  }
  else
  if (isa<SwitchStmt>(s))
  {
    // Add new block level
    NewBlock(STMT_SWITCH, s->getLocEnd());

    // Flag there is not a flow control change just prior to a case statement
    // such as a break, return, continue, exit() or goto.
    nextCase = false;

    // Flag we're in a conditional to avoid statement instrumentation of
    // function calls within
    inConditional = true;

    int line, col;
    SWITCH_INFO swInfo;
    swInfo.oss = new ostringstream;

    GetTokStartPos(s, &swInfo.stLine,
                      &swInfo.stCol);
    GetTokEndPos(s, &swInfo.endLine,
                    &swInfo.endCol);
/*
CDBG << "Switch Start: " << swInfo.stLine << ", " << swInfo.stCol << ENDL;
CDBG << "Switch End: "  << swInfo.endLine << ", " << swInfo.endCol << ENDL;
*/

    // Haven't found a case or default statement yet - once we have,
    // we can place a goto above and label after statement instrumentation.
    // Such as:
    //  goto _vamp_label;
    //  case 42:
    //  _vamp_foo_stmt(12);
    //  _vamp_label12:
    gotCase = false;

    SwitchStmt *Switch = cast<SwitchStmt>(s);
    Stmt *BODY = Switch->getBody();
    GetTokEndPos(BODY, &swInfo.line, &swInfo.col);
    swInfo.cnt = 0;

    // Point to right parenthesis in switch statement as end of statement
    Expr *expr = Switch->getCond();
    SourceRange sr = expr->getSourceRange();
    SourceLocation st = sr.getEnd();
    SourceLocation st1 = GetLocAfter(st);
    GetTokPos(st1, &stmtEndLine, &stmtEndCol);

    // Flag we're in a conditional to avoid instrumentation of function
    // calls within
    inConditional = true;
    condEndPos = st1;

/*
SourceLocation l = Switch->getSwitchLoc();
GetTokPos(l, &line, &col);
CDBG << "Switch Loc: " << line << ", " << col << ENDL;
CDBG << "Switch end: " << stmtEndLline << ", " << stmtEndCol << ENDL;
*/

    // Add the range used by the expression
    GetTokPos(sr.getBegin(), &swInfo.stLineExpr,
                             &swInfo.stColExpr);
    swInfo.endLineExpr = stmtEndLine;
    swInfo.endColExpr = stmtEndCol - 1;
    switchInfo.push_back(swInfo);

    // Count statements within switch
    inStmt = true;

    // Add inst statement at end of switch statement
    AddInstEndOfStatement(s);
  }
  else
  if (isa<CaseStmt>(s) || isa<DefaultStmt>(s))
  {
    SourceLocation colon;
    Stmt *subStmt;
    bool isCase;

    if (isa<CaseStmt>(s))
    {
      CaseStmt *c = cast<CaseStmt>(s);
      colon = c->getColonLoc();
      subStmt = c->getSubStmt();
      isCase = true;
    }
    else
    {
      DefaultStmt *c = cast<DefaultStmt>(s);
      colon = c->getColonLoc();
      subStmt = c->getSubStmt();
      isCase = false;
    }

    SourceLocation st = s->getLocStart();
    int line, col;
    GetTokStartPos(s, &line, &col);
    *switchInfo.back().oss << ", [" << line << "," << col;
    GetTokPos(colon, &stmtEndLine, &stmtEndCol);

// FIXME: This should always be true - assert?
    if (switchInfo.size() > 0)
    {
      *switchInfo.back().oss << "," << stmtEndLine << "," << stmtEndCol << "]";
      switchInfo.back().cnt++;
    }

    // Point past colon for instrumentation
    colon = colon.getLocWithOffset(1);

    // Make sure statement instrumentation occurs
    forceStmtInst = true;

#ifdef VAMP_DEBUG_SWITCH
CDBG << "gotCase = " << gotCase << "; nextCase = " << nextCase << "\n";
#endif

    if (gotCase && !nextCase && (doStmtSingle || doStmtCount || doBranch))
    {
      ostringstream label;
      label << "_vamp_" << fileName << "_label" << labelNum++;
      string caseLabel = label.str();
      //Rewrite.InsertText(st, "goto " + caseLabel + ";\n", true, true);
      // REQ# STMT016
      Rewrite.ReplaceText(st, 1, "goto " + caseLabel + ";\n" +
                          (isCase ? "c" : "d"));
      // REQ# STMT015
      // REQ# STMT017
      AddInstrumentStmt(colon, st, "\n", "\n" + caseLabel + ":\n");
    }
    else
    {
      // Add inst statement at end of case statement
//FIXME: Use AddInstAfterLoc()?
      // REQ# STMT015
      AddInstrumentStmt(colon, st, "\n", "");
      gotCase = true;
    }

    forceStmtInst = false;

    // Flag we haven't had a control flow break yet, such as break; goto, etc.
    nextCase = false;
  }
  else
  if ((isa<BreakStmt>(s)) || (isa<ContinueStmt>(s)) || (isa<GotoStmt>(s)))
  {
    SourceLocation stmt;
    string tk;
    stmt = GetTokenAfterStmt(s, tk);
    nextCase = ((tk == "case") || (tk == "default")) &&
               ((blockLevel.size() == 0) ||
                (blockLevel.back().type == STMT_SWITCH));

#ifdef VAMP_DEBUG_SWITCH
CDBG << "nextCase = " << nextCase << "; type = " << tk << endl;
#endif

    SourceLocation st = s->getLocStart();
    if (isa<BreakStmt>(s))
    {
      // Point to the ; after break
      stmtEndLine = stmtStartLine;
      stmtEndCol = stmtStartCol + 5;
    }
    else
    if (isa<ContinueStmt>(s))
    {
      // Point to the ; after continue
      stmtEndLine = stmtStartLine;
      stmtEndCol = stmtStartCol + 8;
    }

    // Add inst statement after break/continue/goto statement
    if (!nextCase)
        AddInstEndOfStatement(s);
  }
  else
  if (isa<ReturnStmt>(s))
  {
    string tk;
    SourceLocation stmt = GetTokenAfterStmt(s, tk);
    nextCase = ((tk == "case") || (tk == "default")) &&
               ((blockLevel.size() == 0) ||
                (blockLevel.back().type == STMT_SWITCH));

#ifdef VAMP_DEBUG_SWITCH
CDBG << "return nextCase = " << nextCase << "; type = " << tk << endl;
#endif

    SourceLocation st = s->getLocStart();

    if ((stmtStartLine == stmtEndLine) && (stmtStartCol == stmtEndCol))
    {
      // Point to the ; after return (assumes "return;")
      stmtEndLine = stmtStartLine;
      stmtEndCol = stmtStartCol + 6;
    }

    if (inMain)
    {
      if (CheckLoc(st))
      {
////    Rewrite.InsertText(st, "\n_vamp_output();\n", true, true);
        // Rewrite the 'r' in "return" to guarantee _vamp_output is placed.
        // REQ# MISC002
        vampRewriter.ReplaceText(st, 1, "\n_vamp_output();\nr");
      }
    }

    // Add inst statement after return statement
    if (!nextCase)
      AddInstEndOfStatement(s);
  }
  else
  if (isa<LabelStmt>(s))
  {
    SourceLocation st = s->getLocStart();
    SourceLocation end = GetLocAfterToken(st, tok::colon);
    // Add inst statement at end of label statement
//FIXME: Use AddInstAfterLoc()?
    AddInstrumentStmt(end, st, "\n", "\n");
  }

  if (addStmt)
  {
    // REQ# DTBS011
    AddStmtInfo(stmtStartLine, stmtStartCol, stmtEndLine, stmtEndCol);
  }

  return true; // returning false aborts the traversal
}

bool MyRecursiveASTVisitor::VisitFunctionDecl(FunctionDecl *f)
{
  // REQ# STMT019
  inFuncDecl = true;
  firstStmt = false;
  SourceRange sr = f->getSourceRange();

/*
  string fn = f->getNameInfo().getName().getAsString();
  cout << "Function name is " << fn << "\n";
  cout << "hasBody() = " << f->hasBody() << "\n";
  cout << "isExternC() = " << f->isExternC() << "\n";
  cout << "isInlineSpecified() = " << f->isInlineSpecified() << "\n";
  cout << "hasSkippedBody() == " << f->hasSkippedBody() << "\n";
  cout << "isInlineDefinitionExternallyVisible() == " << f->isInlineDefinitionExternallyVisible() << "\n";
  cout << "doesDeclarationForceExternallyVisibleDefinition() == " << f->doesDeclarationForceExternallyVisibleDefinition() << "\n";
  cout << "StorageClass == SC_Extern" << (int) (f->getStorageClass() == SC_Extern) << "\n";
  cout << "---------------\n";
//  if (f->hasBody())// && !f->isInlineSpecified())
  if (f->hasBody() && !f->isExternC())
*/
  if (f->hasBody() && (!f->isInlineSpecified() || (f->getStorageClass() != SC_Extern)))
  {
    inFunction = true;

    Stmt *s = f->getBody();

//    QualType q = f->getResultType();
//    const Type *typ = q.getTypePtr();

    // REQ# MISC002
    inMain = f->isMain();
    SourceLocation end = s->getLocEnd();

    // Point to start of function, so any Stmt will be ignored until afterwards
    funcStartPos = s->getLocStart().getRawEncoding();

    // Get name of function
    DeclarationNameInfo dni = f->getNameInfo();
    DeclarationName dn = dni.getName();
    funcName = dn.getAsString();
    if (funcCnt++ > 0)
      funcInfo << ",\n";
    // REQ# DTBS010
    funcInfo << "    \"" << funcName << "\", [";
    int line, col;
    SourceLocation st = sr.getBegin();
    GetTokPos(st, &line, &col);
    funcInfo << line << "," << col << ",";
    GetTokPos(end, &line, &col);
    funcInfo << line << "," << col << "]";

    // Get start of body and ignore any Stmt info until we get there
    // Once we get there, skip past any declarations before inserting
    // first statement instrumentation (via firstStmt = true)
    GetTokPos(s->getLocStart(), &stmtEndLine, &stmtEndCol);

    // Reset variables for next function
    // REQ# STMT019
    inDecl = false;
    declWithInit = false;
  }
  else
  {
    // Point to end of prototype, so any Stmt will be ignored until afterwards
    funcStartPos = sr.getEnd().getRawEncoding();
  }

  return true; // returning false aborts the traversal
}

bool MyASTConsumer::HandleTopLevelDecl(DeclGroupRef d)
{
  typedef DeclGroupRef::iterator iter;

  for (iter b = d.begin(), e = d.end(); b != e; ++b)
  {
/*
 *** SourceLocation fails when including stdio.h!
 *** Must be some non-variable declaration.
 *** Don't bother counting as a statement.
 *** Or see:
 ***  bool VisitVarDecl(VarDecl *var)
 ***  {
 ***     Expr * Init = var->getInit() ;
 ***     bool IsGlobal = var->hasGlobalStorage() && !var->isStaticLocal() ;
 ***  ...

    if (!(*b)->hasBody())
    {
      // This is a non-function declaration, which I assume to be a variable
      // declaration.  Set necessary flags in case declaration has an
      // initializer, in which case we treat it in the statement count.
      bool INV;
      SourceLocation ST = (*b)->getLocStart();
      SourceManager &sourceMgr = rv.Rewrite.getSourceMgr();
      rv.stmtStartLine = sourceMgr.getPresumedLineNumber(ST, &INV);
      rv.stmtStartCol =  sourceMgr.getPresumedColumnNumber(ST, &INV);
      SourceLocation loc = (*b)->getLocEnd();
      int tokOffset = Lexer::MeasureTokenLength(loc,
                                                sourceMgr,
                                                rv.Rewrite.getLangOpts()) + 1;
      SourceLocation END = loc.getLocWithOffset(tokOffset);
      rv.stmtEndLine = sourceMgr.getPresumedLineNumber(END, &INV);
      rv.stmtEndCol =  sourceMgr.getPresumedColumnNumber(END, &INV);
      rv.inDecl = true;
      rv.inLogicalOp = false;
      rv.inStmt = false;
    }
*/

    rv.TraverseDecl(*b);
    rv.CheckMCDCExprEnd();

//cout << "inLogicalOp = " << (int) rv.inLogicalOp << "\n";

    if (isa<FunctionDecl>(*b) && (*b)->hasBody())
    {
      // Clean up after function
      rv.FunctionEnd((*b)->getLocEnd());
    }
  }

  return true; // keep going
}

//CompilerInstance compiler;

// Instrument file with command line options and selected vampOptions
// Returns true on success, and false on failure
bool Vamp::Instrument(int argc, char *argv[], string outName, VAMP_CONFIG &vampOptions)
{
  //CompilerInstance compiler;
//std::unique_ptr<CompilerInstance> compiler(new CompilerInstance());
    CompilerInstance *compiler = new CompilerInstance;
//DiagnosticOptions &diagOpts = compiler->getDiagnosticOpts();
//IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());
IntrusiveRefCntPtr<DiagnosticOptions> diagOpts = new DiagnosticOptions();
#ifdef USE_QT
  diagOpts->ShowColors = true;
#endif
//  llvm::sys::Process::UseANSIEscapeCodes(true);
  std::string verr_string;
  vamp_string_ostream vErr(verr_string);
  TextDiagnosticPrinter *pTextDiagnosticPrinter =
                         new TextDiagnosticPrinter( vErr, &*diagOpts);
  compiler->createDiagnostics(pTextDiagnosticPrinter);

  // Create an invocation that passes any flags to preprocessor
  //unique_ptr<CompilerInvocation> Invocation(new CompilerInvocation);
  CompilerInvocation::CreateFromArgs(compiler->getInvocation(), argv + 1, argv + argc,
                                      compiler->getDiagnostics());

  compiler->getInvocation().setLangDefaults(compiler->getLangOpts(),
                              clang::IK_CXX,
                              vampOptions.langStandard);

  //compiler->setInvocation(Invocation.release());
  // Set default target triple
  shared_ptr<TargetOptions> pto( new TargetOptions());
  pto->Triple = llvm::sys::getDefaultTargetTriple();
  shared_ptr<TargetInfo>
    pti(TargetInfo::CreateTargetInfo(compiler->getDiagnostics(), pto));
  compiler->setTarget(pti.get());

  compiler->createFileManager();
  compiler->createSourceManager(compiler->getFileManager());

  compiler->createPreprocessor(TU_Module);
  compiler->getPreprocessorOpts().UsePredefines = false;
  compiler->getPreprocessor().getBuiltinInfo().InitializeBuiltins(compiler->getPreprocessor().getIdentifierTable(),
                             compiler->getPreprocessor().getLangOpts());

  compiler->createASTContext();

  SourceManager &sourceMgr = compiler->getSourceManager();

  // Initialize rewriter
  Rewriter Rewrite;
  Rewrite.setSourceMgr(sourceMgr, compiler->getLangOpts());

  // Get filename
  string fileName(argv[argc - 1]);

  const FileEntry *pFile = compiler->getFileManager().getFile(fileName);
  //sourceMgr.createMainFileID(pFile);
  sourceMgr.setMainFileID(sourceMgr.createFileID(pFile, SourceLocation(), SrcMgr::C_User));
  if (pFile == NULL)
  {
      // File not found
      return false;
  }
  compiler->getDiagnosticClient().BeginSourceFile(compiler->getLangOpts(),
                                           &compiler->getPreprocessor());

  string dirName = pFile->getDir()->getName();
  if (dirName == ".")
  {
    char ascDirName[256];
    getcwd(ascDirName, sizeof(ascDirName));
    dirName = string(ascDirName);
  }

  // Get GMT time of modification as YYYY-MM-DD HH:MM:SS
  time_t modTime = pFile->getModificationTime();

  MyASTConsumer astConsumer(Rewrite, vampOut, vampErr);

  astConsumer.rv.SetVampOptions(vampOptions);
  if (!astConsumer.rv.PrepareResults(dirName, outName, fileName.c_str()))
  {
    return false;
  }

  try {
    // Parse the AST
    ParseAST(compiler->getPreprocessor(),
             &astConsumer,
             compiler->getASTContext());
  } catch(exception &e)
  {
    *vampErr << "Could not instrument source - aborting" << ENDL;
    return false;
  }

  compiler->getDiagnosticClient().EndSourceFile();
//cout << verr_string << "\n" << flush;
  *vampErr << "\n" << verr_string << "\n";

#ifdef VAMP_DEBUG
  *vampOut << "hasUncompilableErrorOccurred() = ";
  if (compiler->getDiagnostics().hasUncompilableErrorOccurred())
      *vampOut << "True" << ENDL;
  else
      *vampOut << "False" << ENDL;
  *vampOut << "hasUnrecoverableErrorOccurred() = ";
  if (compiler->getDiagnostics().hasUnrecoverableErrorOccurred())
      *vampOut << "True" << ENDL;
  else
      *vampOut << "False" << ENDL;
  *vampOut << "hasFatalErrorOccurred() = ";
  if (compiler->getDiagnostics().hasFatalErrorOccurred())
      *vampOut << "True" << ENDL;
  else
      *vampOut << "False" << ENDL;
  *vampOut << "hasErrorOccurred() = ";
  if (compiler->getDiagnostics().hasErrorOccurred())
      *vampOut << "True" << ENDL;
  else
      *vampOut << "False" << ENDL;
#endif

  if (compiler->getDiagnostics().hasErrorOccurred())
  {
    *vampOut << ENDL << "Fatal error(s) occurred - instrumentation aborted" << ENDL;
    return false;
  }

  if (!astConsumer.rv.ProcessResults(dirName, modTime))
  {
    return false;
  }

  return true;  // Return success
}

#ifdef DO_VAMP_MAIN
int main(int argc, char *argv[])
{
  if (argc < 2)
  {
     llvm::errs() << "Usage: vamp <options> <filename>" << ENDL;
     return 1;
  }

  struct stat sb;
  ostringstream cfgErr;
  ConfigFile cf(&cfgErr);

  VAMP_CONFIG vampOptions;

  // Get filename
  string fileName(argv[argc - 1]);

  // Make sure it exists
  if (stat(fileName.c_str(), &sb) == -1)
  {
    perror(fileName.c_str());
    return EXIT_FAILURE;
  }

  // REQ# CONF001
  if (!cf.ParseConfigFile("vamp.cfg", vampOptions))
  {
    llvm::errs() << "Bad vamp.cfg: " << cfgErr.str() << ENDL;
  }

  string outName(argv[1]);
  size_t path = outName.rfind("/");
#ifdef _WIN32
    size_t path1 = outName.rfind("\\");
    if ((path1 != string::npos) && (path1 > path))
        path = path1;
#endif
  if (path == string::npos)
  {
    path = 0;
  }
  else
    ++path;

  // Strip path from filename
  fileName = outName.substr(path);
  string pathName = outName.substr(0, path);

  size_t ext = fileName.rfind(".");
  size_t len = fileName.length();
  if (ext == string::npos)
     ext = len;
  string extension = fileName.substr(ext);

  // Remove instrumentation suffix
  fileName = fileName.substr(0, ext);
  // Remove suffix
  //fileName = fileName.substr(0, fileName.length() - rv.saveSuffix.length());
  outName = pathName + vampOptions.saveDirectory + "/" + fileName +
                       vampOptions.saveSuffix + extension;

#ifdef USE_STRING
  std::string outStr;
  std::string errStr;
  vamp_string_ostream vout(outStr);
  vamp_string_ostream verr(errStr);
  //ostringstream outStr, errStr;

  //Vamp vamp(&outStr, &errStr);
  Vamp vamp(&vout, &verr);
  vamp.Instrument(argc, argv, outName, vampOptions);
  llvm::outs() << outStr;
  llvm::errs() << errStr;
#else
  Vamp vamp(&llvm::outs(), &llvm::errs());
  vamp.Instrument(argc, argv, outName, vampOptions);
#endif

  return 0;
}
#endif

