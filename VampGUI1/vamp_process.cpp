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

#include "vamp_process.h"

string modTimeStr(time_t &modTime)
{
  struct tm *modTm = gmtime(&modTime);
  char modTimeAsc[64];
  strftime(modTimeAsc, sizeof(modTimeAsc), "%Y-%m-%d %H:%M:%S", modTm);
  string str(modTimeAsc);
  return str;
}

History::History()
{
  coverageOptions = 0;
  coveredInfo = NULL;
  covCntInfo = NULL;
  brInfo = NULL;
  mcdcInfo = NULL;
  condInfo = NULL;
  stackOverflow = 0;
  instCount = 0;
  branchCount = 0;
  mcdcCount = 0;
  condCount = 0;
}

History::~History()
{
  if (coveredInfo != NULL)
  {
    free(coveredInfo);
  }

  if (covCntInfo != NULL)
  {
    free(covCntInfo);
  }

  if (brInfo != NULL)
  {
    free(brInfo);
  }

  if (mcdcInfo != NULL)
  {
    free(mcdcInfo);
  }

  if (condInfo != NULL)
  {
    free(condInfo);
  }
}

// Load information from specified history file <fileName>
// Reads:
// - Instrumented statement info
// - Branch info
// - MC/DC info
bool History::loadHistory(string histName, VAMP_ERR_STREAM *vampErr)
{
  ifstream historyFile (histName.c_str(), ios_base::binary);
  bool result;

  // Read input source file
  if (historyFile.is_open())
  {
    // Load coverage options
    coverageOptions = historyFile.get();

    if (coverageOptions & DO_STATEMENT_COUNT)
    {
      // Load instrumented statement coverage info with counts
      instCount = (historyFile.get() << 8) | historyFile.get();
#ifdef VAMP_DEBUG_STMT
      CDBG << histName << " Instrumented statement count = " << instCount << ENDL;
#endif
      if (instCount)
      {
        covCntInfo = (unsigned int *) malloc(instCount * 4);
        unsigned int *ptr = covCntInfo;
        for (int i = 0; i < instCount; i++)
        {
          // Read 4 bytes into unsigned int
          *ptr++ = (historyFile.get() << 24) |
                   (historyFile.get() << 16) |
                   (historyFile.get() << 8) |
                    historyFile.get();
#ifdef VAMP_DEBUG_STMT
CDBG << "covCntInfo[" << i << "] = " << covCntInfo[i] << ENDL;
#endif
        }
      }
    }
    else
    if (coverageOptions & (DO_STATEMENT_SINGLE | DO_BRANCH))
    {
      // Load instrumented statement coverage info
      // May exist for branch coverage of case statements
      instCount = (historyFile.get() << 8) | historyFile.get();
#ifdef VAMP_DEBUG_STMT
CDBG << histName << " Instrumented statement count = " << instCount << ENDL;
#endif
      if (instCount)
      {
        coveredInfo = (unsigned char *) malloc(instCount);
        unsigned char *ptr = coveredInfo;
        for (int i = 0; i < instCount; i++)
        {
          *ptr++ = historyFile.get();
#ifdef VAMP_DEBUG_STMT
CDBG << "coveredInfo[" << i << "] = " << std::hex << (int) coveredInfo[i] << ENDL;
#endif
        }
      }
    }

    if (coverageOptions & DO_BRANCH)
    {
      // Load branch info
      branchCount = (historyFile.get() << 8) | historyFile.get();
#ifdef VAMP_DEBUG
CDBG << histName << " Branch count = " << branchCount << ENDL;
#endif
      if (branchCount)
      {
        brInfo = (unsigned char *) malloc(branchCount);
        unsigned char *ptr = brInfo;
        for (int i = 0; i < branchCount; i++)
          *ptr++ = historyFile.get();
      }
    }

    if (coverageOptions & DO_MCDC)
    {
      // Load MC/DC info
      mcdcCount = (historyFile.get() << 8) | historyFile.get();
#ifdef VAMP_DEBUG
CDBG << histName << " MCDC count = " << mcdcCount << ENDL;
#endif
      if (mcdcCount)
      {
        mcdcInfo = (unsigned char *) malloc(mcdcCount);
        unsigned char *ptr = mcdcInfo;
        for (int i = 0; i < mcdcCount; i++)
          *ptr++ = historyFile.get();
      }
    
      // Get stack overflow flag
      stackOverflow = 0;
      stackOverflow = (historyFile.get() << 8) | historyFile.get();
#ifdef VAMP_DEBUG
CDBG << histName << " Stack Overflow = " << stackOverflow << ENDL;
#endif
    }
    else
    if (coverageOptions & DO_CONDITION)
    {
      // Load Condition info
      condCount = (historyFile.get() << 8) | historyFile.get();
#ifdef VAMP_DEBUG
CDBG << histName << " Condition count = " << condCount << ENDL;
#endif
      if (condCount)
      {
        condInfo = (unsigned char *) malloc(condCount);
        unsigned char *ptr = condInfo;
        for (int i = 0; i < condCount; i++)
          *ptr++ = historyFile.get();
      }
    }
    else
    {
      int count = (historyFile.get() << 8) | historyFile.get();

      if (count)
      {
        *vampErr << "Unexpected byte count in history file - aborting" << ENDL;
        historyFile.close();
        return false;
      }
    }

    historyFile.close();

    // Get time of creation (modification) for history file as string
    struct stat fileStat;
    stat(histName.c_str(), &fileStat);
    modTime = modTimeStr(fileStat.st_mtime);

    result = true;
  }
  else
  {
    *vampErr << "Unable to open history file: " << histName << ENDL;

    result = false;
  }

  return result;
}


VampDB::VampDB()
{
  totalStmtCount = 0;    // Total # statements in source code

  functionCount = 0;     // Number of functions in source
  ifInfoIndex = 0;       // branchInfo.index point to which element to use
  whileInfoIndex = 0;    // for associated if/while/for/switch statement
  forInfoIndex = 0;
  switchInfoIndex = 0;

  histDirectory = ".";
  combineHistory = true;            // Combine history files
  outputCombinedHistory = true;     // Output combined history files
  showTestCases = true;             // Show recommended test cases
  htmlDirectory = ".";
  htmlSuffix = "_vamp";
}

void VampDB::SetVampOptions(VAMP_REPORT_CONFIG &vo)
{
  histDirectory = vo.histDirectory;
  combineHistory = vo.combineHistory;
  showTestCases = vo.showTestCases;
  generateReport = vo.generateReport;
  htmlDirectory = vo.htmlDirectory;
  htmlSuffix = vo.htmlSuffix;
  reportSeparator = vo.reportSeparator;
}

// Get [lhsLine,lhsCol,rhsLine,rhsCol] from JSON file
// Place into sourceLocationType: lhsLine, lhsCol, rhsLine, rhsCol

sourceLocationType VampDB::getLoc(vector<JsonNode> &n)
{
  sourceLocationType loc;

  vector<JsonNode>::iterator i = n.begin();
  loc.lhsLine = i->as_int();
  i++;
  loc.lhsCol  = i->as_int();
  i++;
  loc.rhsLine = i->as_int();
  i++;
  loc.rhsCol  = i->as_int();
  return loc;
}

// Get [line,col] from JSON file
// Place into instInfoType: line, col

instInfoType VampDB::getInstLoc(vector<JsonNode> &n)
{
  instInfoType loc;

  vector<JsonNode>::iterator i = n.begin();
  loc.line = i->as_int();
  i++;
  loc.col  = i->as_int();
  return loc;
}

// Get function_info from JSON file of form:
//   "function_info":
//   [
//     // Function name for each C function, start/end of function
//     "<func1>", [<l1>,<c1>,<l2>,<c2>],
//             ...
//     "<funcn>", [<l1>,<c1>,<l2>,<c2>]
//   ]
//
// Place into functionInfoType:
//     "<func1>" -> function,
//     [<l1>,<c1>,<l2>,<c2>] -> lhsLine, lhsCol, rhsLine, rhsCol

void VampDB::parseFuncInfo(Json &json, vector<JsonNode> &n)
{
  functionInfoType f;

  vector<JsonNode>::iterator i = n.begin();

  while (i != n.end())
  {
    f.function = i->as_string().data();
    ++i;

    vector<JsonNode> node;
    json.ParseArray(i->as_string(), node);

    f.loc = getLoc(node);
    functionInfo.push_back(f);
#ifdef VAMP_DEBUG_PARSE
    CDBG << "function_info: " << f.function << "=[" <<
         f.loc.lhsLine << "," <<
         f.loc.lhsCol  << "," <<
         f.loc.rhsLine << "," <<
         f.loc.rhsCol  << "]" << ENDL;
#endif

    ++functionCount;

    ++i;
  }
}

// Get statement_info from JSON file of form:
//   "statement_info":
//   [
//     // Start/end of each C statement
//     [<stmt1>:<l1>,<c1>,<l2>,<c2>],
//             ...
//     [<stmtn>:<l1>,<c1>,<l2>,<c2>]
//   ]
//
// Place into sourceLocationType:
//     [<l1>,<c1>,<l2>,<c2>] -> lhsLine, lhsCol, rhsLine, rhsCol

void VampDB::parseStatementInfo(Json &json, vector<JsonNode> &n)
{
  sourceLocationType loc;
  int stmtCount = 0;
  int numStmts = 0;

  vector<JsonNode>::iterator i = n.begin();
  vector<functionInfoType>::iterator func = functionInfo.begin();

  while (i != n.end())
  {
    vector<JsonNode> node;
    json.ParseArray(i->as_string(), node);

    loc = getLoc(node);

    if (loc.lhsLine >= func->loc.lhsLine)
    {
      while ((func < functionInfo.end()) &&
             ((loc.lhsLine > func->loc.rhsLine) ||
              ((loc.lhsLine == func->loc.rhsLine) &&
               (loc.lhsCol  > func->loc.rhsCol))))
      {
        // Save statement count for current function
        functionStmtCount.push_back(stmtCount);
        // Save statement count for entire source
        totalStmtCount += stmtCount;

        // Increment count of # statement functions calculated
        ++numStmts;

        // Start over for new function
        stmtCount = 0;
        ++func;
      }

      statementInfo.push_back(loc);
      ++stmtCount;
#ifdef VAMP_DEBUG_PARSE
      CDBG << "statement_info: " << "[" <<
           loc.lhsLine << "," <<
           loc.lhsCol  << "," <<
           loc.rhsLine << "," <<
           loc.rhsCol  << "]" << ENDL;
#endif
    }
    else
    {
      // Ignore statements outside of function
      // (external declarations with initializers)
      // Maybe they shouldn't even be in JSON file
#ifdef VAMP_DEBUG_PARSE
      CDBG << "Ignoring statement at line " <<
              loc.lhsLine << ", " << loc.lhsCol << ENDL;
#endif
    }

    ++i;
  }

  // Save statement count for current function
  functionStmtCount.push_back(stmtCount);
  // Save statement count for entire source
  totalStmtCount += stmtCount;

  // Increment count of # statement functions calculated
  ++numStmts;

  // Make sure matching number of statement info is present
  while (numStmts++ < functionInfo.size())
  {
    // Save statement count for current function
    functionStmtCount.push_back(0);
  }
}

// Get instr_info from JSON file of form:
//   "instr_info":
//   [
//     // Location for each instrumentation statement added, start/end of function
//     [<l1>,<c1>],
//         ...
//     [<ln>,<cn>]
//   ]
//
// Place into instInfoType:
//     [<l1>,<c1>] -> line, col

void VampDB::parseInstInfo(Json &json, vector<JsonNode> &n)
{
  instInfoType loc;

  vector<JsonNode>::iterator i = n.begin();

  while (i != n.end())
  {
    vector<JsonNode> node;
    json.ParseArray(i->as_string(), node);

    loc = getInstLoc(node);
    instInfo.push_back(loc);
#ifdef VAMP_DEBUG_PARSE
    CDBG << "instr_info: " << "[" <<
         loc.line << "," <<
         loc.col  << "]" << ENDL;
#endif

    ++i;
  }
}

// Get mcdc_expr_info from JSON file of form:
//   "mcdc_expr_info":
//   [
//     [ n, // Number of operands in first MCDC expression
//      // Note LHS = left-hand expression, RHS = right-hand expression
//      [<LHS l1>,<LHS c1>,<LHS l2>,<LHS c2>], \
//      [<RHS l1>,<RHS c1>,<RHS l2>,<RHS c2>], "<op>",
//                       ...
//      [<LHS ln>,<LHS cn>,<LHS ln>,<LHS cn>], \
//      [<RHS ln>,<RHS cn>,<RHS ln>,<RHS cn>], "<op>",
//     ],
//                       ...
//     [ n, // Number of operands in nth MCDC expression
//      [<LHS l1>,<LHS c1>,<LHS l2>,<LHS c2>], \
//      [<RHS l1>,<RHS c1>,<RHS l2>,<RHS c2>], "<op>",
//                       ...
//      [<LHS ln>,<LHS cn>,<LHS ln>,<LHS cn>], \
//      [<RHS ln>,<RHS cn>,<RHS ln>,<RHS cn>], "<op>",
//     ]
//   ]
//
// Place into mcdcExprInfoType:
// [<LHS l1>,<LHS c1>,<LHS l2>,<LHS c2>] -> lhsLoc
// [<RHS l1>,<RHS c1>,<RHS l2>,<RHS c2>] -> rhsLoc
// "<op>" -> andOp (true if "&&", else false ( = "||") )

mcdcExprInfoType VampDB::getMCDCExprInfo(Json &json, vector<JsonNode> &n)
{
  mcdcExprInfoType exprInfo;
  int cnt;
  mcdcOpInfoType opInfo;

  vector<JsonNode>::iterator i = n.begin();

  cnt = i->as_int();
  ++i;

  while (cnt--)
  {
    vector<JsonNode> node;
    json.ParseArray(i->as_string(), node);
    opInfo.lhsLoc = getLoc(node);
    ++i;
    vector<JsonNode> node1;
    json.ParseArray(i->as_string(), node1);
    opInfo.rhsLoc = getLoc(node1);
    ++i;
    opInfo.andOp = (i->as_string() == "&&");
    ++i;
    exprInfo.opInfo.push_back(opInfo);
  }

  return exprInfo;
}

// Parse MC/DC information from JSON file generated by vamp
void VampDB::parseMCDCExprInfo(Json &json, vector<JsonNode> &n)
{
  mcdcExprInfoType op;
  vector<mcdcOpInfoType>::iterator j;

  vector<JsonNode>::iterator i = n.begin();

  while (i != n.end())
  {
    vector<JsonNode> node;
    json.ParseArray(i->as_string(), node);
    op = getMCDCExprInfo(json, node);
    mcdcExprInfo.push_back(op);
#ifdef VAMP_DEBUG_PARSE
    CDBG << "mcdc_expr_info: " << "[" << ENDL;
    for (j = op.opInfo.begin(); j < op.opInfo.end(); j++)
    {
      CDBG << "  [" <<
         j->lhsLoc.lhsLine << "," <<
         j->lhsLoc.lhsCol  << "," <<
         j->lhsLoc.rhsLine << "," <<
         j->lhsLoc.rhsCol  << "], [" <<
         j->rhsLoc.lhsLine << "," <<
         j->rhsLoc.lhsCol  << "," <<
         j->rhsLoc.rhsLine << "," <<
         j->rhsLoc.rhsCol  << "], " <<
         (j->andOp ? "&&" : "||") << ENDL;
    }
    CDBG << "  ]" << ENDL;
#endif

    ++i;
  }
}

// Parse "if/else" information from JSON file generated by vamp of form:
// "if", [ \
//      // Location for if expression
//      [<expr l1>,<expr c1>,<expr l2>,<expr c2>], \
//      // Location for then body
//      [<then l1>,<then c1>,<then l2>,<then c2>], \
//      // Location for else body
//      [<else l1>,<else c1>,<else l2>,<else c2>], \
//   <bit>]
// where <bit> is the low bit of the data; 1 = false condition covered
// where <bit>+1 is the next hightest bit; 1 = true  condition covered
ifElseInfoType VampDB::parseIfElseInfo(Json &json, vector<JsonNode> &n)
{
  ifElseInfoType ifElseElement;

  vector<JsonNode>::iterator i = n.begin();
  vector<JsonNode> node;
  json.ParseArray(i->as_string(), node);
  ifElseElement.ifLoc = getLoc(node);
  ++i;
  vector<JsonNode> node1;
  json.ParseArray(i->as_string(), node1);
  ifElseElement.elseLoc = getLoc(node1);
  ++i;
  vector<JsonNode> node2;
  json.ParseArray(i->as_string(), node2);
  ifElseElement.exprLoc = getLoc(node2);
  ++i;
  ifElseElement.branchNum = i->as_int();
#ifdef VAMP_DEBUG_PARSE
  CDBG << "if_else_info: " << "[" <<
           ifElseElement.ifLoc.lhsLine << "," <<
           ifElseElement.ifLoc.lhsCol  << "," <<
           ifElseElement.ifLoc.rhsLine << "," <<
           ifElseElement.ifLoc.rhsCol  << "], [" <<
           ifElseElement.elseLoc.lhsLine << "," <<
           ifElseElement.elseLoc.lhsCol  << "," <<
           ifElseElement.elseLoc.rhsLine << "," <<
           ifElseElement.elseLoc.rhsCol  << "], [" <<
           ifElseElement.exprLoc.lhsLine << "," <<
           ifElseElement.exprLoc.lhsCol  << "," <<
           ifElseElement.exprLoc.rhsLine << "," <<
           ifElseElement.exprLoc.rhsCol  << "], " <<
           ifElseElement.branchNum << ENDL;
#endif

  return ifElseElement;
}

// Parse "while" information from JSON file generated by vamp of form:
// "while", [ \
//      // Location for while expression
//      [<expr l1>,<expr c1>,<expr l2>,<expr c2>], \
//      // Location for while body
//      [<while l1>,<while c1>,<while l2>,<while c2>], \
//   <bit>]
// where <bit> is the low bit of the data; 1 = false condition covered
// where <bit>+1 is the next hightest bit; 1 = true  condition covered
whileInfoType VampDB::parseWhileInfo(Json &json, vector<JsonNode> &n)
{
  whileInfoType whileElement;

  vector<JsonNode>::iterator i = n.begin();
  vector<JsonNode> node;
  json.ParseArray(i->as_string(), node);
  whileElement.whileLoc = getLoc(node);
  ++i;
  vector<JsonNode> node1;
  json.ParseArray(i->as_string(), node1);
  whileElement.exprLoc = getLoc(node1);
  ++i;
  whileElement.branchNum = i->as_int();
#ifdef VAMP_DEBUG_PARSE
  CDBG << "while_info: " << "[" <<
           whileElement.whileLoc.lhsLine << "," <<
           whileElement.whileLoc.lhsCol  << "," <<
           whileElement.whileLoc.rhsLine << "," <<
           whileElement.whileLoc.rhsCol  << "], [" <<
           whileElement.exprLoc.lhsLine << "," <<
           whileElement.exprLoc.lhsCol  << "," <<
           whileElement.exprLoc.rhsLine << "," <<
           whileElement.exprLoc.rhsCol  << "], " <<
           whileElement.branchNum << ENDL;
#endif

  return whileElement;
}

// Parse "for" information from JSON file generated by vamp of form:
// "for", [ \
//      // Location for "for" expression
//      [<expr l1>,<expr c1>,<expr l2>,<expr c2>], \
//      // Location for "for" body
//      [<for l1>,<for c1>,<for l2>,<for c2>], \
//   <bit>]
// where <bit> is the low bit of the data; 1 = false condition covered
// where <bit>+1 is the next hightest bit; 1 = true  condition covered
forInfoType VampDB::parseForInfo(Json &json, vector<JsonNode> &n)
{
  forInfoType forElement;

  vector<JsonNode>::iterator i = n.begin();
  vector<JsonNode> node;
  json.ParseArray(i->as_string(), node);
  forElement.forLoc = getLoc(node);
  ++i;
  vector<JsonNode> node1;
  json.ParseArray(i->as_string(), node1);
  forElement.exprLoc = getLoc(node1);
  ++i;
  forElement.branchNum = i->as_int();
#ifdef VAMP_DEBUG_PARSE
  CDBG << "for_info: " << "[" <<
           forElement.forLoc.lhsLine << "," <<
           forElement.forLoc.lhsCol  << "," <<
           forElement.forLoc.rhsLine << "," <<
           forElement.forLoc.rhsCol  << "], [" <<
           forElement.exprLoc.lhsLine << "," <<
           forElement.exprLoc.lhsCol  << "," <<
           forElement.exprLoc.rhsLine << "," <<
           forElement.exprLoc.rhsCol  << "], " <<
           forElement.branchNum << ENDL;
#endif

  return forElement;
}

// Parse "switch" information from JSON file generated by vamp of form:
// "switch", [ \
//      // Location for switch body
//      [<LHS l1>,<LHS c1>,<LHS l2>,<LHS c2>], \
//      // Location for switch expression
//      [<expr l1>,<expr c1>,<expr l2>,<expr c2>], \
//      // Number of case/default statements
//      <count>,
//      // Location for case/default body
//      [<case l1>,<case c1>,<case l2>,<case c2>], \
//                    ...
//      // Location for case/default body
//      [<case ln>,<case cn>,<case ln>,<case cn>]]
switchInfoType VampDB::parseSwitchInfo(Json &json, vector<JsonNode> &n)
{
  switchInfoType switchElement;
  caseInfoType caseElement;

  vector<JsonNode>::iterator i = n.begin();
  vector<JsonNode> node;
  json.ParseArray(i->as_string(), node);
  switchElement.switchLoc = getLoc(node);
  ++i;
  vector<JsonNode> node1;
  json.ParseArray(i->as_string(), node1);
  switchElement.switchExprLoc = getLoc(node1);
  ++i;
  int cnt = i->as_int();
  ++i;

#ifdef VAMP_DEBUG_PARSE
  CDBG << "switch_info: " << "[" <<
           switchElement.switchLoc.lhsLine << "," <<
           switchElement.switchLoc.lhsCol  << "," <<
           switchElement.switchLoc.rhsLine << "," <<
           switchElement.switchLoc.rhsCol  << "], [" <<
           switchElement.switchExprLoc.lhsLine << "," <<
           switchElement.switchExprLoc.lhsCol  << "," <<
           switchElement.switchExprLoc.rhsLine << "," <<
           switchElement.switchExprLoc.rhsCol  << "]";
#endif

  while (cnt--)
  {
    vector<JsonNode> node;
    json.ParseArray(i->as_string(), node);
    caseElement.caseLoc = getLoc(node);
    i++;

    // Search for location among instrumented statements
    int j;
    for (j = 0; (j < instInfo.size()) &&
         ((instInfo[j].line != caseElement.caseLoc.lhsLine) ||
          (instInfo[j].col != caseElement.caseLoc.lhsCol)); j++) ;
    if (j < instInfo.size())
    {
      caseElement.instNum = j;
    }
    else
    {
      *vampErr << "Case statement not found in instrumented lines:" <<
              caseElement.caseLoc.lhsLine << ", " <<
              caseElement.caseLoc.lhsCol << ENDL;
      throw(1);
    }

    //caseElement.branchNum = i->as_int();
    //i++;
#ifdef VAMP_DEBUG_PARSE
    CDBG << ", [" <<
            caseElement.caseLoc.lhsLine << "," <<
            caseElement.caseLoc.lhsCol  << "," <<
            caseElement.caseLoc.rhsLine << "," <<
            caseElement.caseLoc.rhsCol  << "]";
            //caseElement.caseLoc.rhsCol  << "], " <<
            //caseElement.branchNum;
#endif
    switchElement.caseInfo.push_back(caseElement);
  }

#ifdef VAMP_DEBUG_PARSE
  CDBG << ENDL;
#endif

  return switchElement;
}

// Parse branch information from JSON file generated by vamp
void VampDB::parseBranchInfo(Json &json, vector<JsonNode> &n)
{
  branchInfoType branch;
  ifElseInfoType ifElseElement;
  whileInfoType whileElement;
  forInfoType forElement;
  switchInfoType switchElement;

  vector<JsonNode>::iterator i = n.begin();

  while (i != n.end())
  {
    if ((i->as_string() == "if") ||
        (i->as_string() == "cond"))
    {
      ++i;
      vector<JsonNode> node;
      json.ParseArray(i->as_string(), node);
      ifElseElement = parseIfElseInfo(json, node);
      branch.statementType = "if";
      branch.index = ifInfoIndex++;
      branchInfo.push_back(branch);
      ifElseInfo.push_back(ifElseElement);
    }
    else
    if (i->as_string() == "while")
    {
      ++i;
      vector<JsonNode> node;
      json.ParseArray(i->as_string(), node);
      whileElement = parseWhileInfo(json, node);
      branch.statementType = "while";
      branch.index = whileInfoIndex++;
      branchInfo.push_back(branch);
      whileInfo.push_back(whileElement);
    }
    else
    if (i->as_string() == "for")
    {
      ++i;
      vector<JsonNode> node;
      json.ParseArray(i->as_string(), node);
      forElement = parseForInfo(json, node);
      branch.statementType = "for";
      branch.index = forInfoIndex++;
      branchInfo.push_back(branch);
      forInfo.push_back(forElement);
    }
    else
    if (i->as_string() == "switch")
    {
      ++i;
      vector<JsonNode> node;
      json.ParseArray(i->as_string(), node);
      switchElement = parseSwitchInfo(json, node);
      branch.statementType = "switch";
      branch.index = switchInfoIndex++;
      branchInfo.push_back(branch);
      switchInfo.push_back(switchElement);
    }

    ++i;
  }
}

condInfoType VampDB::parseConditionElement(Json &json, vector<JsonNode> &n)
{
  condInfoType condElement;

  vector<JsonNode>::iterator i = n.begin();

  vector<JsonNode> node;
  json.ParseArray(i->as_string(), node);
  condElement.lhsLoc = getLoc(node);
  ++i;
  vector<JsonNode> node1;
  json.ParseArray(i->as_string(), node1);
  condElement.rhsLoc = getLoc(node1);
  ++i;
   condElement.condition = i->as_string().data();
  ++i;
  condElement.condNum = i->as_int();
#ifdef VAMP_DEBUG_PARSE
  CDBG << "condition_info: " << "[" <<
           condElement.lhsLoc.lhsLine << "," <<
           condElement.lhsLoc.lhsCol  << "," <<
           condElement.lhsLoc.rhsLine << "," <<
           condElement.lhsLoc.rhsCol  << "], [" <<
           condElement.rhsLoc.lhsLine << "," <<
           condElement.rhsLoc.lhsCol  << "," <<
           condElement.rhsLoc.rhsLine << "," <<
           condElement.rhsLoc.rhsCol  << "], \"" <<
           condElement.condition << "\", " <<
           condElement.condNum  << ENDL;
#endif

  return condElement;
}

// Parse condition information from JSON file generated by vamp of form:
// [ \
//      // Location for left-hand side of expression
//      [<expr l1>,<expr c1>,<expr l2>,<expr c2>], \
//      // Location for right-hand side of expression
//      [<expr r1>,<expr r1>,<expr r2>,<expr r2>], "<expr>", <cond> ]
// where <expr> is "&&" for a Boolean And, or "||" for a Boolean Or
// where <cond> is the number of the condition covered
void VampDB::parseConditionInfo(Json &json, vector<JsonNode> &n)
{
  condInfoType condElement;

  vector<JsonNode>::iterator i = n.begin();

  while (i != n.end())
  {
    vector<JsonNode> node;
    json.ParseArray(i->as_string(), node);
    condElement = parseConditionElement(json, node);
    condInfo.push_back(condElement);

    ++i;
  }
}

// Get mcdc_overflow from JSON file of form:
//   "mcdc_overflow":
//   [
//     // Start and End Locations for each MC/DC expression with more
//     // than 31 MCDC operators
//     [<l1>,<c1>,<l2>,<c2>],
//             ...
//     [<l1>,<c1>,<l2>,<c2>]
//   ]
//
// Place into sourceLocationType:
//     [<l1>,<c1>,<l2>,<c2>] -> lhsLine, lhsCol, rhsLine, rhsCol

void VampDB::parseMcdcOverflow(Json &json, vector<JsonNode> &n)
{
  sourceLocationType loc;

  vector<JsonNode>::iterator i = n.begin();

  while (i != n.end())
  {
    vector<JsonNode> node;
    json.ParseArray(i->as_string(), node);

    loc = getLoc(node);
    mcdcOverflowInfo.push_back(loc);
#ifdef VAMP_DEBUG_PARSE
    CDBG << "mcdc_overflow: " << "[" <<
           loc.lhsLine << "," <<
           loc.lhsCol  << "," <<
           loc.rhsLine << "," <<
           loc.rhsCol  << "]" << ENDL;
#endif

    ++i;
  }
}


// Parse information from JSON databse file generated by vamp
void VampDB::parseJsonDb(Json &n, VAMP_ERR_STREAM *errStr)
{
  vampErr = errStr;

  vector<JsonNode>::iterator i = n.jsonNodes.begin();

  while (i != n.jsonNodes.end())
  {
    vector<JsonNode> nodes;

    // get the node name and value as a string
    string nodeName = i->name().data();

    if (nodeName == "function_info")
    {
      n.ParseArray(i->as_string(), nodes);
      parseFuncInfo(n, nodes);
    }
    else
    if (nodeName == "instr_info")
    {
      n.ParseArray(i->as_string(), nodes);
      parseInstInfo(n, nodes);
    }
    else
    if (nodeName == "mcdc_expr_info")
    {
      n.ParseArray(i->as_string(), nodes);
      parseMCDCExprInfo(n, nodes);
    }
    else
    if (nodeName == "statement_info")
    {
      n.ParseArray(i->as_string(), nodes);
      parseStatementInfo(n, nodes);
    }
    else
    if (nodeName == "branch_info")
    {
      n.ParseArray(i->as_string(), nodes);
      parseBranchInfo(n, nodes);
    }
    else
    if (nodeName == "condition_info")
    {
      n.ParseArray(i->as_string(), nodes);
      parseConditionInfo(n, nodes);
    }
    else
    if (nodeName == "mcdc_overflow")
    {
      n.ParseArray(i->as_string(), nodes);
      parseMcdcOverflow(n, nodes);
    }
    else
    // find out where to store the values
    if (nodeName == "filename")
    {
#ifdef VAMP_DEBUG_PARSE
      CDBG << "filename string:" << i->as_string() << ENDL;
#endif
      fileName = i->as_string().data();
    }
    else
    if (nodeName == "src_filename")
    {
#ifdef VAMP_DEBUG_PARSE
      CDBG << "src_filename string:" << i->as_string() << ENDL;
#endif
      srcFileName = i->as_string().data();
    }
    else
    if (nodeName == "pathname")
    {
#ifdef VAMP_DEBUG_PARSE
      CDBG << "pathname string:" << i->as_string() << ENDL;
#endif
      pathName = i->as_string().data();
    }
    else
    if (nodeName == "modtime")
    {
#ifdef VAMP_DEBUG_PARSE
      CDBG << "modtime string:" << i->as_string() << ENDL;
#endif
      modTime = i->as_string().data();
    }
    else
    if (nodeName == "instr_filename")
    {
#ifdef VAMP_DEBUG_PARSE
      CDBG << "instr_filename string:" << i->as_string() << ENDL;
#endif
      instrFileName = i->as_string().data();
    }
    else
    if (nodeName == "instr_pathname")
    {
#ifdef VAMP_DEBUG_PARSE
      CDBG << "instr_pathname string:" << i->as_string() << ENDL;
#endif
      instrPathName = i->as_string().data();
    }
    else
    if (nodeName == "instr_modtime")
    {
#ifdef VAMP_DEBUG_PARSE
      CDBG << "instr_modtime string:" << i->as_string() << ENDL;
#endif
      instrModTime = i->as_string().data();
    }

    ++i;
  }

  if (functionCount)
  {
    instInfoType end;
    end.line = functionInfo[functionCount - 1].loc.rhsLine;
    end.col  = functionInfo[functionCount - 1].loc.rhsCol;
    instInfo.push_back(end);
  }
}

// Get [ppSrcLine,srcLine,fileName] from JSON Map file
// Place into mapType: ppSrcLine,srcLine,fileName

mapType VampProcess::getMapInfo(vector<JsonNode> &n)
{
  mapType loc;

  vector<JsonNode>::iterator i = n.begin();
  loc.ppSrcLine = i->as_int();
  i++;
  loc.srcLine  = i->as_int();
  i++;
  loc.fileName = i->as_string();
  return loc;
}

// Get instr_info from JSON Map file of form:
//   "linemarkers":
//   [
//     // pn = Line number for preprocessed source
//     // sn = Corresponding line number for original source
//     // filen = Filename of original source
//     [<p1>,<s1>,<file1>],
//         ...
//     [<pn>,<sn>,<filen>]
//   ]
//
// Place into instInfoType:
//     [<l1>,<c1>] -> line, col

void VampProcess::parseLineMarkers(Json &json, vector<JsonNode> &n)
{
  mapType loc;

  vector<JsonNode>::iterator i = n.begin();

  while (i != n.end())
  {
    vector<JsonNode> node;
    json.ParseArray(i->as_string(), node);

    loc = getMapInfo(node);
    if (loc.fileName != mapFileName)
        loc.srcLine = -1;   // Flag as outside of source code

    mapInfo.push_back(loc);
#ifdef VAMP_DEBUG_PARSE
    CDBG << "instr_info: " << "[" <<
         loc.line << "," <<
         loc.col  << "]" << ENDL;
#endif

    ++i;
  }
}

// Parse information from JSON databse file generated by vamp
void VampProcess::parseJsonMap(Json &n)
{
  vector<JsonNode>::iterator i = n.jsonNodes.begin();

  while (i != n.jsonNodes.end())
  {
    vector<JsonNode> nodes;

    // get the node name and value as a string
    string nodeName = i->name().data();

    if (nodeName == "filename")
    {
#ifdef VAMP_DEBUG_PARSE
      CDBG << "filename string:" << i->as_string() << ENDL;
#endif
      mapFileName = i->as_string().data();
    }
    else
    if (nodeName == "preproc_filename")
    {
#ifdef VAMP_DEBUG_PARSE
      CDBG << "preproc_filename string:" << i->as_string() << ENDL;
#endif
      mapPPFileName = i->as_string().data();
    }
    else
    if (nodeName == "linemarkers")
    {
        n.ParseArray(i->as_string(), nodes);
        parseLineMarkers(n, nodes);
    }

    ++i;
  }
}

// See if a linemarker database exists, and if so, load it
void VampProcess::processLineMarkers(char *preProcFileName)
{
    string ppMapName(preProcFileName);
    size_t ppMapExtension = ppMapName.rfind(".");
    gotPPMap = false;

    if (ppMapExtension < ppMapName.npos)
    {
      ppMapName.replace(ppMapExtension, ppMapName.npos - ppMapExtension + 1, ".ppmap");

      string ppLine;
      ifstream ppMapFile (ppMapName.c_str());
      ostringstream ppMapSource;

      // Read input source file
      if (ppMapFile.is_open())
      {
        while (ppMapFile.good())
        {
          getline(ppMapFile, ppLine);
          ppMapSource << ppLine;
        }

        ppMapFile.close();

        gotPPMap = true;

        ostringstream jsonErr;
        Json n(&jsonErr);
        try
        {
          n.ParseJson(ppMapSource.str());
        }
        catch(int i)
        {
          *vampErr << "Bad preprocessor map file " << ppMapName << " - " << jsonErr.str();
          gotPPMap = false;
        }

        if (gotPPMap)
        {
            try
            {
              // Now extract map info
              parseJsonMap(n);
              gotPPMap = true;
            }
            catch(int i)
            {
              gotPPMap = false;
            }
        }
      }
    }
}

bool VampProcess::processFile(char *jsonName, VAMP_REPORT_CONFIG &vo)
{
  totalMcdcCombCnt = 0;
  sourceCount = 0;              // Number of lines in source
  totalStmtCoveredCount = 0;    // Total # stmts covered in source
  newStmtCoveredCount = 0;      // # new stmts covered in current run
  mcdcTotalOperandCount = 0;    // # MCDC operands in source code
  mcdcTotalCoverageCount = 0;   // # MCDC operands covered in source
  mcdcNewCoverCount = 0;        // # new MCDC operands covered in current run
  branchTotalOperandCount = 0;  // # Branch operands in source code
  branchTotalCoverageCount = 0; // # Branch operands covered in source
  newBranchCoveredCount = 0;    // # new branches covered in current run
  condTotalOperandCount = 0;    // # Conditions in source code
  condTotalCoverageCount = 0;   // # Conditions covered in source
  newCondCoveredCount = 0;      // # new conditions covered in current run

  string line;
  ifstream jsonFile (jsonName);
  ostringstream jsonSource;

  db.SetVampOptions(vo);

  // Read input source file
  if (jsonFile.is_open())
  {
//    *vampOut << "Processing " << jsonName << ENDL;

    while (jsonFile.good())
    {
      getline(jsonFile, line);
      jsonSource << line;
    }
    jsonFile.close();

#ifdef VAMP_DEBUG_SRC
    string src = jsonSource.str();
    CDBG << src << ENDL;
#endif
  }
  else
  {
    *vampErr << "Unable to open file: " << jsonName << ENDL;
    return false;
  }

#ifdef CHANGE_FNAME
  size_t extension = jsonName.rfind(".");

  if (extension < jsonName.npos)
  {
    jsonName.replace(extension, jsonName.npos - extension + 1, "_vamp.json");
    *vampOut << "Processing " << jsonName << ENDL;
  }
  else
  {
    *vampErr << "Cannot parse " << jsonName << ": Not a C extension" << ENDL;
  }
#endif


  ostringstream jsonErr;
  Json n(&jsonErr);
  try
  {
    n.ParseJson(jsonSource.str());
  }
  catch(int i)
  {
    *vampErr << "Bad database file " << jsonName << " - " << jsonErr.str();
    return false;
  }

  jsonSource.str("");

  try
  {
    db.parseJsonDb(n, vampErr);
  }
  catch(int i)
  {
    return false;
  }

  // Get time of creation (modification) for database file as string
  struct stat jsonStat;
  if (stat(jsonName, &jsonStat) == -1)
  {
    *vampErr << "Cannot stat database file " << jsonName << " - aborting" << ENDL;
    return false;
  }
  string jsonModTime = modTimeStr(jsonStat.st_mtime);

  string fName = db.pathName + DIRECTORY_SEPARATOR + db.srcFileName;

  if (!readSource(fName))
  {
    return false;
  }

  // Get time of creation (modification) for source file as string
  struct stat fileStat;
  if (stat(fName.c_str(), &fileStat) == -1)
  {
    *vampErr << "Cannot stat source file " << fName << " - aborting" << ENDL;
    return false;
  }
  string modTime = modTimeStr(fileStat.st_mtime);
  if (jsonModTime < modTime)
  {
    *vampErr << "Error - database file " << jsonName <<
                " older than source file " << fName << " - aborting" << ENDL;
    return false;
  }

  string instrFile = db.instrPathName + "/" + db.instrFileName;
  // Get time of creation (modification) for database file as string
  struct stat instrStat;
  if (stat(instrFile.c_str(), &instrStat) == -1)
  {
    *vampErr << "Cannot stat instrumented file " << instrFile << " - aborting" << ENDL;
    return false;
  }
  string instrModTime = modTimeStr(instrStat.st_mtime);
  if (instrModTime != db.instrModTime)
  {
    *vampErr << "Error - instrumented file " << instrFile <<
                " creation time does not match database file " << jsonName << " - aborting" << ENDL;
    return false;
  }

  // Init offset into MCDC table
  mcdcOpCnt.push_back(0);

  // Build filename for history file (<file>.c -> <file>.hist)
  string historyName(db.fileName);
  size_t extension = historyName.rfind(".");

// FIXME: Add histDirectory and histSuffix
//        Add CheckDir to see if directory exists and if not, create it.
  // Build filename for combined history file (<file>.c -> <file>.cmbhist)
  string combHistoryName(db.fileName);

  if (extension < historyName.npos)
  {
    historyName.replace(extension, historyName.npos - extension + 1, ".hist");
    combHistoryName.replace(extension,
                            combHistoryName.npos - extension + 1,
                            ".cmbhist");
  }
  else
  {
    // FIXME: Error message here?
    historyName += ".hist";
    combHistoryName += ".cmbhist";
  }

  db.outputCombinedHistory = db.combineHistory;

  string histDir;
#ifdef _WIN32
  if ((db.histDirectory[0] == '\\') ||
      (db.histDirectory[0] == '/') ||
      (db.histDirectory[1] == ':'))
#else
  if (db.histDirectory[0] == '/')
#endif
  {
    // Absolute path - use it
    histDir = db.histDirectory;
  }
  else
  {
    // Relative path - append it to instrumentation path
    // 
    histDir = db.instrPathName + "/" + db.histDirectory;
  }

  //string histName = db.histDirectory + "/" + historyName;
  //string combHistName = db.histDirectory + "/" + combHistoryName;
  string histName = histDir + "/" + historyName;
  string combHistName = histDir + "/" + combHistoryName;

  if (db.combineHistory)
  {
    *vampOut << "Reading combined history file: " << combHistoryName << ENDL;
    //if (!oldHist.loadHistory(db.histDirectory, combHistoryName, vampErr))
    if (!oldHist.loadHistory(combHistName, vampErr))
    {
      db.combineHistory = false;
    }
    else
    {
      if (oldHist.modTime < db.instrModTime)
      {
// FIXME - Look for option in vamp_process.cfg to handle this problem
// Decide to exit, delete or rename combined history file
        *vampErr << "Warning - combined history file " << combHistName <<
                    " older than instrumented source " << instrFile <<
                    " - ignoring" << ENDL;
        db.combineHistory = false;
      }
      else
      {
        *vampOut << "Reading new history file: " << histName << ENDL;
        //if (!newHist.loadHistory(db.histDirectory, historyName, vampErr))
        if (!newHist.loadHistory(histName, vampErr))
        {
          return false;
        }

        if (oldHist.coverageOptions != newHist.coverageOptions)
        {
          *vampErr << "Coverage options do not match between files " <<
                      histName << " and " << combHistName << " - aborting" << ENDL;
          return false;
        }

        if (newHist.modTime < db.instrModTime)
        {
          *vampErr << "New history file " << histName <<
                      " older than instrumented source " <<
                      instrFile << " - aborting" << ENDL;
          return false;
        }

        hist.coverageOptions = oldHist.coverageOptions;
        hist.instCount = oldHist.instCount;
        hist.branchCount = oldHist.branchCount;
        hist.mcdcCount = oldHist.mcdcCount;
        hist.condCount = oldHist.condCount;
        hist.stackOverflow = oldHist.stackOverflow | newHist.stackOverflow;

        if (hist.coverageOptions & DO_STATEMENT_COUNT)
        {
          hist.covCntInfo = (unsigned int *) malloc(hist.instCount * 4);

          // Save largest of old and new as combined count
          for (int i = 0; i < hist.instCount; i++)
          {
            hist.covCntInfo[i] =
               (oldHist.covCntInfo[i] > newHist.covCntInfo[i]) ?
                oldHist.covCntInfo[i] : newHist.covCntInfo[i];
          }
        }
        else
        if (hist.coverageOptions & (DO_STATEMENT_SINGLE | DO_BRANCH))
        {
          // May exist for branch coverage of case statements
          hist.coveredInfo = (unsigned char *) malloc(hist.instCount);

          // Combine old and new statement coverage bit info
          for (int i = 0; i < hist.instCount; i++)
          {
            hist.coveredInfo[i] = oldHist.coveredInfo[i] | newHist.coveredInfo[i];
          }
        }

        if (hist.coverageOptions & DO_BRANCH)
        {
          hist.brInfo = (unsigned char *) malloc(hist.branchCount);

          // Combine old and new branch bit info
          for (int i = 0; i < hist.branchCount; i++)
          {
            hist.brInfo[i] = oldHist.brInfo[i] | newHist.brInfo[i];
          }
        }

        if (hist.coverageOptions & DO_MCDC)
        {
          hist.mcdcInfo = (unsigned char *) malloc(hist.mcdcCount);

          // Clear MC/DC info for now; combine it later
          for (int i = 0; i < hist.mcdcCount; i++)
          {
            hist.mcdcInfo[i] = 0;
          }
        }
        else
        if (hist.coverageOptions & DO_CONDITION)
        {
          hist.condInfo = (unsigned char *) malloc(hist.condCount);

          // Combine old and new condition bit info
          for (int i = 0; i < oldHist.condCount; i++)
          {
            hist.condInfo[i] = oldHist.condInfo[i] | newHist.condInfo[i];
          }
        }
      }
    }
  }

  if (!db.combineHistory)
  {
    *vampOut << "Reading history file: " << historyName << ENDL;
    //if (!hist.loadHistory(db.histDirectory, historyName, vampErr))
    if (!hist.loadHistory(histName, vampErr))
    {
      return false;
    }

    if (hist.modTime < db.instrModTime)
    {
      *vampErr << "History file " << histName << " older than instrumented source " <<
                  instrFile << " - aborting" << ENDL;
      return false;
    }
  }

  // Determine coverage options
  doStmtSingle = hist.coverageOptions & DO_STATEMENT_SINGLE;
  doStmtCount = hist.coverageOptions & DO_STATEMENT_COUNT;
  doBranch = hist.coverageOptions & DO_BRANCH;
  doMCDC = hist.coverageOptions & DO_MCDC;
  doCC = hist.coverageOptions & DO_CONDITION;

  // Process statement coverage info
  // Perform regardless of whether coverage exists
  try
  {
    processStmt();
  }
  catch(int err)
  {
    *vampOut << "Corrupt history file\n";
    return false;
  }

  if (doBranch)
  {
    // Process branch coverage info
    processBranch();
  }

  if (doMCDC)
  {
    // Process MC/DC coverage info
    processMCDC();
  }

  if (doCC)
  {
    // Process condition coverage info
    processCondition();
  }

#ifdef VAMP_DEBUG_SHOW_ATTRIBS
  for (int i = 0; i < source.size(); i++)
  {
    CDBG << source[i] << ENDL;
    CDBG << attribs[i] << ENDL;
  }
#endif

  // Build filename for HTML file (<file>.json -> <file>.html)
#ifdef OLD_WAY
  string htmlName(jsonName);
  extension = htmlName.rfind(".");

  if (extension < htmlName.npos)
  {
    htmlName.replace(extension, htmlName.npos - extension + 1, ".html");
  }
  else
  {
    htmlName += ".html";
  }
#else
  string htmlName(db.fileName);
  extension = htmlName.rfind(".");

  if (extension < htmlName.npos)
  {
    htmlName = htmlName.substr(0, extension);
  }

  // See if directory exists and if not, create it.
  struct stat statInfo;
  string htmlDir;
#ifdef _WIN32
  if ((db.htmlDirectory[0] == '\\') ||
      (db.htmlDirectory[0] == '/') ||
      (db.htmlDirectory[1] == ':'))
#else
  if (db.htmlDirectory[0] == '/')
#endif
  {
      // Absolute path - use it
      htmlDir = db.htmlDirectory;
  }
  else
  {
      // Relative path - append it to instrumentation path
      htmlDir = db.instrPathName + "/" + db.htmlDirectory;
  }

  if (stat(htmlDir.c_str(), &statInfo) == -1)
  {
    *vampErr << "Attempting to create directory " << db.htmlDirectory << ENDL;
/*
#ifdef _WIN32
    if (CreateDirectory(db.htmlDirectory.c_str(), NULL) == 0)
    {
      *vampErr << "CreateDirectory failed for " << db.htmlDirectory << ENDL;
      return false;
    }
#else
*/
#ifdef _WIN32
    if (mkdir(htmlDir.c_str()) == -1)
#else
    if (mkdir(htmlDir.c_str(), 0777) == -1)
#endif
    {
//      perror(htmlDir.c_str());
      *vampErr << "Cannot create directory" << htmlDir << ENDL;
      return false;
    }
  }

  htmlName = htmlDir + DIRECTORY_SEPARATOR + htmlName + db.htmlSuffix;
  //htmlName = htmlDir + DIRECTORY_SEPARATOR + htmlName +
  //           db.htmlSuffix + ".html";
#endif

  *vampOut << "Generating " << htmlName << ".html" << ENDL;
  genHTML(htmlName);
/*
  size_t extension = fileName.rfind(".");

  if (extension < fileName.npos)
  {
    string htmlName = fileName.substr(extension) + ".html";
    //fileName.replace(extension, fileName.npos - extension + 1, "_vamp.json");
    *vampOut << "Generating " << htmlName << ENDL;
  }
  else
  {
    *vampErr << "Cannot parse " << fileName << ": Not a C extension" << ENDL;
  }
*/

  if (db.outputCombinedHistory)
  {
    string combName = db.histDirectory + "/" + combHistoryName;
#ifdef USE_LLVM
    llvm::raw_fd_ostream *outFile;
    string OutErrorInfo;

    // Output combined history info
    *vampOut << "Generating combined history file: " << combName << ENDL;

    outFile = new llvm::raw_fd_ostream(combName.c_str(), OutErrorInfo, 0);

    if (!OutErrorInfo.empty())
    {
      llvm::errs() << "Cannot open " << combName << " for writing\n";
      return false;
    }

    // Send coverage options
    outFile << hist.coverageOptions;

    if (doStmtCount)
    {
      // Send instrumented statement count
      outFile << (hist.instCount >> 8);
      outFile << (hist.instCount & 0xff);
      for (int i = 0; i < hist.instCount; i++)
      {
        outFile << (hist.covCntInfo[i] >> 24);
        outFile << (hist.covCntInfo[i] >> 16) & 0xff;
        outFile << (hist.covCntInfo[i] >> 8) & 0xff;
        outFile << hist.covCntInfo[i] & 0xff;
      }
    }
    else
    if (doStmtSingle || doBranch)
    {
      // Send instrumented statement count
      outFile << (hist.instCount >> 8);
      outFile << (hist.instCount & 0xff);
      for (int i = 0; i < hist.instCount; i++)
        outFile << hist.coveredInfo[i];
    }

    if (doBranch)
    {
      // Send branch info count
      outFile << (hist.branchCount >> 8);
      outFile << (hist.branchCount & 0xff);
      for (int i = 0; i < hist.branchCount; i++)
        outFile << hist.brInfo[i];
    }

    if (doMCDC)
    {
      // Send MC/DC info count
      outFile << (hist.mcdcCount >> 8);
      outFile << (hist.mcdcCount & 0xff);
      for (int i = 0; i < hist.mcdcCount; i++)
        outFile << hist.mcdcInfo[i];

      outFile << ((hist.stackOverflow >> 8) & 0xff);
      outFile << (hist.stackOverflow & 0xff);
    }
    else
    if (doCC)
    {
      // Send condition info count
      outFile << (hist.condCount >> 8);
      outFile << (hist.condCount & 0xff);
      for (int i = 0; i < hist.condCount; i++)
        outFile << hist.condInfo[i];
    }
#else
    FILE *fd;

    // Output combined history info
    *vampOut << "Generating combined history file: " << combName << ENDL;

    fd = fopen(combName.c_str(), "wb");
    if (fd == NULL)
    {
      *vampErr << "Could not open " << combName << " for output" << ENDL;
      return false;
    }

    // Send coverage options
    fputc(hist.coverageOptions, fd);

    if (doStmtCount)
    {
      // Send instrumented statement count
      fputc((hist.instCount >> 8), fd);
      fputc((hist.instCount & 0xff), fd);
      for (int i = 0; i < hist.instCount; i++)
      {
        fputc(hist.covCntInfo[i] >> 24, fd);
        fputc((hist.covCntInfo[i] >> 16) & 0xff, fd);
        fputc((hist.covCntInfo[i] >> 8) & 0xff, fd);
        fputc(hist.covCntInfo[i] & 0xff, fd);
      }
    }
    else
    if (doStmtSingle || doBranch)
    {
      // Send instrumented statement count
      fputc((hist.instCount >> 8), fd);
      fputc((hist.instCount & 0xff), fd);
      for (int i = 0; i < hist.instCount; i++)
        fputc(hist.coveredInfo[i], fd);
    }

    if (doBranch)
    {
      // Send branch info count
      fputc((hist.branchCount >> 8), fd);
      fputc((hist.branchCount & 0xff), fd);
      for (int i = 0; i < hist.branchCount; i++)
        fputc(hist.brInfo[i], fd);
    }

    if (doMCDC)
    {
      // Send MC/DC info count
      fputc((hist.mcdcCount >> 8), fd);
      fputc((hist.mcdcCount & 0xff), fd);
      for (int i = 0; i < hist.mcdcCount; i++)
        fputc(hist.mcdcInfo[i], fd);

      fputc(((hist.stackOverflow >> 8) & 0xff), fd);
      fputc((hist.stackOverflow & 0xff), fd);
    }
    else
    if (doCC)
    {
      // Send condition info count
      fputc((hist.condCount >> 8), fd);
      fputc((hist.condCount & 0xff), fd);
      for (int i = 0; i < hist.condCount; i++)
        fputc(hist.condInfo[i], fd);
    }

    fclose(fd);
#endif
  }
  return true;
}


// Load source file <fileName> into string vector
bool VampProcess::readSource(string fileName)
{
  string line;
  ifstream sourceFile (fileName.c_str());
  vector<string>::iterator i;
  bool result;

  // Read input source file
  if (sourceFile.is_open())
  {
    while ( sourceFile.good() )
    {
      string lineAttr;

      getline(sourceFile, line);
      source.push_back(line);

      // Create attribute line of all nulls
      lineAttr.append(line.length(), 0);
      attribs.push_back(lineAttr);
      ++sourceCount;
    }
    sourceFile.close();

#ifdef VAMP_DEBUG_SRC
    for (i = source.begin(); i < source.end(); i++)
      CDBG << *i << ENDL;
#endif

    result = true;
  }
  else
  {
    *vampErr << "Unable to open input file: " << fileName << ENDL;

    result = false;
  }

  return result;
}


// Return a string of source code from [loc.lhsLine, loc.lhsCol] to
//                                     [loc.rhsLine, loc.rhsCol]
string VampProcess::sourceText(sourceLocationType &loc)
{
  string str;
  int lhsLine = loc.lhsLine;
  int lhsCol = loc.lhsCol;

  while (lhsLine < loc.rhsLine)
  {
    str += source[lhsLine - 1].substr(lhsCol - 1);
    ++lhsLine;
    lhsCol = 1;
  }
  str += source[lhsLine - 1].substr(lhsCol - 1, loc.rhsCol - lhsCol + 1);

  return str;
}

// Return a vector of strings of source code from [loc.lhsLine, loc.lhsCol] to
//                                                [loc.rhsLine, loc.rhsCol]
// Indent each new line with indent spaces
void VampProcess::genHtmlSourceText(sourceLocationType &loc,
                                    int indent,
                                    vector<string> &strings)
{
  string str;
  int lhsLine = loc.lhsLine;
  int lhsCol = loc.lhsCol;

  while (lhsLine < loc.rhsLine)
  {
    while (source[lhsLine - 1][lhsCol - 1] <= 0x20)
    {
      if (lhsCol++ >= source[lhsLine - 1].length())
      {
        // Ignore blank lines entirely
        ++lhsLine;
        lhsCol = 1;
      }
    }
        
    // Works if line not blank...  Compare lhsCol to rhsCol...
    str += source[lhsLine - 1].substr(lhsCol - 1);
    ++lhsLine;
    strings.push_back(str);
    str = "";
    if (indent)
      str.insert(0, indent, ' ');
    lhsCol = 1;
  }

  if (lhsLine <= loc.rhsLine)
  {
    while ((lhsCol <= loc.rhsCol) && (source[lhsLine - 1][lhsCol - 1] <= 0x20))
    {
      lhsCol++;
    }
    if (lhsCol < loc.rhsCol)
    {
      str += source[lhsLine - 1].substr(lhsCol - 1, loc.rhsCol - lhsCol + 1);
    }
  }
  strings.push_back(str);
}

// Return a string of source code from [loc.lhsLine, loc.lhsCol] to
//                                     [loc.rhsLine, loc.rhsCol]
// Add <br /> at line breaks, and indent next line indent spaces
string VampProcess::htmlSourceText(sourceLocationType &loc, int indent)
{
  vector<string> strings;
  //vector<string>::iterator s;
  string str;

  genHtmlSourceText(loc, indent, strings);
  //for (s = strings.begin(); s != strings.end(); s++)
  for (int i = 0; i < strings.size(); i++)
  {
    //str += *s;
    str += strings[i];
    if ((i + 1) != strings.size())
      str += "<br />";
  }

  return str;
}

// Walk through MC/DC tree node to add to list for given leaf
void VampProcess::walkTreeNode(int &nodeCnt,
                               mcdcNode *node,
                               bool doRHS,
                               vector<sourceLocationType> &nodeInfo)
{
  // Label for each of up to 31 operands in expression
  //string opStr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcde");
  sourceLocationType loc;

  if (doRHS)
  {
    loc.lhsLine = node->rhsMinLine;
    loc.lhsCol  = node->rhsMinCol;
    loc.rhsLine = node->rhsMaxLine;
    loc.rhsCol  = node->rhsMaxCol;
  }
  else
  {
    loc.lhsLine = node->lhsMinLine;
    loc.lhsCol  = node->lhsMinCol;
    loc.rhsLine = node->lhsMaxLine;
    loc.rhsCol  = node->lhsMaxCol;
  }

  // Add Node name (A-Za-e) and source code associated with it
//  nodeInfo << "  " << opStr[nodeCnt] << " = " << sourceText(loc) << ENDL;
  nodeInfo.push_back(loc);
  ++nodeCnt;
}

// Walk through MC/DC tree to build a list of each leaf
void VampProcess::walkTree(int &nodeCnt,
                           mcdcNode *node,
                           vector<sourceLocationType> &nodeInfo)
{
  if (node->lhs == NULL)
    walkTreeNode(nodeCnt, node, 0, nodeInfo);
  else
    walkTree(nodeCnt, node->lhs, nodeInfo);

  if (node->rhs == NULL)
    walkTreeNode(nodeCnt, node, 1, nodeInfo);
  else
    walkTree(nodeCnt, node->rhs, nodeInfo);
}

// Check for match between string mcdcStr and strings in testStr
// Treat '-' in each testStr as a wild card.
// If checkBoth == true, treat '-' in mcdcStr as a wild card, too.
int VampProcess::checkMatch(string &mcdcStr,
                            vector<string> &testStr,
                            //bool checkBoth = false)
                            bool checkBoth)
{
  vector<string>::iterator n;
  const char *mStr = mcdcStr.c_str();
  int len = mcdcStr.length();

  int which = 0;
  for (n = testStr.begin(); n < testStr.end(); n++, which++)
  {
    const char *tStr = n->c_str();
    int i;
    for (i = 0; (i < len) &&
                ((mStr[i] == tStr[i]) || (tStr[i] == '-') ||
                 (checkBoth && (mStr[i] == '-'))); i++) ;
    if (i == len)
      return which;
  }

  return -1;
}

// Set specified source code attribute for given range
void VampProcess::setAttrib(unsigned char attrib, sourceLocationType &range)
{
  int lhsLine = range.lhsLine;
  int lhsCol = range.lhsCol;

  // Handle lines before current line
  while (lhsLine < range.rhsLine)
  {
    // FIXME - with corrupt source, lhsLine can be greater than attribs.size()
    if (lhsLine > attribs.size())
        throw(1);

    while (lhsCol <= attribs[lhsLine - 1].length())
    {
      attribs[lhsLine - 1][lhsCol - 1] |= attrib;
      lhsCol++;
    }
    lhsCol = 0;
    lhsLine++;
  }

  // Handle current line
  if (lhsLine == range.rhsLine)
  {
    while (lhsCol <= range.rhsCol)
    {
      attribs[lhsLine - 1][lhsCol - 1] |= attrib;
      lhsCol++;
    }
  }
}

// Process statement coverage information
void VampProcess::processStmt(void)
{
  vector<functionInfoType>::iterator func = db.functionInfo.begin();
  vector<sourceLocationType>::iterator stmt = db.statementInfo.begin();
  int curLocLine = 0;

  // First set attribute flag for all statements
  // Walk through statementInfo and set STMT_CODE attribute for
  // entire range of each statement
  // Also set STMT_NEW_STMT attribute at start of a new statement that
  // is on the same line as another statement (e.g. i = 0; j = 1;)
  //                                 set attribute here----^
  // Also set STMT_NEW_LINE attribute for statements that exceed
  // MAX_COLUMN characters.  Scan back up to 10 characters for a blank
  // and set attribute there for cleaner line breaks.
  while ((func < db.functionInfo.end()) && (stmt < db.statementInfo.end()))
  {
#ifdef CHECK_OUTSIDE_FUNC
    // Ignore all lines not inside of function
    // (external declarations with initializers)
    // Maybe these shouldn't be in the .json file...
    while ((stmt->lhsLine < func->loc.lhsLine) ||
           ((stmt->lhsLine == func->loc.lhsLine) &&
            (stmt->lhsCol < func->loc.lhsCol)))
    {
#ifdef VAMP_DEBUG_STMT
      CDBG << "Ignoring statement at line " << stmt->lhsLine << ", " << stmt->lhsCol << ENDL;
#endif
      stmt++;

      stmtCount--;
    }
#endif

    while ((stmt < db.statementInfo.end()) &&
           ((stmt->lhsLine < func->loc.rhsLine) ||
            ((stmt->lhsLine == func->loc.rhsLine) &&
             (stmt->lhsCol < func->loc.rhsCol))))
    {
      int minColumn = 0;

      if (stmt->lhsLine == curLocLine)
      {
        // Current statement on same line as last statement
        // Set attribute to move it to new line
        attribs[curLocLine - 1][stmt->lhsCol - 1] |= STMT_NEW_STMT;

        minColumn = stmt->lhsCol - 1;
      }

      // Check for any long lines
      while ((stmt->rhsCol - minColumn) > MAX_COLUMN)
      {
        minColumn += MAX_COLUMN;

        // Break on a space if within 10 characters of MAX_COLUMN
        int tmpColumn = source[stmt->lhsLine - 1].rfind(' ', minColumn - 1);
        if ((minColumn - tmpColumn) < 10)
          minColumn = tmpColumn;
        attribs[stmt->lhsLine - 1][minColumn] |= STMT_NEW_LINE;
        //attribs[stmt->lhsLine - 1][minColumn - 1] |= STMT_NEW_LINE;
      }

#ifdef OLD_BLOCK_STMT
      sourceLocationType s = *stmt;

      // Check if next statement's beginning is before
      // current statement's end, as is the case with, for example,
      // for, switch and do statements
      if ((stmt != statementInfo.end()) &&
          ((s.rhsLine > (stmt + 1)->lhsLine) ||
           ((s.rhsLine == (stmt + 1)->lhsLine) &&
            (s.rhsCol > (stmt + 1)->lhsCol))))
      {
        // Yes - only set attributes up to start of next line
        s.rhsLine = (stmt + 1)->lhsLine;
        s.rhsCol = (stmt + 1)->lhsCol;
      }

      // Flag statement as code
      setAttrib(STMT_CODE, s);
#else
      // Flag statement as code
      setAttrib(STMT_CODE, *stmt);
#endif

      curLocLine = stmt->lhsLine;
      stmt++;
    }

    func++;
  }

  if (doStmtSingle || doStmtCount)
  {
    // Now set code coverage attribute

    int coveredCount = 0;      // Number of lines that had coverage

    func = db.functionInfo.begin();
    stmt = db.statementInfo.begin();

    for (int inst = 0; inst < ((int) db.instInfo.size() - 1); inst++)
    {
      // Skip function till we find the one inst is within
      while ((func < db.functionInfo.end()) &&
             ((db.instInfo[inst].line > func->loc.rhsLine) ||
              ((db.instInfo[inst].line == func->loc.rhsLine) &&
               (db.instInfo[inst].col  >  func->loc.rhsCol))))
      {
        // Save statistics for report
#ifdef VAMP_DEBUG_STMT
CDBG << "Pushing count " << coveredCount << ENDL;
CDBG << "Inst: " << db.instInfo[inst].line << ", " << db.instInfo[inst].col << ENDL;
CDBG << "Func: " << func->loc.rhsLine << ", " << func->loc.rhsCol << ENDL;
#endif
        functionStmtCoverageCount.push_back(coveredCount);
        totalStmtCoveredCount += coveredCount;
        coveredCount = 0;

        ++func;
      }

      if (func < db.functionInfo.end())
      {
        int lastLine, lastCol;
        if ((db.instInfo[inst + 1].line > func->loc.rhsLine) ||
            ((db.instInfo[inst + 1].line == func->loc.rhsLine) &&
             (db.instInfo[inst + 1].col  >  func->loc.rhsCol)))
        {
          lastLine = func->loc.rhsLine;
          lastCol = func->loc.rhsCol;
        }
        else
        {
          lastLine = db.instInfo[inst + 1].line;
          lastCol = db.instInfo[inst + 1].col;
        }

        int index = inst >> 3;
        int mask = 1 << (inst & 7);

        if (index > hist.instCount)
        {
          *vampErr << "StmtInfo Overflow! " << index << "," <<
                   hist.instCount << ENDL;
          throw(1);
        }

        // See if line hit
        if ((doStmtCount && hist.covCntInfo[inst]) ||
            (doStmtSingle && (hist.coveredInfo[index] & mask)))
// FIXME: Do we need to deal with doBranch here? Don't think so!
//            ((doStmtSingle || doBranch) && (hist.coveredInfo[index] & mask)))
        {
          // Yes - Set attributes for this line on to next instrumented
          // statement
          int line = db.instInfo[inst].line;
          int col = db.instInfo[inst].col;

          // Find statement info for this line
          while ((stmt < db.statementInfo.end()) &&
                 ((stmt->lhsLine < line) ||
                   ((stmt->lhsLine == line) && (stmt->lhsCol < col))))
          {
            ++stmt;
          }

/* FIXME- May be end of file?
          if (stmt == db.statementInfo.end())
          {
            *vampErr << "Instrumented Statement not found! " << line << "," <<
                     col << " - aborting" << ENDL;
            return false;
          }
*/

#ifdef VAMP_DEBUG_STMT
CDBG << "COVERED: " << line << "," << col << " to " <<
        db.instInfo[inst + 1].line << "," << db.instInfo[inst + 1].col << ENDL;
#endif

//CDBG << "Go till line " << instInfo[inst + 1].line << ENDL;
//          while (line < instInfo[inst + 1].line)
////CDBG << "Go till line " << lastLine << ENDL;
          while (line < lastLine)
          {
            if (line >= attribs.size())
            {
              *vampErr << "Bad line: " << line << ", " << attribs.size() << ENDL;
              throw(1);
            }

            while (col <= attribs[line - 1].length())
            {
              bool newStmt = false;
              if (attribs[line - 1][col - 1] & STMT_CODE)
              {
                attribs[line - 1][col - 1] |= STMT_COVERED;
                if (db.combineHistory)
                {
                  if (doStmtCount)
                  {
                    // Check if old history covered
                    if (oldHist.covCntInfo[inst])
                      attribs[line - 1][col - 1] |= STMT_COVERED_OLD;

                    // Check if new history covered
                    if (newHist.covCntInfo[inst] != 0)
                    {
                      attribs[line - 1][col - 1] |= STMT_COVERED_NEW;
                      // Flag if this is new coverage
                      if (oldHist.covCntInfo[inst] == 0)
                        newStmt = true;
                    }
                  }
                  else
// FIXME: Do we need to deal with doBranch here? Don't think so!
                  if (doStmtSingle)
                  {
                    // Check if old history covered
                    if (oldHist.coveredInfo[index] & mask)
                      attribs[line - 1][col - 1] |= STMT_COVERED_OLD;

                    // Check if new history covered
                    if (newHist.coveredInfo[index] & mask)
                    {
                      attribs[line - 1][col - 1] |= STMT_COVERED_NEW;
                      if (((oldHist.coveredInfo[index] &
                           newHist.coveredInfo[index]) ^
                           newHist.coveredInfo[index]) & mask)
                        newStmt = true;
                    }
                  }
                }
              }

              if ((line == stmt->lhsLine) && (col == stmt->lhsCol))
              {
                ++coveredCount;
//CDBG << "Covered " << line << ", " << col << "; " << coveredCount << ENDL;
                if (newStmt)
                  ++newStmtCoveredCount;
              }

              ++col;
              if ((line == stmt->rhsLine) && (col > stmt->rhsCol))
              {
                ++stmt;
              }
            }

            ++line;
            col = 1;
            if ((line > stmt->rhsLine) ||
                ((line == stmt->rhsLine) && (col > stmt->rhsCol)))
            {
              ++stmt;
            }
          }

          if (line >= attribs.size())
          {
            *vampErr << "Bad line: " << line << ", " << attribs.size() << ENDL;
            throw(1);
          }

          //while (col < instInfo[inst + 1].col)
          while (col < lastCol)
          {
            bool newStmt = false;
            if (attribs[line - 1][col - 1] & STMT_CODE)
            {
              attribs[line - 1][col - 1] |= STMT_COVERED;
              ////lineCovered = true;

              if (db.combineHistory)
              {
                if (doStmtSingle)
                {
                  // Check if old history covered
                  if (oldHist.coveredInfo[index] & mask)
                    attribs[line - 1][col - 1] |= STMT_COVERED_OLD;

                  // Check if new history covered
                  if (newHist.coveredInfo[index] & mask)
                  {
                    attribs[line - 1][col - 1] |= STMT_COVERED_NEW;
                    if (((oldHist.coveredInfo[index] &
                         newHist.coveredInfo[index]) ^
                         newHist.coveredInfo[index]) & mask)
                      newStmt = true;
                  }
                }
                else
                {
                  // Check if old history covered
                  if (oldHist.covCntInfo[inst])
                    attribs[line - 1][col - 1] |= STMT_COVERED_OLD;

                  // Check if new history covered
                  if (newHist.covCntInfo[inst] != 0)
                  {
                    attribs[line - 1][col - 1] |= STMT_COVERED_NEW;
                    // Flag if this is new coverage
                    if (oldHist.covCntInfo[inst] == 0)
                      newStmt = true;
                  }
                }
              }
            }

            if ((line == stmt->lhsLine) && (col == stmt->lhsCol))
            {
//CDBG << "Covered " << line << ", " << col << "; " << coveredCount << ENDL;
              ++coveredCount;
              if (newStmt)
                ++newStmtCoveredCount;
            }

            ++col;
            if ((line == stmt->rhsLine) && (col > stmt->rhsCol))
            {
              ++stmt;
            }
          }
        }
      }
      else
      {
        *vampErr << "Statement found outside of function -- Corrupt history file" << ENDL;
        throw(1);
      }
    }

    // Save statistics for report
    functionStmtCoverageCount.push_back(coveredCount);
    totalStmtCoveredCount += coveredCount;
#ifdef VAMP_DEBUG_STMT
    CDBG << "New statements covered = " << newStmtCoveredCount << ENDL;

    float coveragePercent = 100.0f * totalStmtCoveredCount / db.totalStmtCount;
    char perCent[32];
    sprintf(perCent, "%4.1f", coveragePercent);
    CDBG << totalStmtCoveredCount <<
            " statements covered of " << db.totalStmtCount << " total" << ENDL;
    CDBG << "Statement coverage: " << perCent << "%" << endl << ENDL;
#endif
  }
}

// Generate HTML string for background color
string VampProcess::htmlBgColor(int color)
{
  ostringstream oss;

  oss << "<span style=\"background: #" << std::hex << color << "\">";
  return oss.str();
}

// Generate HTML string for percentage covered
string VampProcess::htmlPercentageStyle(string name, int percent)
{
  ostringstream style;

  style << "<style type=\"text/css\">" << ENDL;
  style << '#' << name << " {" << ENDL;
  style << "border:2px solid blue;" << ENDL;
  style << "background:#ff6060;" << ENDL;
  style << "}" << ENDL;
  style << '#' << name << "_in {" << ENDL;
  style << "width:" << percent << "%;" << ENDL;
  style << "font-size:12pt;" << ENDL;
  style << "background:#60ff60;" << ENDL;
  style << "}" << ENDL;
  style << "</style>" << ENDL;

  return style.str();
}

// Determine minimum number of additional independent MC/DC conditions
// needed to complete MC/DC coverage
int VampProcess::getMinCond(vector<mcdcPair> *mcdcPairs,
                            int mcdcPairIndex,
                            bitset<MAX_BITSET> &falseConditions,
                            bitset<MAX_BITSET> &trueConditions)
{
  int minValue = 65535;
  vector<mcdcPair>::iterator pair = mcdcPairs[mcdcPairIndex].begin();
  bitset<MAX_BITSET> minFalse, minTrue;
  bitset<MAX_BITSET> thisFalse, thisTrue;
  unsigned int thisMin;

  // Recursively walk through each possible independent pair and
  // compute number of additional conditions needed for that pair.
  // Determine which of the independent pairs require the fewest number.
  while (pair < mcdcPairs[mcdcPairIndex].end())
  {
    // Determine if there are any pairs below this level.
    // If so, add the next level's pair count.
//FIXME - What is this line doing?
    bool lastPair = (mcdcPairs[mcdcPairIndex + 1].size() == 0);
    //bool lastPair = ((mcdcPairIndex + 1) >= mcdcPairs->size());

    thisFalse = falseConditions;
    thisTrue = trueConditions;

    if (falseConditions.test(pair->f))
    {
      if (trueConditions.test(pair->t))
      {
        // Got both conditions already
        if (lastPair)
          thisMin = 0;
        else
          thisMin = getMinCond(mcdcPairs, mcdcPairIndex + 1,
                               thisFalse, thisTrue);
      }
      else
      {
        // Add True condition of pair
        thisTrue.set(pair->t);

        if (lastPair)
          thisMin = 1;
        else
          thisMin = 1 + getMinCond(mcdcPairs, mcdcPairIndex + 1,
                                   thisFalse, thisTrue);
      }
    }
    else
    if (trueConditions.test(pair->t))
    {
      // Add False condition of pair
      thisFalse.set(pair->f);

      if (lastPair)
        thisMin = 1;
      else
      {
        thisMin = 1 + getMinCond(mcdcPairs, mcdcPairIndex + 1,
                                 thisFalse, thisTrue);
      }
    }
    else
    {
      // Add both False and True conditions of pair
      thisFalse.set(pair->f);
      thisTrue.set(pair->t);

      if (lastPair)
        thisMin = 2;
      else
        thisMin = 2 + getMinCond(mcdcPairs, mcdcPairIndex + 1,
                                 thisFalse, thisTrue);
    }

    if (thisMin < minValue)
    {
      minValue = thisMin;
      minFalse = thisFalse;
      minTrue = thisTrue;
    }

    ++pair;
  }

  falseConditions = minFalse;
  trueConditions = minTrue;
  return minValue;
}


// Generate HTML for branch or condition coverage (e.g.  "Covered  Not Covered")
// Return number of conditions covered (0-3).
// Append HTML to funcHTML.
int VampProcess::genCoverageHTML(int covered,
                                 bool isBranch,
                                 sourceLocationType &s,
                                 ostringstream &funcHTML)
{
  int numCovered;

  if (covered == 3)
  {
    // Covered both TRUE and FALSE conditions
    if (isBranch)
      setAttrib(STMT_BR_COV_FULL, s);
    numCovered = 2;
                    //"  Covered      Covered  "
    funcHTML << "  " << htmlBgColor(BG_GREEN) <<
                "Covered</span>" <<
                "      " << htmlBgColor(BG_GREEN) <<
                "Covered</span>  ";
  }
  else
  if (covered == 2)
  {
    // Covered only TRUE condition
    if (isBranch)
      setAttrib(STMT_BR_COV_TRUE, s);
    numCovered = 1;
                    //"  Covered    Not Covered"
    funcHTML << "  " << htmlBgColor(BG_GREEN) <<
                "Covered</span>" <<
                "    " << htmlBgColor(BG_RED) <<
                "Not Covered</span>";
  }
  else
  if (covered == 1)
  {
    // Covered only FALSE condition
    if (isBranch)
      setAttrib(STMT_BR_COV_FALSE, s);
    numCovered = 1;
                    //"Not Covered    Covered  "
    funcHTML << htmlBgColor(BG_RED) <<
                "Not Covered</span>" <<
                "    " << htmlBgColor(BG_GREEN) <<
                "Covered</span>  ";
  }
  else
  {
    // Covered no conditions
    numCovered = 0;
                    //"Not Covered  Not Covered"
    funcHTML << htmlBgColor(BG_RED) <<
                "Not Covered</span>" <<
                "  " << htmlBgColor(BG_RED) <<
                "Not Covered</span>";
  }

  return numCovered;
}

// Process branch coverage info
void VampProcess::processBranch(void)
{
  vector<branchInfoType>::iterator br;
  vector<ifElseInfoType>::iterator ifIter = db.ifElseInfo.begin();
  vector<whileInfoType>::iterator whileIter = db.whileInfo.begin();
  vector<forInfoType>::iterator forIter = db.forInfo.begin();
  vector<switchInfoType>::iterator swIter = db.switchInfo.begin();
  vector<functionInfoType>::iterator func = db.functionInfo.begin();
  ostringstream branchFuncHTML;
  int branchFuncCoverCnt = 0;
  int branchFuncOperandCnt = 0;
  int branchCnt = 0;

  for (br = db.branchInfo.begin(); br < db.branchInfo.end(); br++)
  {
    int branchNum;
    sourceLocationType s;

    if ((br->statementType == "while") ||
        (br->statementType == "do"))
    {
      if (whileIter != db.whileInfo.end())
      {
        s = whileIter->exprLoc;
        branchNum = whileIter->branchNum;
      }
      else
      {
        *vampErr << "While branch not found - aborting" << ENDL;
        throw(1);
      }

      whileIter++;
    }
    else
    if (br->statementType == "for")
    {
      if (forIter != db.forInfo.end())
      {
        s = forIter->exprLoc;
        branchNum = forIter->branchNum;
      }
      else
      {
        *vampErr << "For branch not found - aborting" << ENDL;
        throw(1);
      }

      forIter++;
    }
    else
    if (br->statementType == "if")
    {
      if (ifIter != db.ifElseInfo.end())
      {
        s = ifIter->exprLoc;
        branchNum = ifIter->branchNum;
      }
      else
      {
        *vampErr << "If branch not found - aborting" << ENDL;
        throw(1);
      }

      ifIter++;
    }
    else
    if (br->statementType == "switch")
    {
      s = swIter->switchExprLoc;
    }
    else
    {
      *vampErr << "Unknown branch type " << br->statementType <<
              " - aborting" << ENDL;
      throw(1);
    }

    // Find function this expression is within
    while (s.lhsLine > func->loc.rhsLine)
    {
      // Save branch info for current function
      branchHTMLInfo.push_back(branchFuncHTML.str());

      // Save coverage statistics for computing percentages
      branchFunctionCoverageCount.push_back(branchFuncCoverCnt);
      branchFunctionOperandCount.push_back(branchFuncOperandCnt);
      branchTotalCoverageCount += branchFuncCoverCnt;
      branchTotalOperandCount += branchFuncOperandCnt;

      // Clear count for next function
      branchFuncCoverCnt = 0;
      branchFuncOperandCnt = 0;

      // Increment count of # branch functions calculated
      ++branchCnt;

      branchFuncHTML.str("");
      func++;
      if (func >= db.functionInfo.end())
      {
        *vampErr << "Branch expression outside function range - aborting" << ENDL;
        throw(1);
      }
    }

#ifdef VAMP_DEBUG_BRANCH
CDBG << "Branch " << branchNum << " type " << br->statementType << " at " << 
        func->function << " line " << s.lhsLine << ENDL;
#endif

    branchFuncHTML << "<table border bgcolor=\"#d0d0d0\" " <<
                      "width=80% align=\"center\">" << ENDL;
    //branchFuncHTML << "<tr><td colspan=2 bgcolor=#ffffa0>" << ENDL;
    //branchFuncHTML << "<tr><td colspan=2 bgcolor=#a0ffa0>" << ENDL;
    if (db.combineHistory)
      branchFuncHTML << "<tr><td colspan=3 bgcolor=#e0a0ff>" << ENDL;
    else
      branchFuncHTML << "<tr><td bgcolor=#e0a0ff>" << ENDL;
      //branchFuncHTML << "<tr><td colspan=2 bgcolor=#e0a0ff>" << ENDL;
    branchFuncHTML << "<center><b><font size=\"5\">" <<
                      "Branch Condition at " << func->function <<
                      "() <a href='#line_" <<
                      s.lhsLine << "'>line " <<
                      s.lhsLine << "</a>";
    if (s.rhsLine > s.lhsLine)
      branchFuncHTML << "-" << s.rhsLine;
    branchFuncHTML << ":</font>" << ENDL;
    branchFuncHTML << "<br /><br />" << ENDL;
    branchFuncHTML << "<font size=\"5\">" <<
                      htmlSourceText(s, 0) <<
                      "</font></b></center><br />" << ENDL;
    branchFuncHTML << "</td></tr>" << ENDL;

    if (br->statementType == "switch")
    {
      //branchFuncHTML << "<tr><td colspan=2 bgcolor=#a0a0ff>" << ENDL;
      branchFuncHTML << "<tr><td>" << ENDL;
      branchFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;

      // Count number of case statements covered
      int caseCnt = swIter->caseInfo.size();
      int caseCoveredCnt = 0;
      vector<caseInfoType>::iterator c;

      // Determine longest case string
      int maxCaseLen = 0;
      for (c = swIter->caseInfo.begin();
           c < swIter->caseInfo.end(); c++)
      {
        string str = sourceText(c->caseLoc);
        int len = str.length();
        if (len > maxCaseLen)
          maxCaseLen = len;
      }

      for (c = swIter->caseInfo.begin();
           c < swIter->caseInfo.end(); c++)
      {
        int instNum = c->instNum;
        bool covered;
        if (doStmtCount)
          covered = hist.covCntInfo[instNum] != 0;
        else
        if (doStmtSingle | doBranch)
          covered = (hist.coveredInfo[instNum >> 3] & (1 << (instNum & 7))) != 0;
/*
        else
// FIXME: Handle case statement coverage when no statement instrumentation
//        enabled!!!
          covered = false;
*/

        string str = sourceText(c->caseLoc);
        int spCnt = maxCaseLen - str.length() + 2;
        str.insert(str.length(), spCnt, ' ');
        branchFuncHTML << str;

        if (covered)
        {
          branchFuncHTML << "  " << htmlBgColor(BG_GREEN) <<
                            "Covered</span>  " << ENDL;
          caseCoveredCnt++;
        }
        else
        {
          branchFuncHTML << htmlBgColor(BG_RED) <<
                            "Not Covered</span>" << ENDL;
        }
      }

      if (caseCoveredCnt == caseCnt)
      {
        // Covered all case conditions
        setAttrib(STMT_BR_COV_FULL, s);
      }
      else
      if (caseCoveredCnt)
      {
        // Covered some case conditions
        setAttrib(STMT_BR_COV_TRUE, s);
      }

      branchFuncOperandCnt += caseCnt;
      branchFuncCoverCnt += caseCoveredCnt;
    }
    else
    {

      if ((branchNum / 8) < hist.branchCount)
      {
#ifdef VAMP_DEBUG_BRANCH
CDBG << "Branch value: " << ((oldHist.brInfo[branchNum / 8] >> (branchNum & 7)) & 3) << "\n";
#endif
        if (db.combineHistory)
        {
          //branchFuncHTML << "<tr><td colspan=2 bgcolor=#a0a0ff>" << ENDL;
          branchFuncHTML << "<tr><td>" << ENDL;
          branchFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;
          int oldCovered = (oldHist.brInfo[branchNum / 8] >> (branchNum & 7)) & 3;
          branchFuncHTML << "     Previous Runs      " << ENDL;
                          //"  Covered      Covered  "
                          //"Not Covered    Covered  "
          branchFuncHTML << "   TRUE         FALSE   " << ENDL;
          genCoverageHTML(oldCovered, true, s, branchFuncHTML);
          branchFuncHTML << "</pre></font></b></center>" << ENDL;
          branchFuncHTML << "</td><td>" << ENDL;
          branchFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;
          //branchFuncHTML << "   ";
          int newCovered = (newHist.brInfo[branchNum / 8] >> (branchNum & 7)) & 3;
          branchFuncHTML << "      Current Run       " << ENDL;
          branchFuncHTML << "   TRUE         FALSE   " << ENDL;
          genCoverageHTML(newCovered, true, s, branchFuncHTML);
          branchFuncHTML << "</pre></font></b></center>" << ENDL;
          branchFuncHTML << "</td><td>" << ENDL;
          branchFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;
          //branchFuncHTML << "   ";
          int covered = (hist.brInfo[branchNum / 8] >> (branchNum & 7)) & 3;
          branchFuncHTML << "     Combined Runs      " << ENDL;
          branchFuncHTML << "   TRUE         FALSE   " << ENDL;
          branchFuncCoverCnt += genCoverageHTML(covered, true, s, branchFuncHTML);

          // Determine number of newly covered branches
          int newCov = (oldCovered & newCovered) ^ newCovered;
#ifdef VAMP_DEBUG_BRANCH
CDBG << "Combined = " << covered << ", Current = " << newCovered << ", New = " << newCov << ENDL;
#endif
          if (newCov == 3)
            newBranchCoveredCount += 2;
          else
          if (newCov)
            ++newBranchCoveredCount;
        }
        else
        {
          branchFuncHTML << "<tr><td>" << ENDL;
          branchFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;
          branchFuncHTML << "   TRUE         FALSE   " << ENDL;

          int covered = (hist.brInfo[branchNum / 8] >> (branchNum & 7)) & 3;
          branchFuncCoverCnt += genCoverageHTML(covered, true, s, branchFuncHTML);
        }

        branchFuncHTML << ENDL;
        branchFuncOperandCnt += 2;
      }
      else
      {
        branchFuncHTML << "   TRUE         FALSE" << ENDL;
        *vampErr << br->statementType << " branch number not found: " <<
                 branchNum << " - aborting" << ENDL;
        throw(1);
      }

      branchFuncHTML << "</pre></font></b></center>" << ENDL;
      branchFuncHTML << "</td></tr>" << ENDL;
    }

    branchFuncHTML << "</table>" << ENDL;

    branchFuncHTML << ENDL;
  }

  // Save branch info for current function
  branchHTMLInfo.push_back(branchFuncHTML.str());

  branchFuncHTML.str("");

  // Save coverage statistics for computing percentages
  branchFunctionCoverageCount.push_back(branchFuncCoverCnt);
  branchFunctionOperandCount.push_back(branchFuncOperandCnt);
  branchTotalCoverageCount += branchFuncCoverCnt;
  branchTotalOperandCount += branchFuncOperandCnt;

  // Increment count of # branch functions calculated
  ++branchCnt;

  // Make sure matching number of branch info is present
  while (branchCnt++ < db.functionInfo.size())
  {
    // Save coverage statistics for computing percentages
    branchFunctionCoverageCount.push_back(0);
    branchFunctionOperandCount.push_back(0);

    // Save MC/DC Info for current function
    branchHTMLInfo.push_back("");
  }
}

// Process condition coverage info
void VampProcess::processCondition(void)
{
  vector<condInfoType>::iterator cond;
  vector<functionInfoType>::iterator func = db.functionInfo.begin();
  ostringstream condFuncHTML;
  int condFuncCoverCnt = 0;
  int condFuncOperandCnt = 0;
  int condCnt = 0;

  for (cond = db.condInfo.begin(); cond < db.condInfo.end(); cond++)
  {
    sourceLocationType s;

    // Find function this expression is within
    while (cond->lhsLoc.lhsLine > func->loc.rhsLine)
    {
      // Save condition info for current function
      condHTMLInfo.push_back(condFuncHTML.str());

      // Save coverage statistics for computing percentages
      condFunctionCoverageCount.push_back(condFuncCoverCnt);
      condFunctionOperandCount.push_back(condFuncOperandCnt);
      condTotalCoverageCount += condFuncCoverCnt;
      condTotalOperandCount += condFuncOperandCnt;

      // Clear count for next function
      condFuncCoverCnt = 0;
      condFuncOperandCnt = 0;

      // Increment count of # condition functions calculated
      ++condCnt;

      condFuncHTML.str("");
      func++;
      if (func >= db.functionInfo.end())
      {
        *vampErr << "Condition expression outside function range - aborting" <<
                ENDL;
        throw(1);
      }
    }

    // Build a sourceLocation encompassing all of expression
    s.lhsLine = cond->lhsLoc.lhsLine;
    s.lhsCol = cond->lhsLoc.lhsCol;
    s.rhsLine = cond->rhsLoc.rhsLine;
    s.rhsCol = cond->rhsLoc.rhsCol;

    condFuncHTML << "<table border bgcolor=\"#d0d0d0\" " <<
                      "width=80% align=\"center\">" << ENDL;
    //condFuncHTML << "<tr><td colspan=2 bgcolor=#ffffa0>" << ENDL;
    //condFuncHTML << "<tr><td colspan=2 bgcolor=#a0ffa0>" << ENDL;
    if (db.combineHistory)
      //condFuncHTML << "<tr><td colspan=3 bgcolor=#e0a0ff>" << ENDL;
      condFuncHTML << "<tr><td colspan=3 bgcolor=#ffffa0>" << ENDL;
    else
      //condFuncHTML << "<tr><td bgcolor=#e0a0ff>" << ENDL;
      condFuncHTML << "<tr><td bgcolor=#ffffa0>" << ENDL;
      //condFuncHTML << "<tr><td colspan=2 bgcolor=#e0a0ff>" << ENDL;
    condFuncHTML << "<center><b><font size=\"5\">" <<
                      "Boolean Condition at " << func->function <<
                      "() <a href='#line_" <<
                      cond->lhsLoc.lhsLine << "'>line " <<
                      cond->lhsLoc.lhsLine << "</a>";
    if (cond->rhsLoc.rhsLine > cond->lhsLoc.lhsLine)
      condFuncHTML << "-" << cond->rhsLoc.rhsLine;
    condFuncHTML << ":</font>" << ENDL;
    condFuncHTML << "<br /><br />" << ENDL;
/*
    condFuncHTML << "<font size=\"5\">" <<
                      htmlSourceText(s, 0) <<
                      "</font></b></center><br />" << ENDL;
*/
    condFuncHTML << "<font size=\"5\">";
    condFuncHTML << "A = " << htmlSourceText(cond->lhsLoc, 0) << "<br />" << ENDL;
    condFuncHTML << "B = " << htmlSourceText(cond->rhsLoc, 0) << "<br />" << ENDL;
    condFuncHTML << "Boolean Condition: (A " << cond->condition << " B)" << ENDL;
    condFuncHTML << "</font></b></center><br />" << ENDL;
    condFuncHTML << "</td></tr>" << ENDL;

    if ((cond->condNum / 8) < hist.condCount)
    {
      if (db.combineHistory)
      {
        //condFuncHTML << "<tr><td colspan=2 bgcolor=#a0a0ff>" << ENDL;
        condFuncHTML << "<tr><td>" << ENDL;
        condFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;
        int oldCovered = (oldHist.condInfo[cond->condNum / 8] >> (cond->condNum & 7)) & 3;
        condFuncHTML << "     Previous Runs      " << ENDL;
                        //"  Covered      Covered  "
                        //"Not Covered    Covered  "
        condFuncHTML << "   TRUE         FALSE   " << ENDL;
        genCoverageHTML(oldCovered, false, s, condFuncHTML);
        condFuncHTML << "</pre></font></b></center>" << ENDL;
        condFuncHTML << "</td><td>" << ENDL;
        condFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;
        //condFuncHTML << "   ";
        int newCovered = (newHist.condInfo[cond->condNum / 8] >> (cond->condNum & 7)) & 3;
        condFuncHTML << "      Current Run       " << ENDL;
        condFuncHTML << "   TRUE         FALSE   " << ENDL;
        genCoverageHTML(newCovered, false, s, condFuncHTML);
        condFuncHTML << "</pre></font></b></center>" << ENDL;
        condFuncHTML << "</td><td>" << ENDL;
        condFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;
        //condFuncHTML << "   ";
        int covered = (hist.condInfo[cond->condNum / 8] >> (cond->condNum & 7)) & 3;
        condFuncHTML << "     Combined Runs      " << ENDL;
        condFuncHTML << "   TRUE         FALSE   " << ENDL;
        condFuncCoverCnt += genCoverageHTML(covered, false, s, condFuncHTML);

        // Determine number of newly covered conditions
        int newCov = (oldCovered & newCovered) ^ newCovered;
#ifdef VAMP_DEBUG_BRANCH
CDBG << "Combined = " << covered << ", Current = " << newCovered << ", New = " << newCov << ENDL;
#endif
        if (newCov == 3)
          newCondCoveredCount += 2;
        else
        if (newCov)
          ++newCondCoveredCount;
      }
      else
      {
        condFuncHTML << "<tr><td>" << ENDL;
        condFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;
        condFuncHTML << "   TRUE         FALSE   " << ENDL;

        int covered = (hist.condInfo[cond->condNum / 8] >> (cond->condNum & 7)) & 3;
        condFuncCoverCnt += genCoverageHTML(covered, false, s, condFuncHTML);
      }

      condFuncHTML << ENDL;
      condFuncOperandCnt += 2;
    }
    else
    {
      condFuncHTML << "   TRUE         FALSE" << ENDL;
      *vampErr << "Condition number " << cond->condNum << " not found - aborting" << ENDL;
      throw(1);
    }

    condFuncHTML << "</pre></font></b></center>" << ENDL;
    condFuncHTML << "</td></tr>" << ENDL;

    condFuncHTML << "</table>" << ENDL;

    condFuncHTML << ENDL;
  }

  // Save condition info for current function
  condHTMLInfo.push_back(condFuncHTML.str());

  condFuncHTML.str("");

  // Save coverage statistics for computing percentages
  condFunctionCoverageCount.push_back(condFuncCoverCnt);
  condFunctionOperandCount.push_back(condFuncOperandCnt);
  condTotalCoverageCount += condFuncCoverCnt;
  condTotalOperandCount += condFuncOperandCnt;

  // Increment count of # condition functions calculated
  ++condCnt;

  // Make sure matching number of condition info is present
  while (condCnt++ < db.functionInfo.size())
  {
    // Save coverage statistics for computing percentages
    condFunctionCoverageCount.push_back(0);
    condFunctionOperandCount.push_back(0);

    // Save MC/DC Info for current function
    condHTMLInfo.push_back("");
  }
}


// Process each MC/DC expression
void VampProcess::processMCDC(void)
{
  vector<mcdcExprInfoType>::iterator exp;
  vector<mcdcOpInfoType>::iterator op;
  int mcdcExprNum = 0;
  ostringstream mcdcFuncHTML;
  vector<functionInfoType>::iterator func = db.functionInfo.begin();
  int mcdcFuncCoverCnt = 0;
  int mcdcFuncOperandCnt = 0;
  int mcdcCnt = 0;

  for (exp = db.mcdcExprInfo.begin();
       exp < db.mcdcExprInfo.end();
       exp++, mcdcExprNum++)
  {
    // Build binary tree for MC/DC expression
    mcdcOpInfoType firstOperand = exp->opInfo.front();

    // Find function this expression is within
    while (firstOperand.lhsLoc.lhsLine > func->loc.rhsLine)
    {
      // Save MC/DC Info for current function
      mcdcHTMLInfo.push_back(mcdcFuncHTML.str());

      // Save coverage statistics for computing percentages
      mcdcFunctionCoverageCount.push_back(mcdcFuncCoverCnt);
      mcdcFunctionOperandCount.push_back(mcdcFuncOperandCnt);
      mcdcTotalCoverageCount += mcdcFuncCoverCnt;
      mcdcTotalOperandCount += mcdcFuncOperandCnt;

      // Clear count for next function
      mcdcFuncCoverCnt = 0;
      mcdcFuncOperandCnt = 0;

      // Increment count of # MC/DC functions calculated
      ++mcdcCnt;

      //mcdcFuncHTML.clear();
      mcdcFuncHTML.str("");
      func++;
      if (func >= db.functionInfo.end())
      {
        *vampErr << "MC/DC expression outside function range - aborting" << ENDL;
        throw(1);
      }
    }

    firstOperand.lhsLoc.rhsLine = firstOperand.rhsLoc.rhsLine;
    firstOperand.lhsLoc.rhsCol  = firstOperand.rhsLoc.rhsCol;

    //mcdcFuncHTML << "<table border rules=none frame=box bgcolor=\"#d0d0d0\" " <<
    //          "width=80% align=\"center\">" << ENDL;
    mcdcFuncHTML << "<table border bgcolor=\"#d0d0d0\" " <<
                    "width=80% align=\"center\">" << ENDL;
    //mcdcFuncHTML << "<tr><td colspan=2>" << ENDL;
    mcdcFuncHTML << "<tr><td colspan=2 bgcolor=#ffffa0>" << ENDL;
    mcdcFuncHTML << "<center><b><font size=\"5\">" <<
                    "Expression at " << func->function <<
                    "() <a href='#line_" <<
                    firstOperand.lhsLoc.lhsLine << "'>line " <<
                    firstOperand.lhsLoc.lhsLine << "</a>";
    if (firstOperand.rhsLoc.rhsLine > firstOperand.lhsLoc.lhsLine)
      mcdcFuncHTML << "-" << firstOperand.rhsLoc.rhsLine;
    mcdcFuncHTML << ":</font>" << ENDL;
    //mcdcFuncHTML << ":</font></b><center>" << ENDL;
    //mcdcFuncHTML << "</td></tr>" << ENDL;
    //mcdcFuncHTML << "<tr><td>" << ENDL;
    mcdcFuncHTML << "<br /><br />" << ENDL;
    //mcdcFuncHTML << sourceText(firstOperand.lhsLoc) << "<br />" << ENDL;
    //mcdcFuncHTML << "<center><b><font size=\"5\">" <<
    //                sourceText(firstOperand.lhsLoc) <<
    //                ":</font></b></center>" << ENDL;
    mcdcFuncHTML << "<font size=\"5\">" <<
                    htmlSourceText(firstOperand.lhsLoc, 0) <<
                    "</font></b></center><br />" << ENDL;
    mcdcFuncHTML << "</td></tr>" << ENDL;

    int opCnt = 0;
    for (op = exp->opInfo.begin(); op < exp->opInfo.end(); op++)
    {
      // Unused expression pointer (used only in Vamp)
      clang::BinaryOperator *tmpExpr = NULL;
      if (!exprTree.insertLeaf(op->andOp, opCnt++, tmpExpr,
                               op->lhsLoc.lhsLine, op->lhsLoc.lhsCol,
                               op->lhsLoc.rhsLine, op->lhsLoc.rhsCol,
                               op->rhsLoc.lhsLine, op->rhsLoc.lhsCol,
                               op->rhsLoc.rhsLine, op->rhsLoc.rhsCol))
      {
        *vampErr << "Expression too complex at line " << op->lhsLoc.lhsLine <<
                    " - aborting" << ENDL;
        throw(1);
      }
    }

    if (exprTree.root != NULL)
    {
      int leafCnt = 0;

      // Label for each of up to 31 operands in expression
      string opStr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcde");
      vector<sourceLocationType> mcdcNodeInfo;

      walkTree(leafCnt, exprTree.root, mcdcNodeInfo);

      //mcdcFuncHTML << "<tr><td>" << ENDL;
      //mcdcFuncHTML << "<center><b><font size=\"4\">" <<
      //                "MC/DC Conditions For Expression:" <<
      //                "</font></b></center>" << ENDL;
      mcdcFuncHTML << "<tr><td colspan=2 bgcolor=#a0a0ff>" << ENDL;
      mcdcFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;
      mcdcFuncHTML << "MC/DC Conditions For Expression" << ENDL;
      mcdcFuncHTML << "</font></b></center>" << ENDL;
      mcdcFuncHTML << "</td></tr>" << ENDL;

      //mcdcFuncHTML << "<tr><td colspan=2 bgcolor=#a0a0ff>" << ENDL;
      //mcdcFuncHTML << "<center><font size=\"5\">" <<
      //                "MC/DC Conditions For Expression:</font></center>" <<
      //                ENDL;
      //mcdcFuncHTML << "</th></tr>" << ENDL;

#ifdef FUNKY
      mcdcFuncHTML << "<tr><td width=50% align=\"right\"><pre>" << ENDL;
      vector<sourceLocationType>::iterator node = mcdcNodeInfo.begin();
      for (int i = 0; node < mcdcNodeInfo.end(); ++i, ++node)
      {
        mcdcFuncHTML << opStr[i] << " = " << ENDL;
      }
      mcdcFuncHTML << "</pre></td>" << ENDL;

      mcdcFuncHTML << "<td><pre>" << ENDL;

      node = mcdcNodeInfo.begin();
      for (int i = 0; node < mcdcNodeInfo.end(); ++i, ++node)
      {
        mcdcFuncHTML << sourceText(*node) << ENDL;
      }
      mcdcFuncHTML << "</pre></td></tr>" << ENDL;
#else
      mcdcFuncHTML << "<tr><td colspan=2>" << ENDL;
      mcdcFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;
      mcdcFuncHTML << "<table bgcolor=\"#d0d0d0\" " <<
                      "align=\"center\">" << ENDL;
      mcdcFuncHTML << "<tr><td>" << ENDL;
      mcdcFuncHTML << "<div style='text-align: left'><b><font size=\"5\"><pre>" << ENDL;

      vector<sourceLocationType>::iterator node = mcdcNodeInfo.begin();
      for (int i = 0; node < mcdcNodeInfo.end(); ++i, ++node)
      {
        vector<string> strings;
        genHtmlSourceText(*node, 0, strings);
        mcdcFuncHTML << opStr[i] << " = " << strings[0] << ENDL;
        for (int j = 1; j < strings.size(); j++)
        {
          mcdcFuncHTML << "    " << strings[j] << ENDL;
        }
      }

      mcdcFuncHTML << "</pre></font></b></div>" << ENDL;
      mcdcFuncHTML << "</td></tr></table>" << ENDL;
      mcdcFuncHTML << "</pre></font></b></center>" << ENDL;
      mcdcFuncHTML << "</td></tr>" << ENDL;
#endif
      vector<string> falseStr;
      vector<string> trueStr;

      // Get possible True and False combinations
      exprTree.getFalse(exprTree.root, &falseStr);
      exprTree.getTrue(exprTree.root, &trueStr);

      int operandCnt = falseStr.front().length();

      // Process history and find matches for each MCDC expression
/*
      unsigned int gotFalse = 0;    // Mask for False results covered
      unsigned int gotTrue = 0;     // Mask for True results covered
      unsigned int gotOldFalse = 0; // Mask for old False results covered
      unsigned int gotOldTrue = 0;  // Mask for old True results covered
      unsigned int gotNewFalse = 0; // Mask for new False results covered
      unsigned int gotNewTrue = 0;  // Mask for new True results covered
*/
      bitset<MAX_BITSET> gotFalse;    // Mask for False results covered
      bitset<MAX_BITSET> gotTrue;     // Mask for True results covered
      bitset<MAX_BITSET> gotOldFalse; // Mask for old False results covered
      bitset<MAX_BITSET> gotOldTrue;  // Mask for old True results covered
      bitset<MAX_BITSET> gotNewFalse; // Mask for new False results covered
      bitset<MAX_BITSET> gotNewTrue;  // Mask for new True results covered

      // Compute number of bytes used to store MCDC expression results:
      // 1 = 1-7, 2 = 8-15, 4 = 15-31
      int byteCnt;
      if (operandCnt < 8)
        byteCnt = 1;
      else
      if (operandCnt < 16)
        byteCnt = 2;
      else
        byteCnt = 4;

      mcdcByteCnt.push_back(byteCnt);
      int mcdcCombCnt = byteCnt * (falseStr.size() + trueStr.size());
////CDBG << "opCnt = " << totalMcdcCombCnt;
      totalMcdcCombCnt += mcdcCombCnt;
////CDBG << " to " << totalMcdcCombCnt << ENDL;
      mcdcOpCnt.push_back(totalMcdcCombCnt);
////CDBG << "same as " << mcdcOpCnt[mcdcExprNum] <<
////            " to " << mcdcOpCnt[mcdcExprNum+1] << ENDL;

///*** Consider pulling expressions from this code to pass to code that
///*** follows; this way we could track old/new/combined as we go
      int newOffset = 0;

      if (db.combineHistory)
      {
        // Combine old and new MCDC history here
        int i, j, k, m;

        // Copy old history first
        for (i = mcdcOpCnt[mcdcExprNum];
             (i < mcdcOpCnt[mcdcExprNum + 1]) && oldHist.mcdcInfo[i];
              i += byteCnt)
        {
          for (j = 0; j < byteCnt; j++)
          {
            hist.mcdcInfo[i + j] = oldHist.mcdcInfo[i + j];
#ifdef VAMP_DEBUG_MCDC
char v[64];
sprintf(v, "%#x\n", hist.mcdcInfo[i + j]);
CDBG << "Old history mcdcInfo[" << i + j << "] = " << v << ENDL;
#endif
          }
        }

        // Now add new history
        // New data starts at both newOffset and k;
        // k increments whenever new history data is added
        newOffset = i;
        k = i;

        // For each MCDC value in newHist, look for a match in oldHist
        bool match = false;
        for (j = mcdcOpCnt[mcdcExprNum];
             (j < mcdcOpCnt[mcdcExprNum + 1]) && newHist.mcdcInfo[j] && !match;
              j += byteCnt)
        {
          for (m = mcdcOpCnt[mcdcExprNum]; !match && (m < k); m += byteCnt)
          {
            match = true;
            for (int n = 0; match && (n < byteCnt); n++)
            {
              match = (newHist.mcdcInfo[j + n] == oldHist.mcdcInfo[m + n]);
            }
          }

          if (!match)
          {
            // No match found - copy new val into hist
            for (int n = 0; n < byteCnt; n++)
            {
#ifdef VAMP_DEBUG_MCDC
char v[64];
sprintf(v, "%#x\n", newHist.mcdcInfo[j + n]);
CDBG << "Adding mcdcInfo[" << k << "] = " << v << ENDL;
#endif
               hist.mcdcInfo[k++] = newHist.mcdcInfo[j + n];
            }
          }
        }
      }

#ifdef VAMP_DEBUG_MCDC
      CDBG << "Finding matches for each MC/DC combination:" << ENDL;
      for (int j = 0; j < falseStr.size(); j++)
        CDBG << "falseStr[" << j << "]=" << falseStr[j] << ENDL;
      for (int j = 0; j < trueStr.size(); j++)
        CDBG << "trueStr[" << j << "]=" << trueStr[j] << ENDL;
      //CDBG << "MCDC falseStr[0] = " << falseStr[0] << ENDL;
      CDBG << "MCDC operand count = " << operandCnt << " from '" <<
              falseStr.front() << "'" << ENDL;
      CDBG << "MCDC false count = " << falseStr.size() <<
            "; MCDC true count = " << trueStr.size() << ENDL;
      CDBG << "MCDC byteCnt = " << byteCnt << "; mcdcCombCount = " <<
              mcdcCombCnt << ENDL;

      CDBG << "MCDC History from " << mcdcOpCnt[mcdcExprNum] << " to " <<
                                      mcdcOpCnt[mcdcExprNum + 1] <<  ENDL;
#endif

      // Walk through possible MC/DC combinations (both TRUE and FALSE)
      // till we find a match for the result
      if (hist.mcdcCount)
      {
          int expr;
          for (expr = mcdcOpCnt[mcdcExprNum];
                    hist.mcdcInfo[expr] && (expr < mcdcOpCnt[mcdcExprNum + 1]);
                      expr += mcdcByteCnt[mcdcExprNum])
          {
            if (db.combineHistory && (expr == newOffset))
            {
              // Save results for expressions covered in previous runs
              gotOldTrue = gotTrue;
              gotOldFalse = gotFalse;
            }

            unsigned int mcdcVal = hist.mcdcInfo[expr] >> 1;
    #ifdef VAMP_DEBUG_MCDC
            char v[256];
            for (int j = 0; j < mcdcByteCnt[mcdcExprNum]; j++)
            {
              sprintf(v, "%#x\n", hist.mcdcInfo[expr + j]);
                CDBG << "mcdcVal[" << j << "] = " << v;
            }
    #endif
            // Use top 7 bits of mcdcInfo[expr], followed by 8 bits
            // of following mcdcInfo bytes
            int whichByte = 0;
            int whichBit = 7;

    /*
            if (mcdcByteCnt[mcdcExprNum] == 2)
              mcdcVal |= hist.mcdcInfo[expr + 1];
    */
            string mcdcStr;
            for (int cnt = 0; cnt < operandCnt; cnt++)
            {
              mcdcStr += (char) ('0' | (mcdcVal & 1));
              if (--whichBit)
              {
                mcdcVal >>= 1;
              }
              else
              {
                whichBit = 8;
                ++whichByte;
                mcdcVal = hist.mcdcInfo[expr + whichByte];
              }
            }

            int which = checkMatch(mcdcStr, falseStr);
            if (which != -1)
            {
    #ifdef VAMP_DEBUG_MCDC
              CDBG << mcdcStr << " matches FALSE " << falseStr[which] << ENDL;
    //FIXME - which can be > MAX_BITSET!
    if (which > MAX_BITSET)
    CDBG << "False too big: " << which << ", size = " << falseStr.size() << ENDL;
    #endif
              gotFalse.set(which);
              if (db.combineHistory && (expr >= newOffset))
              {
                // Save results for expressions covered in current run
                gotNewFalse.set(which);
              }
            }
            else
            {
              which = checkMatch(mcdcStr, trueStr);
              if (which != -1)
              {
    #ifdef VAMP_DEBUG_MCDC
                CDBG << mcdcStr << " matches TRUE  " << trueStr[which] << ENDL;
    //FIXME - which can be > MAX_BITSET!
    if (which > MAX_BITSET)
    CDBG << "True too big: " << which << ENDL;
    #endif
                gotTrue.set(which);
                if (db.combineHistory && (expr >= newOffset))
                {
                  // Save results for expressions covered in current run
                  gotNewTrue.set(which);
                }
              }
    #ifdef VAMP_DEBUG_MCDC
              else
              {
                CDBG << mcdcStr << " - no matches found!" << ENDL;
              }
    #endif
            }
          }
      }

#ifdef VAMP_DEBUG_MCDC
      CDBG << "Finding independent pairs" << ENDL;
#endif

      // Count number of times a false/true element is used
      // as part of an independent pair
      int falseCount[MAX_BITSET] = { 0 };
      int trueCount[MAX_BITSET] = { 0 };

      // Find independant pairs for each operand
      vector<string> indPairs;
      vector<int> indPairCount;
      vector<mcdcPair> mcdcPairs[32];
      int mcdcCoverCnt = 0;  // Used for computing percentage covered

      char fStr[64], tStr[64], ch;
      // Determine maximum string length for a pair
      int sz = falseStr.size();
      sprintf(fStr, "F%d:", sz);
      sz = trueStr.size();
      sprintf(tStr, "T%d ", sz);
      int maxLength = strlen(fStr) + strlen(tStr);

      // Compute max # columns of pairs to display before wrapping line
      int maxCol = 80 / maxLength;

      vector<string>::iterator n;
      bool gotNew = false;
      int opTypes[31][4] = { 0 };

      for (int i = 0; i < operandCnt; i++)
      {
        int whichFalse = 0;
        int curCol = 0;
        int cnt = 0;
        int newCnt = 0;
        int pairCount = 0;
        ostringstream indPair;
        //indPair << opStr.substr(i, 1) << ": ";

#ifdef VAMP_DEBUG_MCDC
CDBG << "MCDC falseStr[0] = " << falseStr[0] << ENDL;
#endif
        // Search each False half of pair for match to True side
        for (n = falseStr.begin(); n < falseStr.end(); whichFalse++, n++)
        {
          char ch = n->c_str()[i];
          if (ch != '-')
          {
            string testStr(*n);
            // Convert '0' to '1' or '1' to '0'
            testStr.replace(i, 1, 1, ch ^ 1);
#ifdef VAMP_DEBUG_MCDC
            CDBG << "Testing " << opStr.substr(i, 1) << ": " <<
                    *n << " with " << testStr << ENDL;
#endif
            int whichTrue = checkMatch(testStr, trueStr, true);
            if (whichTrue != -1)
            {
              // Got match - build pair
              ++pairCount;

              // Increment count for each one found
              ++falseCount[whichFalse];
              ++trueCount[whichTrue];

              mcdcPair pair = { whichFalse, whichTrue };
              mcdcPairs[i].push_back(pair);
#ifdef VAMP_DEBUG_MCDC
// FIXME: whichFalse or which can be > MAX_BITSET
if ((whichFalse > MAX_BITSET) || (whichTrue > MAX_BITSET))
CDBG << "Too big: " << whichFalse << ":" << whichTrue << ENDL;
              CDBG << "Got pair #" << pairCount << ": " << whichFalse << ", " << whichTrue << ENDL;
#endif
#ifdef USE_TEXT
              char str[16], ch;
              bool f = gotFalse & (1 << whichFalse);
              bool t = gotTrue & (1 << whichTrue);
              if (f && t)
              {
                ch = '*';
                cnt = 2;
              }
              else
              if (f)
              {
                ch = '<';
                if (!cnt)
                  cnt = 1;
              }
              else
              if (t)
              {
                ch = '>';
                if (!cnt)
                  cnt = 1;
              }
              else
              {
                ch = ' ';
              }
              sprintf(str, "F%d:T%d %c   ", whichFalse + 1, whichTrue + 1, ch);
              str[11] = '\0';
              indPair << str;
#else
              bool f = gotFalse.test(whichFalse);
              bool t = gotTrue.test(whichTrue);

              bool newF = false;
              bool newT = false;
              if (db.combineHistory)
              {
                if (((gotOldFalse & gotNewFalse) ^ gotNewFalse).test(whichFalse))
                {
                  newCnt++;
                  newF = true;
                }
                if (((gotOldTrue & gotNewTrue) ^ gotNewTrue).test(whichTrue))
                {
                  newCnt++;
                  newT = true;
                }
              }

              sprintf(fStr, "F%d", whichFalse + 1);
              sprintf(tStr, "T%d", whichTrue + 1);
              int total = strlen(fStr) + strlen(tStr) + 1;
              if (db.combineHistory)
              {
                if (newF)
                  sprintf(fStr, "<u>F%d</u>", whichFalse + 1);
                if (newT)
                  sprintf(tStr, "<u>T%d</u>", whichTrue + 1);
              }

              if (f && t)
              {
                indPair << htmlBgColor(BG_GREEN) << fStr << ':' <<
                           tStr << "</span>";
                cnt = 2;
                ++opTypes[i][3];
              }
              else
              if (f)
              {
                indPair << htmlBgColor(BG_GREEN) << fStr << "</span>:" <<
                           htmlBgColor(BG_RED) << tStr << "</span>";
                if (!cnt)
                {
                  cnt = 1;
                }
                ++opTypes[i][2];
              }
              else
              if (t)
              {
                indPair << htmlBgColor(BG_RED) << fStr << "</span>:" <<
                           htmlBgColor(BG_GREEN) << tStr << "</span>";
                if (!cnt)
                {
                  cnt = 1;
                }
                ++opTypes[i][1];
              }
              else
              {
                indPair << htmlBgColor(BG_RED) << fStr << ':' <<
                           tStr << "</span>";
                ++opTypes[i][0];
              }

              if (db.combineHistory && newCnt)
              {
                indPair << '*';
                ++total;
              }

              // Pad the string so that pairs align vertically (e.g.)
              // F1:T1    F1:T13   F1:T14
              // F12:T37  F12:T38  F12:T39
              string spaceStr;
              spaceStr.insert(0, maxLength - total, ' ');
              indPair << spaceStr;

              // Limit string length
              if (++curCol > maxCol)
              {
                indPair << "\n   ";
                curCol = 0;
              }
#endif
#ifdef VAMP_DEBUG_MCDC
              CDBG << "Found match for " << opStr.substr(i, 1) << ":" <<
                      *n << " to " << trueStr[whichTrue] << ENDL;
#endif
            }
          }
        }
#ifdef VAMP_DEBUG_MCDC
        CDBG << "Completed " << i << " of " << operandCnt << ENDL;
#endif

        string str;
        if (cnt == 0)
          str = htmlBgColor(BG_RED);
        else
        if (cnt == 2)
          str = htmlBgColor(BG_GREEN);
        else
          str = htmlBgColor(BG_YELLOW);
        str += opStr.substr(i, 1) + ":</span> ";
        str += indPair.str();
#ifdef VAMP_DEBUG_MCDC
//CDBG << str << ENDL;
#endif

        //indPairs.push_back(indPair.str());
        indPairs.push_back(str);
// FIXME: indPairCount and pairCount not used!
        indPairCount.push_back(pairCount);

        mcdcCoverCnt += cnt;
        mcdcNewCoverCount += newCnt;
        if (newCnt)
          gotNew = true;
      }
#ifdef VAMP_DEBUG_MCDC
      CDBG << "Generating MCDC Function results" << ENDL;
#endif

      // Display results
      mcdcFuncHTML << "<tr><td colspan=2 bgcolor=#a0a0ff>" << ENDL;
      mcdcFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;
      mcdcFuncHTML << "MC/DC Possible Combinations" << ENDL;
      mcdcFuncHTML << "</font></b></center>" << ENDL;
      mcdcFuncHTML << "</td></tr>" << ENDL;

      mcdcFuncHTML << "<tr valign=top><td width=50% align=\"right\">" <<
                      "<font size=\"5\"><b><pre>" << ENDL;
      mcdcFuncHTML << "Outcome = FALSE:  ";
      int num = 2 * operandCnt - 4;
      if (num > 0)
      {
        string sp;
        sp.insert(0, num, ' ');
        mcdcFuncHTML << sp;
      }

      mcdcFuncHTML << endl << ENDL;
      mcdcFuncHTML << "      <u>";

      // Intersperse spaces with 'A..Z'
      for (int i = 0; i < operandCnt; i++)
        mcdcFuncHTML << opStr[i] << ' ';

      mcdcFuncHTML << "  Covered</u>     " << ENDL;

      char str[8];
      int i;
      for (i = 0, n = falseStr.begin(); n < falseStr.end(); n++)
      {
        sprintf(str, "F%-2d ", i + 1);
        mcdcFuncHTML << str;

        // Intersperse spaces with '0', '1' and '-'
        for (int j = 0; j < n->length(); j++)
          mcdcFuncHTML << (*n)[j] << ' ';

        if (gotFalse.test(i++))
          mcdcFuncHTML << "    Yes       " << ENDL;
        else
          mcdcFuncHTML << "    No        " << ENDL;
      }

      mcdcFuncHTML << "</pre></b></font></td>" << ENDL;

      mcdcFuncHTML << "<td width=50%><font size=\"5\"><b><pre>" << ENDL;
      mcdcFuncHTML << "      Outcome = TRUE:" << endl << ENDL;
      mcdcFuncHTML << "      <u>";
      for (i = 0; i < operandCnt; i++)
        mcdcFuncHTML << opStr[i] << ' ';
      mcdcFuncHTML << "  Covered</u>  " << ENDL;

      for (i = 0, n = trueStr.begin(); n < trueStr.end(); n++)
      {
        sprintf(str, "  T%-2d ", i + 1);
        mcdcFuncHTML << str;
        for (int j = 0; j < n->length(); j++)
          mcdcFuncHTML << (*n)[j] << ' ';
        if (gotTrue.test(i++))
          mcdcFuncHTML << "    Yes" << ENDL;
        else
          mcdcFuncHTML << "    No " << ENDL;
      }

      mcdcFuncHTML << "</pre></b></font></td></tr>" << ENDL;

      mcdcFuncHTML << "<tr><td colspan=2 bgcolor=#a0a0ff>" << ENDL;
      mcdcFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;
      mcdcFuncHTML << "MC/DC Independent Pairs" << ENDL;
      mcdcFuncHTML << "</font></b></center>" << ENDL;
      mcdcFuncHTML << "</td></tr>" << ENDL;

      mcdcFuncHTML << "<tr><td colspan=2>" << ENDL;
      mcdcFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;
      mcdcFuncHTML << "<table bgcolor=\"#d0d0d0\" " <<
                      "align=\"center\">" << ENDL;
      mcdcFuncHTML << "<tr><td>" << ENDL;
      mcdcFuncHTML << "<div style='text-align: left'><b><font size=\"5\"><pre>" << ENDL;
      //mcdcFuncHTML << "<div style='float: right; text-align: left'><b><font size=\"5\"><pre>" << ENDL;
      //??mcdcFuncHTML << "<div style='text-align: left'><b><font size=\"5\"><pre>" << ENDL;

#ifdef VAMP_DEBUG_MCDC
      CDBG << "Computing pairs" << ENDL;
#endif
      vector<string>::iterator ind;
      i = 0;
      for (ind = indPairs.begin(); ind < indPairs.end(); ++ind, ++i)
      {
        mcdcFuncHTML << *ind << ENDL;
      }
#ifdef VAMP_DEBUG_MCDC
      CDBG << "Completed pairs" << ENDL;
#endif

      mcdcFuncHTML << "</pre></font></b></div>" << ENDL;
      mcdcFuncHTML << "</td></tr></table>" << ENDL;

      mcdcFuncCoverCnt += mcdcCoverCnt;
      mcdcFuncOperandCnt += (2 * operandCnt);
      float coveragePercent = 100.0f * mcdcCoverCnt / (2.0f * operandCnt);
      char perCent[32];
      sprintf(perCent, "%4.1f", coveragePercent);
      if (db.combineHistory && gotNew)
      {
        mcdcFuncHTML << endl <<
                  "* = New coverage this run; underscore designates which" <<
                        ENDL;
      }
      mcdcFuncHTML << endl << "MC/DC coverage: " << perCent << "%<br />" <<
                      ENDL;
      mcdcFuncHTML << "</font></b></center>" << ENDL;
      //mcdcFuncHTML << "</font></b></div>" << ENDL;
      mcdcFuncHTML << "</td></tr>" << ENDL;
/*
      int coveragePercent = (1000 * mcdcCoverCnt / 
                                    (2 * operandCnt) + 5) / 10;
      mcdcFuncHTML << "<tr><td colspan=2>" << ENDL;
      mcdcFuncHTML << "<div id=\"" << func << "_mcdc_percent\">" <<
                      "<div id=\"" << func << "_mcdc_percent_in\">" <<
                      coveragePercent << "% <font size=\"1\">&nbsp;&nbsp;(" <<
                      mcdcCoverCnt << " / " <<
                      (2 * operandCnt) <<
                      ")</font></div></div> </td>" << ENDL;
*/

      // FIXME: Count number of times each failed F# and T# are used as
      //        members of independent pairs. Then determine which
      //        are the fewest members needed to reach coverage.

/*
      unsigned int testFalse = gotFalse;
      unsigned int testTrue = gotTrue;
*/
      bitset<MAX_BITSET> testFalse = gotFalse;
      bitset<MAX_BITSET> testTrue = gotTrue;
#ifdef VAMP_DEBUG_MCDC
      CDBG << "Computing minimum number of test cases" << ENDL;
#endif
      int minOps;

      if (falseStr.size() * trueStr.size() < 200)
      {
        minOps = getMinCond(mcdcPairs, 0, testFalse, testTrue);
      }
      else
      {
        // Create a curtailed version of mcdcPairs to use for determining
        // minimum number of tests needed to complete MC/DC coverage.
        // Give priority to partially completed pairs with a high "hit count"
        // for the incomplete half of the pair.  The term "hit count" implies
        // the term is used to complete other MC/DC pairs.

        vector<mcdcPair> minMcdcPairs[32];
        for (int i = 0; i < operandCnt; i++)
        {
          int minFalseOnly;  // Minimum # pairs to use with only False covered
          int minTrueOnly;   // Minimum # pairs to use with only True covered
          int minNeither;    // Minimum # pairs to use with neither covered

          // FIXME: Create some heuristic to determine reasonable number of
          // pairs to add based on operandCnt.  Fewer operands mean more pairs
          // are reasonable to use.
          minFalseOnly = 32 / operandCnt;
          minTrueOnly = 32 / operandCnt;
          minNeither = 0;

          if (opTypes[i][3])
          {
            // Already covered - nothing more to add
            // FIXME: Determine if we need to add one example
            bool done = false;
            vector<mcdcPair>::iterator pair = mcdcPairs[i].begin();
            while (!done && (pair < mcdcPairs[i].end()))
            {
              if (gotFalse.test(pair->f) && gotTrue.test(pair->t))
              {
                minMcdcPairs[i].push_back(*pair);
                done = true;
              }

              ++pair;
            }
          }
          else
          {
            // Are there fewer False only covered pairs than requested?
            if (minFalseOnly > opTypes[i][2])
            {
              // Yes - give remainder to minTrueOnly
              minTrueOnly += minFalseOnly - opTypes[i][2];
              minFalseOnly = opTypes[i][2];
            }

            // Are there fewer True only covered pairs than requested?
            if (minTrueOnly > opTypes[i][1])
            {
              // Yes - give remainder to minTrueOnly
              minNeither += minTrueOnly - opTypes[i][1];
              minTrueOnly = opTypes[i][1];
            }

            // Are there fewer non-covered pairs than requested?
            if (minNeither > opTypes[i][0])
            {
              // Yes - reduce to how many are available
              minNeither = opTypes[i][0];
            }

            vector<indexPairType> trueIndex;
            vector<indexPairType> falseIndex;
            vector<indexPairType> neitherIndex;
            indexPairType indexPair;

            vector<mcdcPair>::iterator pair = mcdcPairs[i].begin();
            for (int index = 0; pair < mcdcPairs[i].end(); ++pair, ++index)
            {
              if (gotFalse.test(pair->f))
              {
                if (!gotTrue.test(pair->t))
                {
                  // Prioritize pairs with highest True usage counts
                  indexPair.usageCount = trueCount[pair->t];
                  indexPair.index = index;
                  falseIndex.push_back(indexPair);
                }
              }
              else
              if (gotTrue.test(pair->t))
              {
                // Prioritize pairs with highest False usage counts
                indexPair.usageCount = falseCount[pair->f];
                indexPair.index = index;
                trueIndex.push_back(indexPair);
              }
              else
              {
                // Prioritize pairs with highest combined usage counts
                indexPair.usageCount = falseCount[pair->f] + trueCount[pair->t];
                indexPair.index = index;
                neitherIndex.push_back(indexPair);
              }
            }

            // FIXME: sort each index vector based on usage count
#ifdef USE_IND_PAIR_CLASS
            sort(falseIndex.begin(), falseIndex.end());
#else
            sort(falseIndex.begin(), falseIndex.end(), indexPairCompare());
/*
            // Seems like this would be more efficient than sort,
            // but we get different results.
            partial_sort(falseIndex.begin(),
                         falseIndex.begin() + minFalseOnly,
                         falseIndex.end(),
                         indexPairCompare());
*/
#endif

#ifdef VAMP_DEBUG_MCDC
vector<indexPairType>::iterator fpair = falseIndex.begin();
CDBG << "False pairs[" << i << "]:" << ENDL;
while (fpair < falseIndex.end())
{
  CDBG << "F" << mcdcPairs[i][fpair->index].f+1 << ":T" << mcdcPairs[i][fpair->index].t+1 << ", usage count = " << fpair->usageCount << ENDL;
  ++fpair;
}
#endif
            // Now add top prioritized pairs
            for (int j = 0; j < minFalseOnly; j++)
            {
               minMcdcPairs[i].push_back(mcdcPairs[i][falseIndex[j].index]);
#ifdef VAMP_DEBUG_MCDC
               CDBG << "Adding False Only pair[" << i << "] F" <<
                       mcdcPairs[i][falseIndex[j].index].f+1 << ":T" <<
                       mcdcPairs[i][falseIndex[j].index].t+1 << ENDL;
#endif
            }

#ifdef USE_IND_PAIR_CLASS
            sort(trueIndex.begin(), trueIndex.end());
#else
            sort(trueIndex.begin(), trueIndex.end(), indexPairCompare());
/*
            partial_sort(trueIndex.begin(),
                         trueIndex.begin() + minTrueOnly,
                         trueIndex.end(),
                         indexPairCompare());
*/
#endif
/*
vector<indexPairType>::iterator tpair = trueIndex.begin();
CDBG << "True pairs[" << i << "]:" << ENDL;
while (tpair < trueIndex.end())
{
  CDBG << "F" << mcdcPairs[i][tpair->index].f+1 << ":T" << mcdcPairs[i][tpair->index].t+1 << ", usage count = " << tpair->usageCount << ENDL;
  ++tpair;
}
*/
            for (int j = 0; j < minTrueOnly; j++)
            {
               minMcdcPairs[i].push_back(mcdcPairs[i][trueIndex[j].index]);
#ifdef VAMP_DEBUG_MCDC
               CDBG << "Adding True Only pair[" << i << "] F" <<
                       mcdcPairs[i][trueIndex[j].index].f+1 << ":T" <<
                       mcdcPairs[i][trueIndex[j].index].t+1 << ENDL;
#endif
            }

#ifdef USE_IND_PAIR_CLASS
            sort(neitherIndex.begin(), neitherIndex.end());
#else
            sort(neitherIndex.begin(), neitherIndex.end(), indexPairCompare());
/*
            partial_sort(neitherIndex.begin(),
                         neitherIndex.begin() + minNeither,
                         neitherIndex.end(),
                         indexPairCompare());
*/
#endif
/*
//vector<indexPairType>::iterator npair = neitherIndex.begin();
npair = neitherIndex.begin();
CDBG << "Neither pairs[" << i << "]:" << ENDL;
while (npair < neitherIndex.end())
{
  CDBG << "F" << mcdcPairs[i][npair->index].f+1 << ":T" << mcdcPairs[i][npair->index].t+1 << ", usage count = " << npair->usageCount << ENDL;
  ++npair;
}
*/
            for (int j = 0; j < minNeither; j++)
            {
               minMcdcPairs[i].push_back(mcdcPairs[i][neitherIndex[j].index]);
#ifdef VAMP_DEBUG_MCDC
               CDBG << "Adding False Only pair[" << i << "] F" <<
                       mcdcPairs[i][neitherIndex[j].index].f+1 << ":T" <<
                       mcdcPairs[i][neitherIndex[j].index].t+1 << ENDL;
#endif
            }
          }
        }

        minOps = getMinCond(minMcdcPairs, 0, testFalse, testTrue);
      }

#ifdef VAMP_DEBUG_MCDC
      CDBG << "Min pair count = " << minOps << ENDL;
#endif
/*
      unsigned int newFalse = gotFalse ^ testFalse;
      unsigned int newTrue = gotTrue ^ testTrue;
*/
      bitset<MAX_BITSET> newFalse = gotFalse ^ testFalse;
      bitset<MAX_BITSET> newTrue = gotTrue ^ testTrue;
      if (db.showTestCases && minOps)
      {
        int cnt = 1;

        mcdcFuncHTML << "<tr><td colspan=2 bgcolor=#a0a0ff>" << ENDL;
        mcdcFuncHTML << "<center><b><font size=\"5\"><pre>" << ENDL;
        mcdcFuncHTML << "Recommended Additional Test Cases" << ENDL;
        mcdcFuncHTML << "</font></b></center>" << ENDL;
        mcdcFuncHTML << "</td></tr>" << ENDL;

        mcdcFuncHTML << "<tr valign=top><td width=50%>" << ENDL;
        mcdcFuncHTML << "<font size=\"5\"><b><pre>" << ENDL;

        if (newFalse.any())
        {
          for (i = 0; i < MAX_BITSET; i++)
          {
            if (newFalse.test(i))
            {
              mcdcFuncHTML << "<b>  Case " << cnt++ <<
                           //": F" << (char) ('1' + i) << "  " <<
                           ": F" << (i + 1) << "  " <<
                           falseStr[i] << "</b><br />" << ENDL;
              mcdcFuncHTML << "<b>  <u>Result  Condition</u></b>" << ENDL;

              for (int j = 0; j < falseStr[i].length(); j++)
              {
                if (falseStr[i][j] == '0')
                {
                  vector<string> strings;
                  genHtmlSourceText(mcdcNodeInfo[j], 10, strings);
                  mcdcFuncHTML << "   False  " << strings[0] << ENDL;
                  for (int k = 1; k < strings.size(); k++)
                    mcdcFuncHTML << strings[k] << ENDL;
                }
                else
                if (falseStr[i][j] == '1')
                {
                  vector<string> strings;
                  genHtmlSourceText(mcdcNodeInfo[j], 10, strings);
                  mcdcFuncHTML << "   True   " << strings[0] << ENDL;
                  for (int k = 1; k < strings.size(); k++)
                    mcdcFuncHTML << strings[k] << ENDL;
                }
              }

              mcdcFuncHTML << ENDL;
            }
          }
        }

        mcdcFuncHTML << "</pre></b></font></td>" << ENDL;
        mcdcFuncHTML << "<td width=50%><font size=\"5\"><b><pre>" << ENDL;

        if (newTrue.any())
        {
          for (i = 0; i < MAX_BITSET; i++)
          {
            if (newTrue.test(i))
            {
              mcdcFuncHTML << "<b>  Case " << cnt++ <<
                              //": T" << (char) ('1' + i) << "  " <<
                              ": T" << (i + 1) << "  " <<
                              trueStr[i] << "</b><br />" << ENDL;
              mcdcFuncHTML << "<b>  Result  Condition</b>" << ENDL;

              for (int j = 0; j < trueStr[i].length(); j++)
              {
                if (trueStr[i][j] == '0')
                {
                  vector<string> strings;
                  genHtmlSourceText(mcdcNodeInfo[j], 10, strings);
                  mcdcFuncHTML << "   False  " << strings[0] << ENDL;
                  for (int k = 1; k < strings.size(); k++)
                    mcdcFuncHTML << strings[k] << ENDL;
                }
                else
                if (trueStr[i][j] == '1')
                {
                  vector<string> strings;
                  genHtmlSourceText(mcdcNodeInfo[j], 10, strings);
                  mcdcFuncHTML << "   True   " << strings[0] << ENDL;
                  for (int k = 1; k < strings.size(); k++)
                    mcdcFuncHTML << strings[k] << ENDL;
                }
              }

              mcdcFuncHTML << ENDL;
            }
          }
        }

        mcdcFuncHTML << "</pre></b></font></td></tr>" << ENDL;
      }


      exprTree.destroyTree();
    }

    mcdcFuncHTML << "</table>" << ENDL;

    mcdcFuncHTML << ENDL;
  }

  // Save coverage statistics for computing percentages
  mcdcFunctionCoverageCount.push_back(mcdcFuncCoverCnt);
  mcdcFunctionOperandCount.push_back(mcdcFuncOperandCnt);
  mcdcTotalCoverageCount += mcdcFuncCoverCnt;
  mcdcTotalOperandCount += mcdcFuncOperandCnt;

  // Save MC/DC Info for current function
  mcdcHTMLInfo.push_back(mcdcFuncHTML.str());

  mcdcFuncHTML.str("");

  // Increment count of # MC/DC functions calculated
  ++mcdcCnt;

  // Make sure matching number of MC/DC info is present
  while (mcdcCnt++ < db.functionInfo.size())
  {
    // Save coverage statistics for computing percentages
    mcdcFunctionCoverageCount.push_back(0);
    mcdcFunctionOperandCount.push_back(0);

    // Save MC/DC Info for current function
    mcdcHTMLInfo.push_back("");
  }
}

void VampProcess::genHTML(string htmlName)
{
  string vrptName = htmlName + ".vrpt";
  string rptName = htmlName + ".rpt";
  htmlName += ".html";
  ofstream htmlFile (htmlName.c_str());
  ofstream vrptFile (vrptName.c_str());
  ofstream rptFile;
  int mapIndex = 0;
  if (mapInfo.size() == 0)
      gotPPMap = false;

  if (db.generateReport)
  {
    rptFile.open(rptName.c_str());
  }

  vrptFile << "{\n  \"file_name\": \"" << db.fileName << "\",\n";

  htmlFile << "<!DOCTYPE html>" << ENDL;
  htmlFile << "<html lang=\"en\">" << ENDL;
  htmlFile << "  <head>" << ENDL;
  htmlFile << "  <meta charset=\"utf-8\"/>" << ENDL;
  htmlFile << "    <title>" << db.fileName <<
              " Coverage Report (Instrumented by VAMP)</title>" << ENDL;

  // Insert styles needed to provide coverage summary for file
  if (db.totalStmtCount)
  {
    int coveragePercent = (1000 * totalStmtCoveredCount / 
                                  db.totalStmtCount + 5) / 10;

    htmlFile << htmlPercentageStyle("total_stmt_percent", coveragePercent);
  }

  if (branchTotalOperandCount)
  {
    int coveragePercent = (1000 * branchTotalCoverageCount / 
                                  branchTotalOperandCount + 5) / 10;

    htmlFile << htmlPercentageStyle("total_branch_percent", coveragePercent);
  }

  if (doMCDC)
  {
    if (mcdcTotalOperandCount)
    {
      int coveragePercent = (1000 * mcdcTotalCoverageCount / 
                                    mcdcTotalOperandCount + 5) / 10;

      htmlFile << htmlPercentageStyle("total_mcdc_percent", coveragePercent);
    }
  }

  if (doCC)
  {
    if (condTotalOperandCount)
    {
      int coveragePercent = (1000 * condTotalCoverageCount / 
                                    condTotalOperandCount + 5) / 10;

      htmlFile << htmlPercentageStyle("total_cond_percent", coveragePercent);
    }
  }

  // Insert styles needed to display percentages
  for (int i = 0; i < db.functionInfo.size(); i++)
  {
    if ((doStmtSingle || doStmtCount) && db.functionStmtCount[i])
    {
      int coveragePercent = (1000 * functionStmtCoverageCount[i] /
                                    db.functionStmtCount[i] + 5) / 10;

      htmlFile << htmlPercentageStyle(db.functionInfo[i].function +
                                      "_stmt_percent", coveragePercent);
    }

    if (doBranch && branchFunctionOperandCount[i])
    {
      int coveragePercent = (1000 * branchFunctionCoverageCount[i] / 
                                    branchFunctionOperandCount[i] + 5) / 10;

      htmlFile << htmlPercentageStyle(db.functionInfo[i].function +
                                      "_branch_percent", coveragePercent);
    }

    if (doMCDC)
    {
      if (mcdcFunctionOperandCount[i])
      {
        int coveragePercent = (1000 * mcdcFunctionCoverageCount[i] /
                                      mcdcFunctionOperandCount[i] + 5) / 10;

        htmlFile << htmlPercentageStyle(db.functionInfo[i].function +
                                        "_mcdc_percent", coveragePercent);
      }
    }

    if (doCC)
    {
      if (condFunctionOperandCount[i])
      {
        int coveragePercent = (1000 * condFunctionCoverageCount[i] / 
                                      condFunctionOperandCount[i] + 5) / 10;

        htmlFile << htmlPercentageStyle(db.functionInfo[i].function +
                                        "_cond_percent", coveragePercent);
      }
    }
  }

  if (db.combineHistory)
  {
    // Insert styles needed to display new coverage
    if ((doStmtSingle || doStmtCount) && db.totalStmtCount)
    {
      int coveragePercent = (1000 * newStmtCoveredCount / 
                                    db.totalStmtCount + 5) / 10;

      htmlFile << htmlPercentageStyle("new_stmt_percent", coveragePercent);
    }

    if (doBranch && branchTotalOperandCount)
    {
      int coveragePercent = (1000 * newBranchCoveredCount / 
                                    branchTotalOperandCount + 5) / 10;

      htmlFile << htmlPercentageStyle("new_branch_percent", coveragePercent);
    }

    if (doMCDC)
    {
      if (mcdcTotalOperandCount)
      {
        int coveragePercent = (1000 * mcdcNewCoverCount /
                                      mcdcTotalOperandCount + 5) / 10;

        htmlFile << htmlPercentageStyle("new_mcdc_percent", coveragePercent);
      }
    }

    if (doCC)
    {
      if (condTotalOperandCount)
      {
        int coveragePercent = (1000 * newCondCoveredCount / 
                                      condTotalOperandCount + 5) / 10;

        htmlFile << htmlPercentageStyle("new_cond_percent", coveragePercent);
      }
    }
  }

  htmlFile << "  </head>" << ENDL;

  htmlFile << "  <body>" << ENDL;
  htmlFile << "    <h1 align=\"center\">" << db.fileName <<
              " Coverage Report (Instrumented by VAMP)</h1>" << ENDL;
  htmlFile << "  <hr />" << ENDL;

  vrptFile << "  \"mcdc_stack_overflow\": ";

  if (doMCDC && hist.mcdcCount && hist.stackOverflow)
  {
    mcdcOpInfoType firstOperand =
            db.mcdcExprInfo[hist.stackOverflow - 1].opInfo.front();
    htmlFile << "<table border bgcolor=\"#d0d0d0\" " <<
                //"width=80% align=\"center\">" << ENDL;
                "align=\"center\">" << ENDL;
    htmlFile << "<tr><td bgcolor=#ffff00>" << ENDL;
    htmlFile << "<center><b><font size=\"6\" color=#ff0000>" <<
                "ERROR - MC/DC Expression Stack Overflow at "
                "<a href='#line_" <<
                firstOperand.lhsLoc.lhsLine << "'>line " <<
                firstOperand.lhsLoc.lhsLine << "</a><br />";
    htmlFile << "MC/DC Data Collection Terminated Early";
    htmlFile << "</font></b></center>\n";
    htmlFile << "</td></tr></table>\n";
    htmlFile << "<br /><br />\n";

    vrptFile << "true,\n";
  }
  else
  {
    vrptFile << "false,\n";
  }

  int coverageTypes = (doStmtSingle || doStmtCount) +
                      doBranch + (doMCDC || doCC);
  int coverageCnt = 100 / coverageTypes;     // Normally 33%
  int summaryCnt = 100 / (coverageTypes + 1);  // Normally 25%

  // Display overall summary of results
  htmlFile << "<h1><center>Overall Summary of Results for " <<
              db.fileName << "</center></h1>" << ENDL;

  htmlFile << "<table border rules=none frame=box bgcolor=\"#d0d0d0\" " <<
              "width=60% align=\"center\">" << ENDL;
  htmlFile << "<tr>";
  if (doStmtSingle || doStmtCount)
  {
    htmlFile << "<th width=" << coverageCnt << "%> Statement </th> <td></td>" << ENDL;
  }
  if (doBranch)
  {
    htmlFile << "<th width=" << coverageCnt << "%> Branch </th> <td></td>" << ENDL;
  }
  if (doMCDC)
  {
    htmlFile << "<th width=" << coverageCnt << "%> MC/DC </th> <td></td>" << ENDL;
  }
  if (doCC)
  {
    htmlFile << "<th width=" << coverageCnt << "%> Condition </th> <td></td>" << ENDL;
  }
  htmlFile << "</tr>" << ENDL;
  htmlFile << "<tr></tr>" << ENDL;
  htmlFile << "<tr></tr>" << ENDL;

  htmlFile << "<tr>" << ENDL;

  if (db.generateReport)
  {
    rptFile << "File Coverage Summary";
    if (doStmtSingle || doStmtCount)
      rptFile << db.reportSeparator << "Statement Covered" <<
                 db.reportSeparator << "Statement Total";
    if (doBranch)
      rptFile << db.reportSeparator << "Branch Covered" <<
                 db.reportSeparator << "Branch Total";
    if (doMCDC)
      rptFile << db.reportSeparator << "MCDC Covered" <<
                 db.reportSeparator << "MCDC Total";
    if (doCC)
      rptFile << db.reportSeparator << "Condition Covered" <<
                 db.reportSeparator << "Condition Total";
    rptFile << ENDL;
    rptFile << db.fileName;
  }

  if (doStmtSingle || doStmtCount)
  {
    htmlFile << "<td width=" << coverageCnt << "% style=\"white-space:nowrap\"> ";
    vrptFile << "  \"total_statement_count\": " << db.totalStmtCount << ",\n";
    vrptFile << "  \"total_statements_covered\": " << totalStmtCoveredCount << ",\n";

    if (db.totalStmtCount)
    {
      int coveragePercent = (1000 * totalStmtCoveredCount / 
                                      db.totalStmtCount + 5) / 10;
      htmlFile << "<div id=\"total_stmt_percent\">" <<
                  "<div id=\"total_stmt_percent_in\">" <<
                  coveragePercent << "% <font size=\"1\">&nbsp;&nbsp;(" <<
                  totalStmtCoveredCount << " / " <<
                  db.totalStmtCount <<
                  ")</font></div></div> </td> <td></td>" << ENDL;
    }
    else
    {
      htmlFile << "<center> No Statements </center>" <<
                  " </td> <td></td>" << ENDL;
    }

    if (db.generateReport)
    {
      rptFile << db.reportSeparator << totalStmtCoveredCount <<
                 db.reportSeparator << db.totalStmtCount;
    }
  }

  if (doBranch)
  {
    htmlFile << "<td width=" << coverageCnt << "% style=\"white-space:nowrap\"> ";

    vrptFile << "  \"total_branch_count\": " << branchTotalOperandCount << ",\n";
    vrptFile << "  \"total_branches_covered\": " << branchTotalCoverageCount << ",\n";

    if (branchTotalOperandCount)
    {
      int coveragePercent = (1000 * branchTotalCoverageCount / 
                                    branchTotalOperandCount + 5) / 10;
      htmlFile << "<div id=\"total_branch_percent\">" <<
                  "<div id=\"total_branch_percent_in\">" <<
                  coveragePercent << "% <font size=\"1\">&nbsp;&nbsp;(" <<
                  branchTotalCoverageCount << " / " <<
                  branchTotalOperandCount <<
                  ")</font></div></div> </td> <td></td>" << ENDL;
    }
    else
    {
      htmlFile << "<center>   No Branches  </center>" <<
                  " </td> <td></td>" << ENDL;
    }

    if (db.generateReport)
    {
      rptFile << db.reportSeparator << branchTotalCoverageCount <<
                 db.reportSeparator << branchTotalOperandCount;
    }
  }

  if (doMCDC)
  {
    htmlFile << "<td width=" << coverageCnt << "% style=\"white-space:nowrap\"> ";

    vrptFile << "  \"total_mcdc_count\": " << mcdcTotalOperandCount << ",\n";
    vrptFile << "  \"total_mcdc_covered\": " << mcdcTotalCoverageCount << ",\n";

    if (mcdcTotalOperandCount)
    {
      int coveragePercent = (1000 * mcdcTotalCoverageCount / 
                                    mcdcTotalOperandCount + 5) / 10;
      htmlFile << "<div id=\"total_mcdc_percent\">" <<
                  "<div id=\"total_mcdc_percent_in\">" <<
                  coveragePercent << "% <font size=\"1\">&nbsp;&nbsp;(" <<
                  mcdcTotalCoverageCount << " / " <<
                  mcdcTotalOperandCount <<
                  ")</font></div></div> </td> <td></td>" << ENDL;
    }
    else
    {
      htmlFile << "<center> No MC/DC Exprs </center>" <<
                  " </td> <td></td>" << ENDL;
    }

    if (db.generateReport)
    {
      rptFile << db.reportSeparator << mcdcTotalCoverageCount <<
                 db.reportSeparator << mcdcTotalOperandCount;
    }
  }

  if (doCC)
  {
    htmlFile << "<td width=" << coverageCnt << "% style=\"white-space:nowrap\"> ";

    vrptFile << "  \"total_condition_count\": " << condTotalOperandCount << ",\n";
    vrptFile << "  \"total_conditions_covered\": " << condTotalCoverageCount << ",\n";

    if (condTotalOperandCount)
    {
      int coveragePercent = (1000 * condTotalCoverageCount / 
                                    condTotalOperandCount + 5) / 10;
      htmlFile << "<div id=\"total_cond_percent\">" <<
                  "<div id=\"total_cond_percent_in\">" <<
                  coveragePercent << "% <font size=\"1\">&nbsp;&nbsp;(" <<
                  condTotalCoverageCount << " / " <<
                  condTotalOperandCount <<
                  ")</font></div></div> </td> <td></td>" << ENDL;
    }
    else
    {
      htmlFile << "<center> No Conditions  </center>" <<
                  " </td> <td></td>" << ENDL;
    }

    if (db.generateReport)
    {
      rptFile << db.reportSeparator << condTotalCoverageCount <<
                 db.reportSeparator << condTotalOperandCount;
    }
  }

  if (db.generateReport)
  {
    rptFile << ENDL;
  }

  htmlFile << "</tr>" << ENDL;
  //htmlFile << "</td></tr>" << ENDL;
  //htmlFile << "</table></table>" << ENDL;
  htmlFile << "</table>" << ENDL;
  htmlFile << "<br /><br />" << ENDL;

  // Display summary of results per function
  htmlFile << "<h3><center>Summary of Results for Each Function in " <<
              db.fileName << "</center></h3>" << ENDL;

  htmlFile << "<table border rules=none frame=box bgcolor=\"#d0d0d0\" " <<
              "width=80% align=\"center\">" << ENDL;
  //htmlFile << "<table border width=80% align=\"center\">" << ENDL;
  //htmlFile << "<tr><td>" << ENDL;
  //htmlFile << "<table bgcolor=\"#d0d0d0\" width=100% >" << ENDL;
  htmlFile << "<tr>" << ENDL;
  //htmlFile << "<th colspan = 10> <pre> Function   </pre> </th> <td></td>" << ENDL;
  //htmlFile << "<th colspan = 10> <pre> Statement  </pre> </th> <td></td>" << ENDL;
  //htmlFile << "<th colspan = 10> <pre> Branch     </pre> </th> <td></td>" << ENDL;
  //htmlFile << "<th colspan = 10> <pre> MC/DC      </pre> </th> <td></td>" << ENDL;
  htmlFile << "<th width=" << summaryCnt << "%> Function </th> <td></td>" << ENDL;
  if (doStmtSingle || doStmtCount)
  {
    htmlFile << "<th width=" << summaryCnt << "%> Statement </th> <td></td>" << ENDL;
  }
  if (doBranch)
  {
    htmlFile << "<th width=" << summaryCnt << "%> Branch </th> <td></td>" << ENDL;
  }
  if (doMCDC)
  {
    htmlFile << "<th width=" << summaryCnt << "%> MC/DC </th> <td></td>" << ENDL;
  }
  if (doCC)
  {
    htmlFile << "<th width=" << summaryCnt << "%> Condition </th> <td></td>" << ENDL;
  }
  htmlFile << "</tr>" << ENDL;
  htmlFile << "<tr></tr>" << ENDL;
  htmlFile << "<tr></tr>" << ENDL;

  if (db.generateReport)
  {
    rptFile << "Function Coverage Summary";
    if (doStmtSingle || doStmtCount)
      rptFile << db.reportSeparator << "Statement Covered" <<
                 db.reportSeparator << "Statement Total";
    if (doBranch)
      rptFile << db.reportSeparator << "Branch Covered" <<
                 db.reportSeparator << "Branch Total";
    if (doMCDC)
      rptFile << db.reportSeparator << "MCDC Covered" <<
                 db.reportSeparator << "MCDC Total";
    if (doCC)
      rptFile << db.reportSeparator << "Condition Covered" <<
                 db.reportSeparator << "Condition Total";
    rptFile << ENDL;
  }

  vrptFile << "  \"function_info\":\n  [\n";

  for (int i = 0; i < db.functionInfo.size(); i++)
  {
    string func = db.functionInfo[i].function;

    htmlFile << "<tr>" << ENDL;
    htmlFile << "<td width=" << summaryCnt << "%> <center> <a href='#func_" <<
                func << "'> " << func <<
                "</a> </center> </td> <td> </td>" << ENDL;

    if (db.generateReport)
    {
      // Add function name to report file
      rptFile << func;
    }

    if (i > 0)
      vrptFile << ",\n";
    vrptFile << "    \"" << func << "\", ";

    if (doStmtSingle || doStmtCount)
    {
      htmlFile << "<td width=" << summaryCnt << "% style=\"white-space:nowrap\"> ";

      vrptFile << functionStmtCoverageCount[i] << ", " <<
                  db.functionStmtCount[i] << ", ";

      if (db.functionStmtCount[i])
      {
        int coveragePercent = (1000 * functionStmtCoverageCount[i] / 
                                      db.functionStmtCount[i] + 5) / 10;
        htmlFile << "<div id=\"" << func << "_stmt_percent\">" <<
                    "<div id=\"" << func << "_stmt_percent_in\">" <<
                    coveragePercent << "% <font size=\"1\">&nbsp;&nbsp;(" <<
                    functionStmtCoverageCount[i] << " / " <<
                    db.functionStmtCount[i] <<
                    ")</font></div></div> </td> <td></td>" << ENDL;
      }
      else
      {
        htmlFile << "<center> No Statements </center>" <<
                    " </td> <td></td>" << ENDL;
      }

      if (db.generateReport)
      {
        rptFile << db.reportSeparator << functionStmtCoverageCount[i] <<
                   db.reportSeparator << db.functionStmtCount[i];
      }
    }
    else
    {
      vrptFile << "0, 0, ";
    }

    if (doBranch)
    {
      htmlFile << "<td width=" << summaryCnt << "% style=\"white-space:nowrap\"> ";

      vrptFile << branchFunctionCoverageCount[i] << ", " <<
                  branchFunctionOperandCount[i] << ", ";

      if (branchFunctionOperandCount[i])
      {
        int coveragePercent = (1000 * branchFunctionCoverageCount[i] / 
                                      branchFunctionOperandCount[i] + 5) / 10;
        htmlFile << "<div id=\"" << func << "_branch_percent\">" <<
                    "<div id=\"" << func << "_branch_percent_in\">" <<
                    coveragePercent << "% <font size=\"1\">&nbsp;&nbsp;(" <<
                    branchFunctionCoverageCount[i] << " / " <<
                    branchFunctionOperandCount[i] <<
                    ")</font></div></div> </td> <td></td>" << ENDL;
      }
      else
      {
        htmlFile << "<center> No Branches </center>" <<
                    " </td> <td></td>" << ENDL;
      }

      if (db.generateReport)
      {
        rptFile << db.reportSeparator << branchFunctionCoverageCount[i] <<
                   db.reportSeparator << branchFunctionOperandCount[i];
      }
    }
    else
    {
      vrptFile << "0, 0, ";
    }

    if (doMCDC)
    {
      htmlFile << "<td width=" << summaryCnt << "% style=\"white-space:nowrap\"> ";

      vrptFile << mcdcFunctionCoverageCount[i] << ", " <<
                  mcdcFunctionOperandCount[i];

      if (mcdcFunctionOperandCount[i])
      {
        int coveragePercent = (1000 * mcdcFunctionCoverageCount[i] / 
                                      mcdcFunctionOperandCount[i] + 5) / 10;
        htmlFile << "<div id=\"" << func << "_mcdc_percent\">" <<
                    "<div id=\"" << func << "_mcdc_percent_in\">" <<
                    coveragePercent << "% <font size=\"1\">&nbsp;&nbsp;(" <<
                    mcdcFunctionCoverageCount[i] << " / " <<
                    mcdcFunctionOperandCount[i] <<
                    ")</font></div></div> </td> <td></td>" << ENDL;
      }
      else
      {
        htmlFile << "<center> No MC/DC Exprs </center>" <<
                    " </td> <td></td>" << ENDL;
      }

      if (db.generateReport)
      {
        rptFile << db.reportSeparator << mcdcFunctionCoverageCount[i] <<
                   db.reportSeparator << mcdcFunctionOperandCount[i];
      }
    }
    else
    if (!doCC)
    {
      vrptFile << "0, 0";
    }

    if (doCC)
    {
      htmlFile << "<td width=" << summaryCnt << "% style=\"white-space:nowrap\"> ";

      vrptFile << condFunctionCoverageCount[i] << ", " <<
                  condFunctionOperandCount[i];

      if (condFunctionOperandCount[i])
      {
        int coveragePercent = (1000 * condFunctionCoverageCount[i] / 
                                      condFunctionOperandCount[i] + 5) / 10;
        htmlFile << "<div id=\"" << func << "_cond_percent\">" <<
                    "<div id=\"" << func << "_cond_percent_in\">" <<
                    coveragePercent << "% <font size=\"1\">&nbsp;&nbsp;(" <<
                    condFunctionCoverageCount[i] << " / " <<
                    condFunctionOperandCount[i] <<
                    ")</font></div></div> </td> <td></td>" << ENDL;
      }
      else
      {
        htmlFile << "<center> No Conditions  </center>" <<
                    " </td> <td></td>" << ENDL;
      }

      if (db.generateReport)
      {
        rptFile << db.reportSeparator << condFunctionCoverageCount[i] <<
                   db.reportSeparator << condFunctionOperandCount[i];
      }
    }

    htmlFile << "</tr>" << ENDL;
    //htmlFile << "</td></tr>" << ENDL;
    //htmlFile << "</table></table>" << ENDL;

    if (db.generateReport)
    {
      rptFile << ENDL;
    }
  }

  htmlFile << "</table>" << ENDL;
  htmlFile << "<br /><br />" << ENDL;

  if (db.combineHistory)
  {
    if (db.generateReport)
    {
      rptFile << "File Coverage New";
      if (doStmtSingle || doStmtCount)
        rptFile << db.reportSeparator << "Statement Covered" <<
                   db.reportSeparator << "Statement Total";
      if (doBranch)
        rptFile << db.reportSeparator << "Branch Covered" <<
                   db.reportSeparator << "Branch Total";
      if (doMCDC)
        rptFile << db.reportSeparator << "MCDC Covered" <<
                   db.reportSeparator << "MCDC Total";
      if (doCC)
        rptFile << db.reportSeparator << "Condition Covered" <<
                   db.reportSeparator << "Condition Total";
      rptFile << ENDL;
      rptFile << db.fileName;
    }

    // Display summary of results of new coverage
    htmlFile << "<h3><center>Summary of New Coverage for " <<
                db.fileName << "</center></h3>" << ENDL;

    htmlFile << "<table border rules=none frame=box bgcolor=\"#d0d0d0\" " <<
                "width=60% align=\"center\">" << ENDL;
    if (doStmtSingle || doStmtCount)
    {
      htmlFile << "<th width=" << coverageCnt << "%> Statement </th> <td></td>" << ENDL;
    }
    if (doBranch)
    {
      htmlFile << "<th width=" << coverageCnt << "%> Branch </th> <td></td>" << ENDL;
    }
    if (doMCDC)
    {
      htmlFile << "<th width=" << coverageCnt << "%> MC/DC </th> <td></td>" << ENDL;
    }
    if (doCC)
    {
      htmlFile << "<th width=" << coverageCnt << "%> Condition </th> <td></td>" << ENDL;
    }
    htmlFile << "</tr>" << ENDL;
    htmlFile << "<tr></tr>" << ENDL;
    htmlFile << "<tr></tr>" << ENDL;

    htmlFile << "<tr>" << ENDL;

    if (doStmtSingle || doStmtCount)
    {
      htmlFile << "<td width=" << coverageCnt << "% style=\"white-space:nowrap\"> ";

      if (db.totalStmtCount)
      {
        int coveragePercent = (1000 * newStmtCoveredCount / 
                                      db.totalStmtCount + 5) / 10;
        htmlFile << "<div id=\"new_stmt_percent\">" <<
                    "<div id=\"new_stmt_percent_in\">" <<
                    coveragePercent << "% <font size=\"1\">&nbsp;&nbsp;(" <<
                    newStmtCoveredCount << " / " <<
                    db.totalStmtCount <<
                    ")</font></div></div> </td> <td></td>" << ENDL;
      }
      else
      {
        htmlFile << "<center> No Statements </center>" <<
                    " </td> <td></td>" << ENDL;
      }

      if (db.generateReport)
      {
        rptFile << db.reportSeparator << newStmtCoveredCount <<
                   db.reportSeparator << db.totalStmtCount;
      }
    }

    if (doBranch)
    {
      htmlFile << "<td width=" << coverageCnt << "% style=\"white-space:nowrap\"> ";

      if (branchTotalOperandCount)
      {
        int coveragePercent = (1000 * newBranchCoveredCount / 
                                      branchTotalOperandCount + 5) / 10;
        htmlFile << "<div id=\"new_branch_percent\">" <<
                    "<div id=\"new_branch_percent_in\">" <<
                    coveragePercent << "% <font size=\"1\">&nbsp;&nbsp;(" <<
                    newBranchCoveredCount << " / " <<
                    branchTotalOperandCount <<
                    ")</font></div></div> </td> <td></td>" << ENDL;
      }
      else
      {
        htmlFile << "<center>   No Branches  </center>" <<
                    " </td> <td></td>" << ENDL;
      }

      if (db.generateReport)
      {
        rptFile << db.reportSeparator << newBranchCoveredCount <<
                   db.reportSeparator << branchTotalOperandCount;
      }
    }

    if (doMCDC)
    {
      htmlFile << "<td width=" << coverageCnt << "% style=\"white-space:nowrap\"> ";

      if (mcdcTotalOperandCount)
      {
        int coveragePercent = (1000 * mcdcNewCoverCount / 
                                      mcdcTotalOperandCount + 5) / 10;
        htmlFile << "<div id=\"new_mcdc_percent\">" <<
                    "<div id=\"new_mcdc_percent_in\">" <<
                    coveragePercent << "% <font size=\"1\">&nbsp;&nbsp;(" <<
                    mcdcNewCoverCount << " / " <<
                    mcdcTotalOperandCount <<
                    ")</font></div></div> </td> <td></td>" << ENDL;
      }
      else
      {
        htmlFile << "<center> No MC/DC Exprs </center>" <<
                    " </td> <td></td>" << ENDL;
      }

      if (db.generateReport)
      {
        rptFile << db.reportSeparator << mcdcNewCoverCount <<
                   db.reportSeparator << mcdcTotalOperandCount;
      }
    }

    if (doCC)
    {
      htmlFile << "<td width=" << coverageCnt << "% style=\"white-space:nowrap\"> ";

      if (condTotalOperandCount)
      {
        int coveragePercent = (1000 * newCondCoveredCount / 
                                      condTotalOperandCount + 5) / 10;
        htmlFile << "<div id=\"new_cond_percent\">" <<
                    "<div id=\"new_cond_percent_in\">" <<
                    coveragePercent << "% <font size=\"1\">&nbsp;&nbsp;(" <<
                    newCondCoveredCount << " / " <<
                    condTotalOperandCount <<
                    ")</font></div></div> </td> <td></td>" << ENDL;
      }
      else
      {
        htmlFile << "<center> No Conditions  </center>" <<
                    " </td> <td></td>" << ENDL;
      }

      if (db.generateReport)
      {
        rptFile << db.reportSeparator << newCondCoveredCount <<
                   db.reportSeparator << condTotalOperandCount;
      }
    }

    htmlFile << "</table>" << ENDL;
    htmlFile << "<br /><br />" << ENDL;

    if (db.generateReport)
    {
      rptFile << ENDL;
    }
  }

  // Display results for each function
  vector<functionInfoType>::iterator func = db.functionInfo.begin();
  vector<string>::iterator mcdcFuncHTML = mcdcHTMLInfo.begin();
  vector<string>::iterator condFuncHTML = condHTMLInfo.begin();
  vector<string>::iterator branchFuncHTML = branchHTMLInfo.begin();
  int lineColor = 0;
  int sourceColor = 0;
  int nextSourceColor = 0;
  bool oldLineCovered = false;
  bool newLineCovered = false;
  int whichStmt = 0;
  unsigned int curStmtCount;
  int whichLoop;
  int loopLine = 0;
  int loopCol = 0;

  if (doStmtCount)
  {
    // Search branchInfo for a "for" or "while" loop
    // Loops must be checked when doing statement counting, as the line after
    // the loop is instrumented and not the loop itself.  To show the proper
    // count, the sum of the count for the instrumented line before and the
    // line after is used.
    // FIXME (done): Maybe this isn't right either.  If we get to the loop 5 times,
    // and execute the inside only twice, we'll show a count of 5, not 7.
    for (whichLoop = 0; (whichLoop < db.branchInfo.size()) && 
                        (db.branchInfo[whichLoop].statementType != "for") &&
                        (db.branchInfo[whichLoop].statementType != "while");
         ++whichLoop) ;
    if (whichLoop < db.branchInfo.size())
    {
      int index = db.branchInfo[whichLoop].index;

      if (db.branchInfo[whichLoop].statementType == "for")
      {
        loopLine = db.forInfo[index].forLoc.lhsLine;
        loopCol = db.forInfo[index].forLoc.lhsCol;
#ifdef VAMP_DEBUG_BRANCH
CDBG << "For: " << loopLine << ", " << loopCol << ENDL;
#endif
      }
      else
      {
        loopLine = db.whileInfo[index].whileLoc.lhsLine;
        loopCol = db.whileInfo[index].whileLoc.lhsCol;
#ifdef VAMP_DEBUG_BRANCH
CDBG << "While: " << loopLine << ", " << loopCol << ENDL;
#endif
      }
    }
  }

  while (func < db.functionInfo.end())
  {
    ostringstream htmlLine;
    ostringstream htmlSrcLine;
    ostringstream htmlSource;
    ostringstream htmlNew;

    // Display function source code with highlights for statement and
    // branch coverage
    int col = func->loc.lhsCol;
    int srcLine;
    for (int line = func->loc.lhsLine; line <= func->loc.rhsLine; line++)
    {
      while (gotPPMap && ((mapIndex + 1) < mapInfo.size()) && (line >= mapInfo[mapIndex + 1].ppSrcLine))
      {
        ++mapIndex;
      }
      char lineNum[64];
      char srcLineNum[64];
      sprintf(lineNum, "<a name=\"line_%d\"></a>%5d", line, line);
      if (gotPPMap)
      {
          srcLine = line - mapInfo[mapIndex].ppSrcLine + mapInfo[mapIndex].srcLine;
          if (mapInfo[mapIndex].srcLine > 0)
            sprintf(srcLineNum, "%5d", srcLine);
          else
            sprintf(srcLineNum, "   -");
      }

      while (col <= attribs[line - 1].size())
      {
        if (attribs[line - 1][col - 1] & (STMT_NEW_LINE | STMT_NEW_STMT))
        {
          // Add new line number
          if (lineColor && (doStmtSingle || doStmtCount))
          {
            htmlLine << htmlBgColor(lineColor) << lineNum << "    </span>";
            if (gotPPMap)
                htmlSrcLine << htmlBgColor(lineColor) << srcLineNum << "    </span>";

          }
          else
          {
            htmlLine << lineNum;
            if (gotPPMap)
                htmlSrcLine << srcLineNum;
          }

          htmlLine << ENDL;
          if (gotPPMap)
              htmlSrcLine << ENDL;
          htmlSource << ENDL;
          htmlNew << ENDL;
          lineColor = 0;
          oldLineCovered = false;
          newLineCovered = false;
          sprintf(lineNum, "   + ");
          sprintf(srcLineNum, "   + ");
        }

        //if ((lineColor == 0) && (attribs[line - 1][col - 1] & STMT_CODE))
        if (attribs[line - 1][col - 1] & STMT_CODE)
        {
          if (attribs[line - 1][col - 1] & STMT_COVERED)
          {
            if (doStmtCount)
            {
              while ((line > db.instInfo[whichStmt].line) ||
                     ((line == db.instInfo[whichStmt].line) &&
                      (col >= db.instInfo[whichStmt].col)))
              {
                curStmtCount = hist.covCntInfo[whichStmt++];
#ifdef VAMP_DEBUG
CDBG << "Line: " << line << ", " << col << "; Statement: " << whichStmt << ", Count = " << curStmtCount << ENDL;
#endif
              }

              if ((line == loopLine) && (col == loopCol))
              {
                // Handle for/while loops by adding the previous count and
                // next count
                curStmtCount = hist.covCntInfo[whichStmt - 1] +
                               hist.covCntInfo[whichStmt];
#ifdef VAMP_DEBUG
CDBG << "Modified Line: " << line << ", " << col << "; Statement: " << whichStmt << ", Count = " << curStmtCount << ENDL;
CDBG << "Modified Line counts: " << hist.covCntInfo[whichStmt - 1] << " + " <<  hist.covCntInfo[whichStmt] << ENDL;
#endif

                // Search branchInfo for a "for" or "while" loop
                whichLoop++;
                while ((whichLoop < db.branchInfo.size()) && 
                       (db.branchInfo[whichLoop].statementType != "for") &&
                       (db.branchInfo[whichLoop].statementType != "while"))
                {
                  ++whichLoop;
                }

                if (whichLoop < db.branchInfo.size())
                {
                  int index = db.branchInfo[whichLoop].index;

                  // Save location of for/while loop
                  if (db.branchInfo[whichLoop].statementType == "for")
                  {
                    loopLine = db.forInfo[index].forLoc.lhsLine;
                    loopCol = db.forInfo[index].forLoc.lhsCol;
#ifdef VAMP_DEBUG
CDBG << "For: " << loopLine << ", " << loopCol << ENDL;
#endif
                  }
                  else
                  {
                    loopLine = db.whileInfo[index].whileLoc.lhsLine;
                    loopCol = db.whileInfo[index].whileLoc.lhsCol;
#ifdef VAMP_DEBUG
CDBG << "While: " << loopLine << ", " << loopCol << ENDL;
#endif
                  }
                }
              }
            }

            lineColor = BG_GREEN;  // Color line number green

            if (db.combineHistory)
            {
              if (attribs[line - 1][col - 1] & STMT_COVERED_OLD)
                oldLineCovered = true;
              if (attribs[line - 1][col - 1] & STMT_COVERED_NEW)
                newLineCovered = true;
            }
            else
            if (doStmtCount)
            {
              newLineCovered = true;
            }

            if ((attribs[line - 1][col - 1] & STMT_BR_COV_FULL) ==
                                              STMT_BR_COV_FULL)
            {
              nextSourceColor = BG_GREEN;  // Color condition green
            }
            else
            if (attribs[line - 1][col - 1] & STMT_BR_COV_TRUE)
            {
              nextSourceColor = BG_YELLOW;  // Color condition yellow
            }
            else
            if (attribs[line - 1][col - 1] & STMT_BR_COV_FALSE)
            {
              nextSourceColor = BG_ORANGE;  // Color condition orange
            }
            else
            {
              nextSourceColor = 0;  // Don't color character
            }
          }
          else
          if (doStmtSingle || doStmtCount)
          {
            lineColor = BG_RED;     // Color it red
            nextSourceColor = BG_RED;
            oldLineCovered = false;
            newLineCovered = false;
          }
        }
        else
        {
          nextSourceColor = 0;    // Remove color
        }

        if (nextSourceColor != sourceColor)
        {
          if (sourceColor)
          {
            // End old color
            htmlSource << "</span>";
          }

          if (nextSourceColor)
          {
            htmlSource << htmlBgColor(nextSourceColor);
          }

          sourceColor = nextSourceColor;
        }

        htmlSource << source[line - 1][col - 1];
        ++col;
      }

      // Add line number
      if (lineColor && (doStmtSingle || doStmtCount))
      {
        htmlLine << htmlBgColor(lineColor) << lineNum << "    </span>";
        if (gotPPMap)
            htmlSrcLine << htmlBgColor(lineColor) << srcLineNum << "    </span>";

        if (db.combineHistory)
        {
          // Add to column showing Previous/Current/Combined Run coverage
          // for this line
          htmlNew << ' ';
          if (oldLineCovered)
            htmlNew << htmlBgColor(BG_GREEN) << "++";
          else
            htmlNew << htmlBgColor(BG_RED) << "--";
          htmlNew << "</span>   ";
          if (newLineCovered)
          {
            htmlNew << htmlBgColor(BG_GREEN);
            if (doStmtCount)
            {
              htmlNew << curStmtCount;
            }
            else
            {
              htmlNew << "++";
            }
          }
          else
            htmlNew << htmlBgColor(BG_RED) << "--";
          htmlNew << "</span>   ";
          if (lineColor == BG_GREEN)
            htmlNew << htmlBgColor(BG_GREEN) << "++";
          else
            htmlNew << htmlBgColor(BG_RED) << "--";
          htmlNew << "</span>";
        }
        else
        if (doStmtCount)
        {
          // Add to column showing Current Run coverage for this line
          htmlNew << ' ';
          if (newLineCovered)
          {
            htmlNew << htmlBgColor(BG_GREEN);
            htmlNew << curStmtCount;
          }
          else
            htmlNew << htmlBgColor(BG_RED) << "--";
          htmlNew << "</span>   ";
        }
        lineColor = 0;
        oldLineCovered = false;
        newLineCovered = false;
      }
      else
      {
        htmlLine << lineNum;
        if (gotPPMap)
            htmlSrcLine << srcLineNum;
      }

      htmlLine << ENDL;
      if (gotPPMap)
          htmlSrcLine << ENDL;
      htmlSource << ENDL;
      htmlNew << ENDL;
      col = 1;
    }

    //htmlFile << "      <table bgcolor=\"#d0d0d0\" border=\"1\" align=\"center\">" << ENDL;
    //htmlFile << "      <table bgcolor=\"#e0ffff\" border=\"1\" align=\"center\">" << ENDL;
    htmlFile << "      <table bgcolor=\"#e0efff\" border=\"1\" align=\"center\">" << ENDL;
    htmlFile << "    <tr>" << ENDL;
    htmlFile << "      <td align=\"center\">" << ENDL;
    htmlFile << "        <pre><b>Preproc\nLine:</b></pre>" << ENDL;
    htmlFile << "      </td>" << ENDL;
    if (gotPPMap)
    {
        htmlFile << "      <td align=\"center\">" << ENDL;
        htmlFile << "        <pre><b>Source\nLine:</b></pre>" << ENDL;
        htmlFile << "      </td>" << ENDL;
    }
    htmlFile << "      <td align=\"center\">" << ENDL;
    htmlFile << "    <a name=\"func_" << func->function << "\"></a>" << ENDL;
    htmlFile << "    <b><font size=\"6\">Function: " <<
                func->function << "</font></b>" << ENDL;
    htmlFile << "      </td>" << ENDL;
    if (db.combineHistory && (doStmtSingle || doStmtCount))
    {
      htmlFile << "      <td align=\"center\">" << ENDL;
      htmlFile << "   <pre><b>Stmt Count\nPrev Curr Comb\nRuns Run  Runs</b></pre>" << ENDL;
      htmlFile << "      </td>" << ENDL;
    }
    else
    if (doStmtCount)
    {
      htmlFile << "      <td align=\"center\">" << ENDL;
      htmlFile << "   <pre>Stmt\nCount</pre>" << ENDL;
      htmlFile << "      </td>" << ENDL;
    }
    htmlFile << "    </tr>" << ENDL;
    htmlFile << "      <td>" << ENDL;
    htmlFile << "        <pre>" << ENDL;
    htmlFile << htmlLine.str();
    htmlFile << "        </pre>" << ENDL;
    htmlFile << "      </td>" << ENDL;
    if (gotPPMap)
    {
        htmlFile << "      <td>" << ENDL;
        htmlFile << "        <pre>" << ENDL;
        htmlFile << htmlSrcLine.str();
        htmlFile << "        </pre>" << ENDL;
        htmlFile << "      </td>" << ENDL;
    }
    htmlFile << "      <td>" << ENDL;
    htmlFile << "        <pre>" << ENDL;
    htmlFile << htmlSource.str();
    htmlFile << "        </pre>" << ENDL;
    htmlFile << "      </td>" << ENDL;
    //if (db.combineHistory && (doStmtSingle || doStmtCount))
    if (doStmtCount || (db.combineHistory && doStmtSingle))
    {
      //htmlFile << "      <td>" << ENDL;
      htmlFile << "      <td align=\"center\">" << ENDL;
      htmlFile << "        <pre>" << ENDL;
      htmlFile << htmlNew.str();
      htmlFile << "        </pre>" << ENDL;
      htmlFile << "      </td>" << ENDL;
    }
    htmlFile << "      </table>" << ENDL;

    htmlFile << "      <br clear=\"left\" />" << ENDL;
    htmlFile << "      <pre>" << ENDL;

    if (doBranch)
    {
      htmlFile << *branchFuncHTML;
    }

    if (doMCDC)
    {
      htmlFile << *mcdcFuncHTML;
    }

    if (doCC)
    {
      htmlFile << *condFuncHTML;
    }

    htmlFile << "      </pre>" << ENDL;

    htmlLine.str("");
    htmlSrcLine.str("");
    htmlSource.str("");
    ++func;

    if (doMCDC)
    {
      ++mcdcFuncHTML;
    }

    if (doCC)
    {
      ++condFuncHTML;
    }
    ++branchFuncHTML;
  }

  htmlFile << "  </body>" << ENDL;
  htmlFile << "</html>" << ENDL;
  htmlFile.close();

  vrptFile << "\n  ]\n}\n";
  vrptFile.close();

  if (db.generateReport)
  {
    rptFile.close();
  }
}

#ifndef USE_QT
int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    llvm::errs() << "Usage: " << argv[0] << " <JSON file>" << ENDL;
    return EXIT_FAILURE;
  }

  ostringstream cfgErr;
  ConfigFile cf(&cfgErr);
  VAMP_REPORT_CONFIG vo;

  // FIXME - Parse this before call to processFile and pass resulting vo in
  if (!cf.ParseReportConfigFile("vamp_process.cfg", vo))
  {
      llvm::errs() << "Bad vamp_process.cfg " << cfgErr.str().data() << ENDL;
  }

  VampProcess vp(&llvm::outs(), &llvm::errs());
  if (!vp.processFile(argv[1], vo))
  {
    llvm::errs() << "Processing " << argv[1] << " failed\n";
    return EXIT_SUCCESS;
  }

  return EXIT_FAILURE;
}
#endif

