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

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <fstream>
#include "json.h"
#include "clang/Frontend/LangStandard.h"

#define MCDC_STACK_SIZE 4

typedef struct {
    // From vamp.cfg
    bool doStmtSingle;
    bool doStmtCount;
    bool doBranch;
    bool doMCDC;
    bool doCC;
    string saveDirectory;
    string saveSuffix;
    int mcdcStackSize;
    clang::LangStandard::Kind langStandard;
} VAMP_CONFIG;

typedef struct {
    // From vamp_process.cfg
    bool combineHistory;
    bool generateReport;
    bool showTestCases;
    string histDirectory;
    string reportSeparator;
    string htmlDirectory;
    string htmlSuffix;
} VAMP_REPORT_CONFIG;

typedef struct {
    bool doPreProcess;
    bool useVampPreprocessor;
    bool retainComments;
    bool showLineMarkers;
    string preProcPath;
    string preProcArguments;
    string preProcDirectory;
    string preProcSuffix;
    vector<string> includePaths;
    vector<string> definesList;
} VAMP_PREPROC_CONFIG;

class ConfigFile
{
public:
    //ConfigFile(llvm::raw_ostream *errStr) : vampErr(errStr)
    ConfigFile(std::ostringstream *errStr) : vampErr(errStr)
    {
    }
    bool ParseConfigFile(string fileName, VAMP_CONFIG &vo);
    bool ParseReportConfigFile(string fileName, VAMP_REPORT_CONFIG &vpo);

private :
    void ParseConfig(Json &n, VAMP_CONFIG &vo);
    void ParseReportConfig(Json &n, VAMP_REPORT_CONFIG &vpo);

//    llvm::raw_ostream *vampErr;
    std::ostringstream *vampErr;
};

#endif // CONFIGFILE_H
