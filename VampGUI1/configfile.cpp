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

#include "configfile.h"
#ifdef USE_QT
#include <QTextStream>
#include <QMessageBox>
#include <QFile>
#endif

// Load configuration info from JSON config file
void ConfigFile::ParseConfig(Json &n, VAMP_CONFIG &vo)
{
    vector<JsonNode>::iterator i = n.jsonNodes.begin();

    while (i != n.jsonNodes.end())
    {
        // get the node name and value as a string
        string nodeName = i->name().data();

        // recursively call ourselves to dig deeper into the tree
        if (nodeName == "do_statement_single")
            vo.doStmtSingle = i->as_bool();
        else
        if (nodeName == "do_statement_count")
            vo.doStmtCount = i->as_bool();
        else
        if (nodeName == "do_branch")
            vo.doBranch = i->as_bool();
        else
        if (nodeName == "do_MCDC")
            vo.doMCDC = i->as_bool();
        else
        if (nodeName == "do_condition")
            vo.doCC = i->as_bool();
        else
        if (nodeName == "save_directory")
            vo.saveDirectory = i->as_string();
        else
        if (nodeName == "save_suffix")
            vo.saveSuffix = i->as_string();
        else
        if (nodeName == "mcdc_stack_size")
            vo.mcdcStackSize = i->as_int();
        else
        if (nodeName == "lang_standard")
        {
            string langStd = i->as_string();
            if (langStd == "lang_c89")
            {
                vo.langStandard = clang::LangStandard::lang_c89;
            }
            else
            if (langStd == "lang_gnu89")
            {
                vo.langStandard = clang::LangStandard::lang_gnu89;
            }
            else
            if (langStd == "lang_c99")
            {
                vo.langStandard = clang::LangStandard::lang_c99;
            }
            else
            if (langStd == "lang_gnu99")
            {
                vo.langStandard = clang::LangStandard::lang_gnu99;
            }
            else
            if (langStd == "lang_c11")
            {
                vo.langStandard = clang::LangStandard::lang_c11;
            }
            else
            if (langStd == "lang_gnu11")
            {
                vo.langStandard = clang::LangStandard::lang_gnu11;
            }
            else
            {
#ifdef USE_QT
                QMessageBox::warning(0, QString("Warning"), QString("Unknown vamp.cfg lang_standard: %1").arg(QString::fromStdString(langStd)));
#else
                *vampErr << "Unknown vamp.cfg lang_standard: " << langStd.c_str() << endl;
#endif
            }
        }
        else
        {
#ifdef USE_QT
            QMessageBox::warning(0, QString("Warning"), QString("Unknown vamp.cfg node: %1").arg(QString::fromStdString(nodeName)));
#else
            *vampErr << "Unknown vamp.cfg node: " << nodeName << endl;
#endif
        }

        ++i;
    }
}

// Open and parse configuration info from JSON config file
bool ConfigFile::ParseConfigFile(string fileName, VAMP_CONFIG &vo)
{
    // Set default values which can be overridden by config file
    vo.doStmtSingle = true;
    vo.doStmtCount = false;
    vo.doBranch = true;
    vo.doCC = false;
    vo.doMCDC = true;
    vo.saveDirectory = "VAMP_INST";
    vo.saveSuffix = "";
    vo.mcdcStackSize = MCDC_STACK_SIZE;
    vo.langStandard = clang::LangStandard::lang_c89;

#ifdef USE_QT
    QFile jsonFile(QString::fromStdString(fileName));
    if (!jsonFile.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox::critical(0, QString("Error"), QString("Cannot read file %1:\n%2.")
                                                   .arg(QString::fromStdString(fileName))
                                                   .arg(jsonFile.errorString()));
        return false;
    }

    QTextStream in(&jsonFile);

    // Read file into a string
    QString jsonSource = in.readAll();
#else
    std::ifstream jsonFile(fileName.c_str());

    if (!jsonFile.is_open())
    {
        *vampErr << "Configuration file vamp.cfg not found - using default values" << endl;

#ifdef VAMP_DEBUG
  CDBG << "doStmtSingle: " << (vo.doStmtSingle ? "true" : "false") << endl;
  CDBG << "doStmtCount: " << (vo.doStmtCount ? "true" : "false") << endl;
  CDBG << "doBranch: " << (vo.doBranch ? "true" : "false") << endl;
  CDBG << "doMCDC: " << (vo.doMCDC ? "true" : "false") << endl;
  CDBG << "doCC: " << (vo.doCC ? "true" : "false") << endl;
  CDBG << "saveDirectory: " << vo.saveDirectory << endl;
  CDBG << "saveSuffix: " << vo.saveSuffix << endl;
  CDBG << "mcdcStackSize: " << vo.mcdcStackSize << endl;
  CDBG << "includePaths:" << endl;
  vector<string>::iterator it;
  for (it = vo.includePaths.begin(); it != vo.includePaths.end(); it++)
    CDBG << *it << endl;
#endif

        return false;
    }

    // Read file into a string 
    string jsonSource((istreambuf_iterator<char>(jsonFile)),
                       istreambuf_iterator<char>());
#endif

    jsonFile.close();

    // Now parse it
    std::ostringstream jsonErr;
    Json json(&jsonErr);
    try
    {
#ifdef USE_QT
      json.ParseJson(jsonSource.toStdString());
#else
      json.ParseJson(jsonSource);
#endif
    }
    catch(int i)
    {
#ifdef USE_QT
        QMessageBox::warning(0, QString("Error"), QString("Bad config file: %1\n%2").arg(jsonSource).arg(QString::fromStdString(jsonErr.str())));
#else
        *vampErr << "Bad config file: " << jsonSource << endl;
        *vampErr << jsonErr;
#endif
        return false;
    }

    ParseConfig(json, vo);

#ifdef VAMP_DEBUG
    cerr << "doStmtSingle: " << (vo.doStmtSingle ? "true" : "false") << endl;
    cerr << "doStmtCount: " << (vo.doStmtCount ? "true" : "false") << endl;
    cerr << "doBranch: " << (vo.doBranch ? "true" : "false") << endl;
    cerr << "doMCDC: " << (vo.doMCDC ? "true" : "false") << endl;
    cerr << "doCC: " << (vo.doCC ? "true" : "false") << endl;
    cerr << "saveDirectory: " << vo.saveDirectory << endl;
    cerr << "saveSuffix: " << vo.saveSuffix << endl;
    cerr << "mcdcStackSize: " << vo.mcdcStackSize << endl;
#endif

    return true;
}

// Load report configuration info from JSON config file
void ConfigFile::ParseReportConfig(Json &n, VAMP_REPORT_CONFIG &vpo)
{
    vector<JsonNode>::iterator i = n.jsonNodes.begin();

    while (i != n.jsonNodes.end())
    {
        // get the node name and value as a string
        string nodeName = i->name().data();

        // recursively call ourselves to dig deeper into the tree
        if (nodeName == "combine_history")
            vpo.combineHistory = i->as_bool();
        else
        if (nodeName == "show_test_cases")
            vpo.showTestCases = i->as_bool();
        else
        if (nodeName == "do_report")
            vpo.generateReport = i->as_bool();
        else
        if (nodeName == "report_separator")
            vpo.reportSeparator = i->as_string();
        else
        if (nodeName == "hist_directory")
            vpo.histDirectory = i->as_string();
        else
        if (nodeName == "html_directory")
            vpo.htmlDirectory = i->as_string();
        else
        if (nodeName == "html_suffix")
           vpo.htmlSuffix = i->as_string();

        ++i;
    }
}

// Open and parse configuration info from JSON config file
bool ConfigFile::ParseReportConfigFile(string fileName, VAMP_REPORT_CONFIG &vpo)
{
    // Set default values which can be overridden by config file
    vpo.histDirectory = ".";
    vpo.combineHistory = true;
    vpo.showTestCases = true;
    vpo.generateReport = true;
    vpo.reportSeparator = ",";
    vpo.htmlDirectory = ".";
    vpo.htmlSuffix = "";

#ifdef USE_QT
    QFile jsonFile(QString::fromStdString(fileName));
    if (!jsonFile.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox::critical(0, QString("Error"), QString("Cannot read file %1:\n%2.")
                                                   .arg(QString::fromStdString(fileName))
                                                   .arg(jsonFile.errorString()));
        return false;
    }

    QTextStream in(&jsonFile);

    // Read file into a string
    QString jsonSource = in.readAll();
#else
    ifstream jsonFile(fileName.data());

    if (!jsonFile.is_open())
    {
        *vampErr << "Cannot read " << fileName << " - using default values" << endl;
        return false;
    }

    // Read file into a string 
    string jsonSource((istreambuf_iterator<char>(jsonFile)),
                       istreambuf_iterator<char>());
#endif

    jsonFile.close();

    // Now parse it
    std::ostringstream jsonErr;
    Json json(&jsonErr);
    try
    {
#ifdef USE_QT
      json.ParseJson(jsonSource.toStdString());
#else
      json.ParseJson(jsonSource);
#endif
    }
    catch(int i)
    {
#ifdef USE_QT
        QMessageBox::warning(0, QString("Error"), QString("Bad config file: %1\n%2").arg(jsonSource).arg(QString::fromStdString(jsonErr.str())));
#else
        *vampErr << "Bad config file: " << jsonSource << endl;
        *vampErr << jsonErr;
#endif

        return false;
    }

    ParseReportConfig(json, vpo);

#ifdef VAMP_DEBUG
  cerr << "htmlistDirectory: " << vpo.htmlistDirectory << endl;
  cerr << "combineHistory: " << (vpo.combineHistory ? "true" : "false") << endl;
  cerr << "showTestCases: " << (vpo.showTestCases ? "true" : "false") << endl;
  cerr << "generateReport: " << (vpo.generateReport ? "true" : "false") << endl;
  cerr << "reportSeparator: \"" << vpo.reportSeparator << "\"" << endl;
  cerr << "htmlDirectory: " << vpo.htmlDirectory << endl;
  cerr << "htmlSuffix: " << vpo.htmlSuffix << endl;
//  cerr << "includePaths:" << endl;
//  vector<QString>::iterator it;
//  for (it = vpo.includePaths.begin(); it != vpo.includePaths.end(); it++)
//    cerr << *it << endl;
#endif

return true;
}

