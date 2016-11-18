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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "vampconfigdialog.h"
#include "ui_vampconfigdialog.h"
#include "json.h"
#include "vamp.h"
#include "vamp_process.h"
#include "vamp_preprocessor.h"
#include "splash.h"
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <QCursor>
#include <QProgressDialog>
#include <QDateTime>
#include <QDir>
//#include <QtPrintSupport/QPrinter>
//#include <QtPrintSupport/QPrintDialog>
#include <QPrinter>
#include <QPrintDialog>
#include <QClipboard>
#include <QMimeData>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    createContextActions();

    // Resize search buttons
    int height = ui->backPushButton->height();
    ui->backPushButton->setFixedWidth(height);
    ui->forwardPushButton->setFixedWidth(height);

    ui->outputTextEdit->setReadOnly(true);
    ui->errorTextEdit->setReadOnly(true);

    // Hide widgets until project opened or created
/*    ui->projectGroupBox->hide();
    ui->projectTreeView->hide();
    ui->resultsGroupBox->hide();
    ui->tabWidget->hide();
    ui->errorsTab->hide();
*/
    projectChanged = false;
    vcDataChanged = false;
    vcReportDataChanged = false;
    vcPreProcDataChanged = false;
//    ui->menu_Project->setVisible(false);
//    ui->splitter->sizeHint()

//    ui->backPushButton->setDefaultAction(ui->webView->pageAction(QWebPage::Back));
    ui->webView->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    ui->webView->setHtml(splash);
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::isAbsolutePath(string path)
{
#ifdef _WIN32
    return ((path[0] == '\\') ||
            (path[0] == '/') ||
            (path[1] == ':'));
#else
    return (path[0] == '/');
#endif
}

// For Windows systems, replace all Windows blackslashes with forward slashes
// If addSlash is true, make sure the path ends with a '/'
// If addSlash is false, remove any ending '/'
QString MainWindow::fixPath(QString qpath, bool addSlash)
{
#ifdef _WIN32
    // Replace all Windows blackslashes with forward slashes
    qpath.replace("\\", "/");
#endif
    if (addSlash)
    {
        if (qpath.right(1) != "/")
        {
            // Add trailing slash
            qpath += "/";
        }
    }
    else
    {
        while (qpath.right(1) == "/")
        {
            // Remove trailing slash
            qpath = qpath.left(qpath.length() - 1);
        }
    }

    return qpath;
}

QString MainWindow::fixPath(string path, bool addSlash)
{
    QString qpath = QString::fromStdString(path);
    return fixPath(qpath, addSlash);
}

// Return minimum unsigned type that can hold val
QString MainWindow::getMinType(int val)
{
    if (val < 256)
        return "unsigned char";
    else
    if (val < 65536)
        return "unsigned short";
    else
        return "unsigned int";
}

// Close Project selected from menu
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (saveProject(true))
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

// Set default vamp.cfg info
void MainWindow::setDefaultVcData(VAMP_CONFIG &vcData)
{
    vcData.doStmtSingle = true;
    vcData.doStmtCount = false;
    vcData.doMCDC = true;
    vcData.doCC = false;
    vcData.doBranch = true;
    vcData.saveDirectory = "VAMP_INST";
    vcData.saveSuffix = "";
    vcData.mcdcStackSize = 4;
    vcData.langStandard = clang::LangStandard::lang_c89;
}

// Set default vamp_process.cfg info
void MainWindow::setDefaultVcReportData(VAMP_REPORT_CONFIG &vcReportData)
{
    vcReportData.histDirectory = ".";
    vcReportData.combineHistory = true;
    vcReportData.showTestCases = true;
    vcReportData.generateReport = true;
    vcReportData.reportSeparator = ",";
    vcReportData.htmlDirectory = "VAMP_HTML";
    vcReportData.htmlSuffix = "";
}

// Set default preprocessor info (kept in .vproj file)
void MainWindow::setDefaultVcPreProcData(VAMP_PREPROC_CONFIG &vcPreProcData)
{
    vcPreProcData.doPreProcess = true;
    vcPreProcData.useVampPreprocessor = true;
    vcPreProcData.retainComments = true;
    vcPreProcData.showLineMarkers = true;
    vcPreProcData.preProcPath = "clang";
    vcPreProcData.preProcArguments = "-E -P %infile%";
    vcPreProcData.preProcDirectory = "VAMP_PP";
    vcPreProcData.preProcSuffix = "";
    vcPreProcData.includePaths.clear();
    vcPreProcData.definesList.clear();
}

// Save vamp.cfg data
void MainWindow::saveVcData(void)
{
    // Open vamp.cfg for output
    QString vampCfgName(projectDirName + "vamp.cfg");
    QFile vcDataFile(vampCfgName);
    if (!vcDataFile.open(QFile::WriteOnly | QFile::Truncate))
    {
        QMessageBox::critical(this, tr("Error"), tr("Cannot create file %1:\n%2")
                             .arg(vampCfgName)
                             .arg(vcDataFile.errorString()));
        return;
    }

    // Write vamp.cfg data as a JSON database
    QTextStream out(&vcDataFile);
    out << "{\n";
    out << "  \"do_statement_single\": " << (vcData.doStmtSingle ? "true" : "false") << ",\n";
    out << "  \"do_statement_count\": " << (vcData.doStmtCount ? "true" : "false") << ",\n";
    out << "  \"do_branch\": " << (vcData.doBranch ? "true" : "false") << ",\n";
    out << "  \"do_MCDC\": " << (vcData.doMCDC ? "true" : "false") << ",\n";
    out << "  \"do_condition\": " << (vcData.doCC ? "true" : "false") << ",\n";
    out << "  \"save_directory\": \"" << fixPath(vcData.saveDirectory, false) << "\",\n";
    out << "  \"save_suffix\": \"" << QString::fromStdString(vcData.saveSuffix) << "\",\n";
    out << "  \"mcdc_stack_size\": " << vcData.mcdcStackSize << ",\n";
    if (vcData.langStandard == clang::LangStandard::lang_c89)
        out << "  \"lang_standard\": \"lang_c89\"\n";
    else
    if (vcData.langStandard == clang::LangStandard::lang_gnu89)
        out << "  \"lang_standard\": \"lang_gnu89\"\n";
    else
    if (vcData.langStandard == clang::LangStandard::lang_c99)
        out << "  \"lang_standard\": \"lang_c99\"\n";
    else
    if (vcData.langStandard == clang::LangStandard::lang_gnu99)
        out << "  \"lang_standard\": \"lang_gnu99\"\n";
    else
    if (vcData.langStandard == clang::LangStandard::lang_c11)
        out << "  \"lang_standard\": \"lang_c11\"\n";
    else
    if (vcData.langStandard == clang::LangStandard::lang_gnu11)
        out << "  \"lang_standard\": \"lang_gnu11\"\n";
    else
        out << "  \"lang_standard\": \"unknown\"\n";
    out << "}\n";
    out.flush();
    vcDataChanged = false;
}

// Write vamp_process.cfg data
void MainWindow::saveVcReportData(void)
{
    // Open vamp_process.cfg for output
    QString vampProcessCfgName(projectDirName + "vamp_process.cfg");
    QFile vcReportDataFile(vampProcessCfgName);
    if (!vcReportDataFile.open(QFile::WriteOnly | QFile::Truncate))
    {
        QMessageBox::critical(this, tr("Error"), tr("Cannot create file %1:\n%2")
                             .arg(vampProcessCfgName)
                             .arg(vcReportDataFile.errorString()));
        return;
    }

    // Write vamp_process.cfg data as a JSON database
    QTextStream out(&vcReportDataFile);
    out << "{\n";
    out << "  \"hist_directory\": \"" << fixPath(vcReportData.histDirectory, false) << "\",\n";
    out << "  \"combine_history\": " << (vcReportData.combineHistory ? "true" : "false") << ",\n";
    out << "  \"show_test_cases\": " << (vcReportData.showTestCases ? "true" : "false") << ",\n";
    out << "  \"do_report\": " << (vcReportData.generateReport ? "true" : "false") << ",\n";
    out << "  \"report_separator\": \"" << QString::fromStdString(vcReportData.reportSeparator) << "\",\n";
    out << "  \"html_directory\": \"" << fixPath(vcReportData.htmlDirectory, false) << "\",\n";
    out << "  \"html_suffix\": \"" << QString::fromStdString(vcReportData.htmlSuffix) << "\"\n";
    out << "}\n";
    out.flush();
    vcReportDataChanged = false;
}

// Save project file
void MainWindow::saveProjectData(void)
{
    QStringList fileList;
    genFileList("", QModelIndex(), fileList);

    // Open project file for output
    QFile projectFile(projectPath);
    if (!projectFile.open(QFile::WriteOnly | QFile::Truncate))
    {
        QMessageBox::critical(this, tr("Error"), tr("Cannot create file %1:\n%2")
                             .arg(projectPath)
                             .arg(projectFile.errorString()));
        return;
    }

    // Write project file as JSON database
    QTextStream out(&projectFile);
    out << "{\n";
    out << "  \"base_directory\": \"" << baseDir << "\",\n";
    out << "  \"preprocess_files\": " << (vcPreProcData.doPreProcess ? "true,\n" : "false,\n");
    out << "  \"use_vamp_preprocessor\": " << (vcPreProcData.useVampPreprocessor ? "true,\n" : "false,\n");
    out << "  \"retain_comments\": " << (vcPreProcData.retainComments ? "true,\n" : "false,\n");
    out << "  \"show_line_markers\": " << (vcPreProcData.showLineMarkers ? "true,\n" : "false,\n");
    out << "  \"preprocess_path\": \"" << QString::fromStdString(vcPreProcData.preProcPath) << "\",\n";
    out << "  \"preprocess_arguments\": \"" << QString::fromStdString(vcPreProcData.preProcArguments) << "\",\n";
    out << "  \"preprocess_directory\": \"" << fixPath(vcPreProcData.preProcDirectory, false) << "\",\n";
    out << "  \"preprocess_suffix\": \"" << QString::fromStdString(vcPreProcData.preProcSuffix) << "\",\n";
    out << "  \"project_files\":\n  [\n";
    for (int i = 0; i < fileList.size(); ++i)
    {
        if (i > 0)
            out << ",\n";
        out << "    \"" << fileList[i] << "\"";
    }
    out << "\n  ],\n";
    out << "  \"include_paths\":\n  [\n";
    for (int i = 0; i < vcPreProcData.includePaths.size(); ++i)
    {
        if (vcPreProcData.includePaths[i] != "")
        {
            if (i > 0)
                out << ",\n";
            out << "    \"" << fixPath(vcPreProcData.includePaths[i], false) << "\"";
        }
    }
    out << "\n  ],\n";
    out << "  \"defines_list\":\n  [\n";
    for (int i = 0; i < vcPreProcData.definesList.size(); ++i)
    {
        if (vcPreProcData.definesList[i] != "")
        {
            if (i > 0)
                out << ",\n";
            out << "    \"" << QString::fromStdString(vcPreProcData.definesList[i]) << "\"";
        }
    }
    out << "\n  ]\n}\n";
    out.flush();
    projectChanged = false;
    vcPreProcDataChanged = false;
}

// Load specified project file
void MainWindow::loadProjectData(QString fileName)
{
    // Open project file for input
    QFile jsonFile(fileName);
    if (!jsonFile.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox::critical(0, QString("Error"), QString("Cannot read file %1:\n%2")
                                                   .arg(fileName)
                                                   .arg(jsonFile.errorString()));
        return;
    }

    QTextStream in(&jsonFile);

    // Read file into a string
    QString jsonSource = in.readAll();

    jsonFile.close();

    // Now parse it
    ostringstream jsonErr;
    Json json(&jsonErr);
    try
    {
      json.ParseJson(jsonSource.toStdString());
    }
    catch(int i)
    {
        QMessageBox::warning(0, QString("Error"), QString("Bad project file: %1\n%2").arg(fileName).arg(QString::fromStdString(jsonErr.str())));
        return;
    }

    // Parse project file
    vector<JsonNode>::iterator i = json.jsonNodes.begin();
    int cnt = 0;

    while (i != json.jsonNodes.end())
    {
        // get the node name and value as a string
        string nodeName = i->name();

        if (nodeName == "base_directory")
        {
            projectBaseDir = fixPath(i->as_string(), false);
            baseDir = projectBaseDir;
        }
        else
        if (nodeName == "preprocess_files")
        {
            vcPreProcData.doPreProcess = i->as_bool();
        }
        else
        if (nodeName == "use_vamp_preprocessor")
        {
            vcPreProcData.useVampPreprocessor = i->as_bool();
        }
        else
        if (nodeName == "retain_comments")
        {
            vcPreProcData.retainComments = i->as_bool();
        }
        else
        if (nodeName == "show_line_markers")
        {
            vcPreProcData.showLineMarkers = i->as_bool();
        }
        else
        if (nodeName == "preprocess_path")
        {
            vcPreProcData.preProcPath = i->as_string();
        }
        else
        if (nodeName == "preprocess_arguments")
        {
            vcPreProcData.preProcArguments = i->as_string();
        }
        else
        if (nodeName == "preprocess_directory")
        {
            vcPreProcData.preProcDirectory = i->as_string();
        }
        else
        if (nodeName == "preprocess_suffix")
        {
            vcPreProcData.preProcSuffix = i->as_string();
        }
        else
        if (nodeName == "include_paths")
        {
            vector<JsonNode> incPaths;
            json.ParseArray(i->as_string(), incPaths);

            // Load include paths from JSON config file
            vector<JsonNode>::const_iterator j = incPaths.begin();

            while (j != incPaths.end())
            {
                if (j->nodeData != "")
                    vcPreProcData.includePaths.push_back(j->nodeData);
                ++j;
            }
        }
        else
        if (nodeName == "defines_list")
        {
            vector<JsonNode> defsList;
            json.ParseArray(i->as_string(), defsList);

            // Load include paths from JSON config file
            vector<JsonNode>::const_iterator j = defsList.begin();

            while (j != defsList.end())
            {
                if (j->nodeData != "")
                    vcPreProcData.definesList.push_back(j->nodeData);
                ++j;
            }
        }
        else
        if (nodeName == "project_files")
        {
            // Get list of project files
            vector<JsonNode> projFiles;
            json.ParseArray(i->as_string(), projFiles);
            vector<JsonNode>::const_iterator j = projFiles.begin();
//            ui->projectTreeView->setCursor(Qt::WaitCursor);
//            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
            QProgressDialog progressDialog(this,
                                           Qt:: Dialog |
                                           //Qt:: FramelessWindowHint |
                                           Qt:: WindowTitleHint |
                                           Qt:: CustomizeWindowHint);

            progressDialog.setCancelButtonText(0);
            progressDialog.setRange(0, projFiles.size());
            progressDialog.setWindowTitle(tr("Loading Project"));

            while (j != projFiles.end())
            {
                if (j->nodeData != "")
                {
                    QStringList fileInfo;
                    QString fName = QString::fromStdString(j->nodeData);
                    progressDialog.setValue(cnt++);
                    progressDialog.setLabelText(tr("Loading %1...")
                                                .arg(fName));
                    qApp->processEvents();

                    fileInstInfo instInfo;
                    loadFileInfo(fName, instInfo);
                    getFileStats(instInfo, fileInfo);
                    insertPath(fName, fileInfo);
                }
                ++j;
            }

            // Recompute base path since something's been added to tree
            getBasePath();
            //cout << "BaseDir = " << baseDir.toStdString() << endl << flush;
        }
        else
        {
            QMessageBox::warning(0, QString("Warning"), QString("Unknown project node: %1")
                                 .arg(QString::fromStdString(nodeName)));
        }
//        ui->projectTreeView->unsetCursor();
//        QApplication::restoreOverrideCursor();

        ++i;
    }
#ifdef VAMP_DEBUG
  cerr << "doPreProcess: " << (vcPreProcData.doPreProcess ? "true" : "false") << endl;
  cerr << "useVampPreprocessor: " << (vcPreProcData.useVampPreprocessor ? "true" : "false") << endl;
  cerr << "retainComments: " << (vcPreProcData.retainComments ? "true" : "false") << endl;
  cerr << "showLineMarkers: " << (vcPreProcData.showLineMarkers ? "true" : "false") << endl;
  cerr << "preProcPath: " << (vcPreProcData.preProcPath ? "true" : "false") << endl;
  cerr << "preProcArguments: " << vcPreProcData.preProcArguments << endl;
  cerr << "preProcDirectory: " << vcPreProcData.preProcDirectory << endl;
  cerr << "preProcSuffix: " << vcPreProcData.preProcSuffix << endl;
  cerr << "includePaths:" << endl;
  QVector<QString>::iterator it;
  for (it = vcPreProcData.includePaths.begin(); it != vcPreProcData.includePaths.end(); it++)
    cerr << *it << endl;
  cerr << "definesList:" << endl;
  for (it = vcPreProcData.definesList.begin(); it != vcPreProcData.definesList.end(); it++)
    cerr << *it << endl;
#endif
}

// Split pathName into dirName (folder) and fileName (name of file)
void MainWindow::getDirFileNames(QString pathName, QString &dirName, QString &fileName)
{
    //cout << "getDirPath =" << pathName.toStdString() << endl;
    int index = pathName.lastIndexOf('/') + 1;
    if (index == 0)
        dirName = "./";
    else
        dirName = pathName.left(index);

    // Strip off the directory
    fileName = pathName.mid(index);
}

// Load the .vrpt file coverage info for the given source file
void MainWindow::loadFileInfo(QString fileName, fileInstInfo &fileInfo)
{
#ifdef OLD_WAY
    QString vrptName;
    QString preProcDirName;
    QString preProcFileName;

    // Get filename of actual source (in case of preprocessing enabled)
    getPreProcFilename(fileName, preProcDirName, preProcFileName);

    QString vrptDirName;
    QString vrptFileName;

    // Split path and filename
    getDirFileNames(preProcFileName, vrptDirName, vrptFileName);

    // Remove file extension and add new .vrpt extension
    int index = vrptFileName.lastIndexOf(".");
    if (index < 0)
    {
        QMessageBox::critical(0, QString("Error"), QString("Illegal filename %1")
                              .arg(fileName));
        return;
    }

    // Include HTML directory
    addPath(vcData.saveDirectory, vrptDirName);
    addPath(vcReportData.htmlDirectory, vrptDirName);
//    vrptName = vrptDirName + vcReportData.htmlDirectory + "/" + vrptFileName.left(index) + ".vrpt";
    vrptName = vrptDirName + vrptFileName.left(index) + ".vrpt";
#else
    QString htmlDirName;
    QString htmlFileName;

    // Get filename of actual source (in case of preprocessing enabled)
    getHtmlFilename(fileName, htmlDirName, htmlFileName);

    // Remove file extension and add new .vrpt extension
    int index = htmlFileName.lastIndexOf(".");
    if (index < 0)
    {
        QMessageBox::critical(0, QString("Error"), QString("Illegal filename %1")
                              .arg(fileName));
        return;
    }

    QString vrptName = htmlFileName.left(index) + ".vrpt";
#endif

    if (!QFile::exists(vrptName))
    {
        QString vrptDirName;
        QString vrptFileName;
        getDirFileNames(vrptName, vrptDirName, vrptFileName);

        // No history file found
        fileInfo.fileName = vrptFileName;
        fileInfo.instInfo = INST_NO;
        return;
    }

    QFile jsonFile(vrptName);
    if (!jsonFile.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox::critical(0, QString("Error"), QString("Cannot read file %1:\n%2")
                                                   .arg(vrptName)
                                                   .arg(jsonFile.errorString()));
        return;
    }

    QTextStream in(&jsonFile);

    // Read file into a string
    QString jsonSource = in.readAll();

    jsonFile.close();

    // Now parse it
    ostringstream jsonErr;
    Json json(&jsonErr);
    try
    {
      json.ParseJson(jsonSource.toStdString());
    }
    catch(int i)
    {
        QMessageBox::warning(0, QString("Error"), QString("Bad report file: %1\n%2").arg(vrptName).arg(QString::fromStdString(jsonErr.str())));
        return;
    }

    vector<JsonNode>::iterator i = json.jsonNodes.begin();

    // Check to see if .vrpt file is older than source
    // If so, set:
    // fileInfo.instInfo = INST_OLD;

    // Check time of modification
    QFileInfo fileModInfo(fileName);
    QDateTime fileModTime = fileModInfo.lastModified();
    QFileInfo vrptModInfo(vrptName);
    QDateTime vrptModTime = vrptModInfo.lastModified();

    if (fileModTime > vrptModTime)
        fileInfo.instInfo = INST_OLD;
    else
        fileInfo.instInfo = INST_YES;

    if (vcPreProcData.doPreProcess)
    {
        QString preProcDirName;
        QString preProcFileName;

        // Get filename of actual source (in case of preprocessing enabled)
        getPreProcFilename(fileName, preProcDirName, preProcFileName);

        // Also check preprocessed file time
        QFileInfo ppFileModInfo(preProcFileName);
        QDateTime ppFileModTime = ppFileModInfo.lastModified();

        if (ppFileModTime > vrptModTime)
            fileInfo.instInfo = INST_OLD;
    }

    // Parse .vrpt file
    while (i != json.jsonNodes.end())
    {
        // get the node name and value as a string
        QString nodeName = QString::fromStdString(i->name());

        if (nodeName == "file_name")
        {
            fileInfo.fileName = QString::fromStdString(i->as_string());
        }
        else
        if (nodeName == "mcdc_stack_overflow")
        {
            fileInfo.mcdcStackOverflow = i->as_bool();
        }
        else
        if (nodeName == "total_statement_count")
        {
            fileInfo.totalStatementCount = i->as_int();
        }
        else
        if (nodeName == "total_statements_covered")
        {
            fileInfo.totalStatementsCovered = i->as_int();
        }
        else
        if (nodeName == "total_branch_count")
        {
            fileInfo.totalBranchCount = i->as_int();
        }
        else
        if (nodeName == "total_branches_covered")
        {
            fileInfo.totalBranchesCovered = i->as_int();
        }
        else
        if (nodeName == "total_mcdc_count")
        {
            fileInfo.totalMcdcCount = i->as_int();
        }
        else
        if (nodeName == "total_mcdc_covered")
        {
            fileInfo.totalMcdcCovered = i->as_int();
        }
        else
        if (nodeName == "total_condition_count")
        {
            fileInfo.totalConditionCount = i->as_int();
        }
        else
        if (nodeName == "total_conditions_covered")
        {
            fileInfo.totalConditionsCovered = i->as_int();
        }
        else
        if (nodeName == "function_info")
        {
            // Get list of functions and their instrumentation info
            vector<JsonNode> funcInfo;
            json.ParseArray(i->as_string(), funcInfo);
            vector<JsonNode>::iterator j = funcInfo.begin();

            while (j != funcInfo.end())
            {
                FUNCTION_INFO functionInfo;
                functionInfo.functionName = QString::fromStdString(j->as_string());
                ++j;
                functionInfo.statementCount = j->as_int();
                ++j;
                functionInfo.statementsCovered = j->as_int();
                ++j;
                functionInfo.branchCount = j->as_int();
                ++j;
                functionInfo.branchesCovered = j->as_int();
                ++j;
                functionInfo.decisionCount = j->as_int();
                ++j;
                functionInfo.decisionsCovered = j->as_int();
                ++j;
                fileInfo.functionInfo.push_back(functionInfo);
            }
        }
        else
        {
            QMessageBox::warning(0, QString("Warning"), QString("Unknown .vrpt node: %1").arg(nodeName));
        }

        ++i;
    }
}

// Create string list of file stats for each column
// First column is:
// "YES" - if instrumentation data collected and current
// "OLD" - if instrumentation data collected but source has been modified since
// "NO" - if no instrumentation data collected
void MainWindow::getFileStats(fileInstInfo &fileInfo, QStringList &stats)
{
    if (fileInfo.instInfo == INST_YES)
        stats.push_back("YES");
    else
    if (fileInfo.instInfo == INST_NO)
        stats.push_back("NO");
    else
    if (fileInfo.instInfo == INST_OLD)
        stats.push_back("OLD");
    else
        stats.push_back("UNK");

    // If statement coverage enabled, next column contains:
    // "N/I" - no statement coverage enabled for this file
    // "" - no instrumentation data collected for this file
    // "xxx%" - percentage of statement coverage for entire file
    if (!vcData.doStmtSingle && !vcData.doStmtCount)
        stats.push_back("N/I");
    else
    if (fileInfo.instInfo == INST_NO)
    {
        stats.push_back("");
    }
    else
    if (fileInfo.totalStatementCount == 0)
        stats.push_back("N/A");
    else
    {
        int coveragePercent = (fileInfo.totalStatementsCovered * 1000 /
                               fileInfo.totalStatementCount + 5) / 10;
        stats.push_back(QString::number(coveragePercent) + "%");
    }

    // If branch coverage enabled, next column contains:
    // "N/I" - no branch coverage enabled for this file
    // "" - no instrumentation data collected for this file
    // "xxx%" - percentage of branch coverage for entire file
    if (!vcData.doBranch)
        stats.push_back("N/I");
    else
    if (fileInfo.instInfo == INST_NO)
    {
        stats.push_back("");
    }
    else
    if (fileInfo.totalBranchCount == 0)
        stats.push_back("N/A");
    else
    {
        int coveragePercent = (fileInfo.totalBranchesCovered * 1000 /
                               fileInfo.totalBranchCount + 5) / 10;
        stats.push_back(QString::number(coveragePercent) + "%");
    }

    // If condition coverage enabled, next column contains:
    // "N/I" - no condition coverage enabled for this file
    // "" - no instrumentation data collected for this file
    // "xxx%" - percentage of condition coverage for entire file
    if (!vcData.doMCDC && !vcData.doCC)
        stats.push_back("N/I");
    else
    if (fileInfo.instInfo == INST_NO)
    {
        stats.push_back("");
    }
    else
    if (vcData.doMCDC)
    {
        if (fileInfo.totalMcdcCount == 0)
            stats.push_back("N/A");
        else
        {
            int coveragePercent = (fileInfo.totalMcdcCovered * 1000 /
                                   fileInfo.totalMcdcCount + 5) / 10;
            stats.push_back(QString::number(coveragePercent) + "%");
        }
    }
    else
    {
        if (fileInfo.totalConditionCount == 0)
            stats.push_back("N/A");
        else
        {
            int coveragePercent = (fileInfo.totalConditionsCovered * 1000 /
                                   fileInfo.totalConditionCount + 5) / 10;
            stats.push_back(QString::number(coveragePercent) + "%");
        }
    }
}

#ifdef IS_USED
// Insert new file info into tree
// Data to be inserted is in list; each element is a column
void MainWindow::insertRow(QStringList &list)
{
    QModelIndex index = ui->projectTreeView->selectionModel()->currentIndex();
    QAbstractItemModel *model = ui->projectTreeView->model();

    if (!model->insertRow(index.row() + 1, index.parent()))
        return;

    for (int column = 0; column < model->columnCount(index.parent()); ++column)
    {
        QModelIndex child = model->index(index.row() + 1, column, index.parent());
        if (column < list.size())
            model->setData(child, QVariant(list[column]), Qt::EditRole);
        else
            model->setData(child, QVariant(""), Qt::EditRole);
    }
}
#endif

// Insert new file into tree at the current index
// nameList contains the path to the file
void MainWindow::insertChild(QStringList nameList)
{
    QModelIndex index = ui->projectTreeView->selectionModel()->currentIndex();
    QAbstractItemModel *model = ui->projectTreeView->model();

    if (model->columnCount(index) == 0)
    {
        if (!model->insertColumn(0, index))
            return;
    }

    if (!model->insertRow(0, index))
        return;

    for (int column = 0; column < model->columnCount(index); ++column)
    {
        QModelIndex child = model->index(0, column, index);
        model->setData(child, QVariant((column < nameList.size()) ? nameList[column] : ""), Qt::EditRole);
        if (!model->headerData(column, Qt::Horizontal).isValid())
            model->setHeaderData(column, Qt::Horizontal, QVariant("[No header]"), Qt::EditRole);
    }

    ui->projectTreeView->selectionModel()->setCurrentIndex(model->index(0, 0, index),
                                            QItemSelectionModel::ClearAndSelect);
}

// Takes a path name and a list of instrumentation attributes
// and adds path to project along with specified attributes as columns
void MainWindow::insertPath(QString pathName, QStringList fileInfo)
{
    QAbstractItemModel *model = ui->projectTreeView->model();
    QModelIndex parent = QModelIndex();
    QModelIndex index;
    int row = 0;
    bool done = false;

    // Split pathName into parts
    QStringList pathNameList = pathName.split('/');

    // Skip any folder that already exists
    for (int i = 0; !done && !pathNameList.isEmpty(); ++i)
    {
        bool found = false;
        for (int j = 0; !found && (j < model->rowCount(parent)); ++j)
        {
            QVariant a = model->index(j, 0, parent).data();
            if (a.toString() == pathNameList.front())
            {
                // Found a match - save current, set parent to match and remove matched folder
                found = true;
                index = parent;
                row = j;
                parent = model->index(j, 0, parent);
                pathNameList.removeFirst();
            }
        }

        if (!found)
            done = true;
    }

    // Set index to folder that didn't match
    ui->projectTreeView->selectionModel()->setCurrentIndex(parent,
                                            QItemSelectionModel::ClearAndSelect);

    if (pathNameList.isEmpty())
    {
        // File exists - update info
        for (int column = 1; column < model->columnCount(index); ++column)
        {
            QModelIndex child = model->index(row, column, index);
            model->setData(child, QVariant(((column - 1) < fileInfo.size()) ? fileInfo[column - 1] : ""), Qt::EditRole);
        }
    }
    else
    // Create any remaining folders
    for (int j = 0; !pathNameList.isEmpty(); ++j)
    {
        QStringList child;
        child.push_back(pathNameList.front());
        if (pathNameList.size() == 1)
            child.append(fileInfo);
        insertChild(child);
        pathNameList.removeFirst();
    }


    // Not sure why this sorting doesn't work...
    /*
    ui->projectTreeView->setSortingEnabled(true);
    ui->projectTreeView->header()->setSortIndicator(0, Qt::AscendingOrder);
    ui->projectTreeView->sortByColumn(0);
    ui->projectTreeView->setSortingEnabled(false);
    // Or:
    ui->projectTreeView->sortByColumn(0, Qt::AscendingOrder);
    ui->projectTreeView->setSortingEnabled(true);
    */
}

// Find base path of tree - follow tree until it forks (has more than 1 child) and return path
void MainWindow::getCommonPath(QString curPath, QModelIndex parent, QString &path)
{
    QAbstractItemModel *model = ui->projectTreeView->model();

    int cnt = model->rowCount(parent);
    // There's a single file inside this one
    if (cnt == 1)
    {
        cnt = model->rowCount(model->index(0, 0, parent));
        // Make sure it's a directory before adding it to the path
        if (cnt != 0)
        {
            if (curPath != "")
                curPath += "/";

            QString a = model->index(0, 0, parent).data().toString();
//            cout << curPath.toStdString() << a.toStdString() << endl << flush;
            getCommonPath(curPath + a, model->index(0, 0, parent), path);
        }
        else
            path = curPath;
    }
    else
    {
        path = curPath;
    }
}

// If projectDirName is a subset of common path, set baseDir to projectDirName.
// Otherwise strip project directory from common path and set baseDir to common path.
// The latter occurs if the project forks prior to the project directory.
void MainWindow::getBasePath()
{
    QString minPath;

    getCommonPath("", QModelIndex(), minPath);
    //cout << "Common path is " << minPath.toStdString() << endl << flush;
    if (path.comparePath(projectDirName, minPath.left(projectDirName.length())))
        baseDir = projectDirName;
    else
        baseDir = minPath;

    projectBaseDir = baseDir;
}

// Recursively walk the project tree and return a list of filenames found there
// curPath is the path we've traversed so far in the tree.
// parent is the node that we are visiting.
// results is a list of path and filenames we have found.
// Invoke with:
//    QStringList fileList;
//    genFileList("", QModelIndex(), fileList);
void MainWindow::genFileList(QString curPath, QModelIndex parent, QStringList &results)
{
    QAbstractItemModel *model = ui->projectTreeView->model();

    int cnt = model->rowCount(parent);
    //cout << "genFileList cnt=" << cnt << "; curPath =" << curPath.toStdString() << "." << endl;
    if (cnt == 0)
    {
        if (curPath != "")
            results.push_back(curPath);
        return;
    }
    else
    {
        if (curPath != "")
            curPath += "/";

        for (int i = 0; i < cnt; ++i)
        {
            QVariant a = model->index(i, 0, parent).data();
            //cout << "GenFileList " << i << " =" << a.toString().toStdString() << ";" << (a.toString() != "") << endl;
            genFileList(curPath + a.toString(), model->index(i, 0, parent), results);
        }
    }
}

void MainWindow::genSummary(QString &outStr, int level, QModelIndex parent)
{
    QAbstractItemModel *model = ui->projectTreeView->model();

    if (level == 0)
    {
        // Add initial HTML header
        outStr += "<!DOCTYPE html>\n";
        outStr += "<html lang=\"en\">\n";
        outStr += "  <head>\n";
        outStr += "  <meta charset=\"utf-8\"/>\n";
        outStr += "    <title>" + projectName + " summary (Instrumented by VAMP)</title>\n";
        outStr += "  </head>\n";
        outStr += "  <body>\n";
        outStr += "  <h1 align=\"center\">" + projectName + " Project Summary (Instrumented by VAMP)</h1>\n";
        outStr += "<table border frame=box align=center>\n";
/*        outStr += "  <col align=\"left\">\n";
        for (int i = 1; i < model->columnCount(model->index(0, 0, parent)); ++i)
            outStr += "  <col align=\"center\">\n";*/
        outStr += "<tr>\n";
        outStr += "<th> " + projectName + " </th>\n";
        outStr += "<th> Inst </th>\n";
        if (vcData.doStmtSingle || vcData.doStmtCount)
            outStr += "<th> Stmt </th>\n";
        if (vcData.doBranch)
            outStr += "<th> Brch </th>\n";
        if (vcData.doCC)
            outStr += "<th> Cond </th>\n";
        if (vcData.doMCDC)
            outStr += "<th> MCDC </th>\n";
        outStr += "</tr>\n";
    }

    int cnt = model->rowCount(parent);
    if (cnt != 0)
    {
        for (int i = 0; i < cnt; ++i)
        {
            // Check second column to see if it's blank, in which case
            // this is a folder name
            QVariant a = model->index(i, 1, parent).data();
            // Get # columns
            int cols = model->columnCount(model->index(i, 0, parent));

            if (a.toString() == "")
            {
                // This is a folder name
                outStr += "<tr>\n";
                if (level)
                    outStr += "<td style=\"padding-left:" + QString::number(level * 10) + "px;\"";
                else
                    outStr += "<td";
                outStr += " colspan=" + QString::number(cols) + ">\n";
                outStr += "<img src=\"data:image/bmp,BMv%04%00%00%00%00%00%006%00%00";
                outStr += "%00%28%00%00%00%16%00%00%00%10%00%00%00%01%00%18%00%00%00";
                outStr += "%00%00%40%04%00%00%C2%1E%00%00%C2%1E%00%00%00%00%00%00%00";
                outStr += "%00%00%00%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%ED%F8%FA%BA%E3";
                outStr += "%EC~%C8%DAL%B2%CCd%B0%C4%AE%D1%DC%E8%F4%F8%FF%FF%FF%FF%FF";
                outStr += "%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%00%00%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%FF%8E%D7%E7_%C3%D9%5D%C0%D6%5E%BF%D6%60%BF%D6D%AF%CAK%A6";
                outStr += "%C1V%B8%D4%5E%C0%D7%5D%BF%D7%5C%BE%D6r%C7%DB%B5%E1%EC%00";
                outStr += "%00%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%FF%FF%FF%FF%FF%FF%FF%FF%FFl%CE%E1%80%D4%E4w%CE%E0o%C9%DDg";
                outStr += "%C4%DAD%AF%CAC%A6%C3N%B8%D7N%C1%DEN%C1%DEN%C1%DEP%C1%DDs%C7";
                outStr += "%DB%00%00%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FFp%D1%E4%89%DB%E8%80%D4%E4w";
                outStr += "%CE%E0o%C9%DDD%AF%CAE%A8%C4N%B8%D7N%C1%DEN%C1%DEN%C1%DE%D3";
                outStr += "%A5I%5D%BF%D7%00%00%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FFu%D4%E7%92%E1%EC";
                outStr += "%89%DB%E8%80%D4%E4w%CE%E0E%B0%CAI%A9%C4R%BA%D8R%C3%DFN%C1";
                outStr += "%DEN%C1%DE%DD%B9%5D%5E%C0%D8%00%00%88%88%88%26%26%26%26%26";
                outStr += "%26%26%26%26%26%26%26%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FFy";
                outStr += "%D8%E9%9B%E7%F0%92%E1%EC%89%DB%E8%80%D4%E4G%B2%CCN%AC%C6V%BC";
                outStr += "%D9%5B%C7%E1V%C5%E0R%C3%DF%EC%EC%EC%60%C1%D9%00%00%BD%BD%BD";
                outStr += "%2B%2B%2BXXXYYYYYY%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%7D%DB";
                outStr += "%EC%A3%EC%F4%9B%E7%F0%92%E1%EC%89%DB%E8J%B4%CET%AF%C8%5C%BF";
                outStr += "%DAf%CD%E4%60%CA%E3%5B%C7%E1%EC%EC%ECb%C2%DA%00%00%FF%FF%FF";
                outStr += "%BD%BD%BD%2B%2B%2BXXXYYY%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%81%DE%EE%AA%F1%F7%A3%EC%F4%9B%E7%F0%92%E1%ECN%B7%D0%5B%B2%C9c";
                outStr += "%C2%DCs%D3%E8m%D0%E6f%CD%E4%EC%EC%ECc%C3%DB%00%00%FF%FF%FF%FF";
                outStr += "%FF%FF%BD%BD%BD%2B%2B%2BXXX%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%FF%84%E0%F0%B0%F5%F9%AA%F1%F7%A3%EC%F4%C6%F1%F6R%BA%D2a%B6%CBi";
                outStr += "%C6%DE%82%DA%ECz%D7%EAs%D3%E8m%D0%E6e%C5%DC%00%00%FF%FF%FF%FF";
                outStr += "%FF%FF%FF%FF%FF%BD%BD%BD%2B%2B%2B%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%FF%FF%88%E3%F2%B4%F7%FB%B0%F5%F9%AA%F1%F7%E8%FA%FCV%BD%D4h%B9";
                outStr += "%CDp%C9%E0%90%E2%F0%89%DE%EE%82%DA%ECo%CC%E2%A6%DD%EB%00%00%FF";
                outStr += "%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%BF%BF%BF%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%FF%FF%FF%FF%FF%8A%E5%F3%B4%F7%FB%B4%F7%FB%B0%F5%F9%F4%FD%FEZ%C0";
                outStr += "%D7n%BC%CEv%CC%E1%9E%E9%F3%97%E5%F2%90%E2%F0k%C9%DF%FF%FF%FF%00";
                outStr += "%00%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%FF%FF%FF%FF%FF%FF%FF%8D%E6%F5%B4%F7%FB%B4%F7%FB%B4%F7%FB%FC%FF";
                outStr += "%FF_%C4%DAt%BE%D0%7D%CF%E3%AC%EF%F7%A5%EC%F5%9E%E9%F3n%CA%E1%FF";
                outStr += "%FF%FF%00%00%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%8D%E7%F5%B4%F7%FB%B4%F7%FB%B4%F7%";
                outStr += "FB%FF%FF%FFd%C8%DDx%C0%D1%81%D2%E5%B7%F4%FA%B1%F2%F9%AC%EF%F7p%CC";
                outStr += "%E2%FF%FF%FF%00%00%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%8D%E7%F5%B4%F7%FB%B4%F7%FB";
                outStr += "%FC%FF%FF%FC%FF%FFi%CB%E0%7F%C6%D5%AE%EE%F6%BF%F9%FC%BB%F7%FB%B7";
                outStr += "%F4%FAr%CD%E4%FF%FF%FF%00%00%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FD%FE%FE%8D%E7%F5%B1%F6%FB";
                outStr += "%E9%FB%FD%99%EA%F4%87%DF%EDy%CD%DC%BC%F5%FA%C2%FA%FD%C2%FA%FD%C2";
                outStr += "%FA%FD%B6%F4%F9%86%D5%E8%FF%FF%FF%00%00%FF%FF%FF%FF%FF%FF%FF%FF";
                outStr += "%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%FF%AF%ED";
                outStr += "%F8%8E%E7%F5%8D%E4%F4%89%E0%F2%88%DE%F1%89%DD%F1%86%DB%EF%83%D9";
                outStr += "%EE%80%D7%EC%7D%D5%EA%8B%D8%EA%C0%E9%F3%FF%FF%FF%00%00\"\n";
                outStr += "width=\"16\" height=\"14\" alt=\"embedded folder icon\">\n";
                outStr += model->index(i, 0, parent).data().toString() + "\n";
                outStr += "</td>\n";
                outStr += "</tr>\n";
            }
            else
            {
                outStr += "<tr>\n";
//                outStr += a.toString()+ " ; cols = " + QString::number(cols) + ", level = " + QString::number(level) + "\n";
                for (int j = 0; j < cols; ++j)
                {
                    QVariant b = model->index(i, j, parent).data();
                    bool gotStyle = false;
                    if (level && (j == 0))
                    {
                        outStr += "<td style=\"padding-left:" + QString::number(level * 10) + "px;";
                        gotStyle = true;
                    }
                    else
                    {
                        outStr += "<td";
                    }
                    QVariant c = model->data(model->index(i, j, parent), Qt::BackgroundRole);
                    QColor color = c.value<QColor>();
                    if (color != Qt::white)
                    {
                        if (!gotStyle)
                        {
                            outStr += " style=\"";
                            gotStyle = true;
                        }
                        else
                            outStr += " ";
                        outStr += "background:" + color.name();
                    }
                    if (gotStyle)
                        outStr += "\"";
                    outStr += "> ";
                    if (j > 0)
                        outStr += "<center> " + b.toString() + " </center> ";
                    else
                        outStr += b.toString();
                    outStr += " </td>\n";
                }
                outStr += "</tr>\n";
            }
            QModelIndex par = model->index(i, 0, parent);

            genSummary(outStr, level + 1, par);
        }
        if (level == 0)
        {
            outStr += "  </body>\n";
            outStr += "</table>\n";
        }
    }
}

// Edit configuration info - use config dialog
void MainWindow::editConfig(void)
{
    vampConfigDialog vc;
    vc.setProjectDirectory(projectDirName);
    vc.setVcData(vcData);
    vc.setVcReportData(vcReportData);
    vc.setVcPreProcData(vcPreProcData);
    if (vc.exec())
    {
        bool vcDataChg = false;
        bool vcReportDataChg = false;
        bool vcPreProcDataChg = false;
        vc.getVcData(vcData, vcDataChg);
        if (vcDataChg)
            vcDataChanged = true;
        vc.getVcReportData(vcReportData, vcReportDataChg);
        if (vcReportDataChg)
            vcReportDataChanged = true;
        vc.getVcPreProcData(vcPreProcData, vcPreProcDataChg);
        if (vcPreProcDataChg)
            vcPreProcDataChanged = true;
    }
}

// Loading or creating a project
// fileName is path to .vproj project
// isNew is true if this is a new project
void MainWindow::checkProject(QString fileName, bool isNew)
{
    projectChanged = false;
    vcDataChanged = false;
    vcReportDataChanged = false;
    vcPreProcDataChanged = false;

    // Enable Project menu option "Add"
    ui->action_Add->setEnabled(true);
    ui->actionEdit_Configuration->setEnabled(true);

    // Enable Project menu option "Save Project"
    ui->actionSave_Project->setEnabled(true);

    // Enable Project menu option "Close Project"
    ui->action_Close_Project->setEnabled(true);

    // Enable Project menu option "Generate Summary"
    ui->action_Generate_Summary->setEnabled(true);

    // Enable Project menu option "Generate Summary"
    ui->action_Print_Summary->setEnabled(true);

    // Enable Project menu option "Generate vamp_output.c"
    ui->action_Generate_vamp_output_c->setEnabled(true);

    fileName = fixPath(fileName, false);
    int index = fileName.lastIndexOf('/') + 1;
    if (index == 0)
        projectDirName = "./";
    else
        projectDirName = fileName.left(index);

    projectPath = fileName;

    // Strip off the directory and file extension (.vproj)
    projectName = projectPath.mid(index, projectPath.size() - index - 6);

    // Create paths to vamp.cfg and vamp_project.cfg
    QString vampCfgName(projectDirName + "vamp.cfg");
    QString vampProcessCfgName(projectDirName + "vamp_process.cfg");

    bool missingCfg = false;
    bool missingReportCfg = false;

    ostringstream cfgErr;
    ConfigFile vampConfig(&cfgErr);

    if (QFile::exists(vampCfgName))
    {
        // Load vamp.cfg
        if (!vampConfig.ParseConfigFile(vampCfgName.toStdString(), vcData))
        {
            QMessageBox::warning(0, QString("Warning"), QString("Bad vamp.cfg - %1").arg(cfgErr.str().data()));
        }
        vcDataChanged = false;
    }
    else
    {
        // Not found - Set default vamp.cfg data
        setDefaultVcData(vcData);
        vcDataChanged = true;
        missingCfg = true;
    }

    if (QFile::exists(vampProcessCfgName))
    {
        // Load and parse vamp_process.cfg
        vampConfig.ParseReportConfigFile(vampProcessCfgName.toStdString(), vcReportData);
        vcReportDataChanged = false;
    }
    else
    {
        // Not found - Set default vamp_process.cfg data
        setDefaultVcReportData(vcReportData);
        vcReportDataChanged = true;
        missingReportCfg = true;
    }

    // Set default preprocessor info
    setDefaultVcPreProcData(vcPreProcData);

    if (!isNew && (missingCfg || missingReportCfg))
    {
        if (missingCfg)
            QMessageBox::warning(0, QString("Warning"), QString("Missing vamp.cfg"));

        if (missingReportCfg)
            QMessageBox::warning(0, QString("Warning"), QString("Missing vamp_process.cfg"));
    }

    if (isNew || missingCfg || missingReportCfg)
    {
        // New project or one of the .cfg files was missing - show config dialog
        editConfig();
    }

    // Enable tree view for project
    ui->projectGroupBox->show();
    ui->projectTreeView->show();
    ui->projectTreeView->setIndentation(10);
//    ui->projectTreeView->setSortingEnabled(true);
//    ui->projectTreeView->setExpanded();

    // Enable output/errors view for project
    ui->tabWidget->show();

    // Display project name
    QPlainTextEditAppendText(ui->outputTextEdit, "Loading project ");
    QPlainTextEditAppendText(ui->outputTextEdit, projectPath + " ...\n", Qt::blue);

    // Create project labels based on types of instrumentation selected in vamp.cfg
    QStringList headers;
    headers << projectName << tr("Inst");
    if (vcData.doStmtCount || vcData.doStmtSingle)
        headers << tr("Stmt");
    if (vcData.doBranch)
        headers << tr("Brch");
    if (vcData.doCC)
        headers << tr("Cond");
    if (vcData.doMCDC)
        headers << tr("MCDC");
    // Throw in a blank at the end to set right-hand margin for
    // last column
//    headers << "";

    // Create model for project tree
    projectModel = new ProjectModel(headers);

    ui->projectTreeView->setModel(projectModel);
    // Enable context menu for each file in tree
    ui->projectTreeView->setContextMenuPolicy(Qt::CustomContextMenu);

    ui->projectTreeView->header()->setStretchLastSection(false);
    // Make first column stretchable
    ui->projectTreeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    // Set width for each remaining column
    for (int column = 1; column < projectModel->columnCount(); ++column)
    {
        // Set width based on width of header
        ui->projectTreeView->resizeColumnToContents(column);
    }

    if (isNew || vcDataChanged || vcReportDataChanged || vcPreProcDataChanged)
        projectChanged = true;

    QPlainTextEditAppendText(ui->outputTextEdit, "Loaded.\n");
}

// Save current project
// If showDialog is true, first show dialog asking to save
// Returns false if Cancel pressed, else returns true
bool MainWindow::saveProject(bool showDialog)
{
    if (vcDataChanged)
    {
        if (showDialog)
        {
            int ret = QMessageBox::warning(this, projectName,
                                           tr("vamp.cfg has been modified.\n"
                                              "Do you want to save your changes?"),
                                           QMessageBox::Save | QMessageBox::Discard
                                           | QMessageBox::Cancel,
                                           QMessageBox::Save);
            if (ret == QMessageBox::Save)
                saveVcData();
            else
            if (ret == QMessageBox::Cancel)
                return false;
        }
        else
            saveVcData();
    }

    if (vcReportDataChanged)
    {
        if (showDialog)
        {
            int ret = QMessageBox::warning(this, projectName,
                                           tr("vamp_process.cfg has been modified.\n"
                                              "Do you want to save your changes?"),
                                           QMessageBox::Save | QMessageBox::Discard
                                           | QMessageBox::Cancel,
                                           QMessageBox::Save);
            if (ret == QMessageBox::Save)
                saveVcReportData();
            else
            if (ret == QMessageBox::Cancel)
                return false;
        }
        else
            saveVcReportData();
    }

    if (projectChanged || (baseDir != projectBaseDir) || vcPreProcDataChanged)
    {
        if (showDialog)
        {
            int ret = QMessageBox::warning(this, projectName,
                                           tr("Project has been modified.\n"
                                              "Do you want to save your changes?"),
                                           QMessageBox::Save | QMessageBox::Discard
                                           | QMessageBox::Cancel,
                                           QMessageBox::Save);
            if (ret == QMessageBox::Save)
                saveProjectData();
            else
            if (ret == QMessageBox::Cancel)
                return false;
        }
        else
            saveProjectData();
    }

    return true;
}

// Init context menu actions
void MainWindow::createContextActions()
{
    removeFileAct = new QAction(tr("&Remove file from project"), this);
    removeFileAct->setStatusTip(tr("Remove file from project"));
    connect(removeFileAct, SIGNAL(triggered()), this, SLOT(removeItem()));
    removeFolderAct = new QAction(tr("&Remove folder"), this);
    removeFolderAct->setStatusTip(tr("Remove folder and contents from project"));
    connect(removeFolderAct, SIGNAL(triggered()), this, SLOT(removeItem()));
    preprocessFileAct = new QAction(tr("&Preprocess file"), this);
    preprocessFileAct->setStatusTip(tr("Preprocess file"));
    connect(preprocessFileAct, SIGNAL(triggered()), this, SLOT(preprocessItem()));
    preprocessFolderAct = new QAction(tr("&Preprocess folder"), this);
    preprocessFolderAct->setStatusTip(tr("Preprocess files in folder"));
    connect(preprocessFolderAct, SIGNAL(triggered()), this, SLOT(preprocessItem()));
    instrumentFileAct = new QAction(tr("&Instrument file"), this);
    instrumentFileAct->setStatusTip(tr("Instrument file"));
    connect(instrumentFileAct, SIGNAL(triggered()), this, SLOT(instrumentItem()));
    instrumentFolderAct = new QAction(tr("&Instrument folder"), this);
    instrumentFolderAct->setStatusTip(tr("Instrument files in folder"));
    connect(instrumentFolderAct, SIGNAL(triggered()), this, SLOT(instrumentItem()));
    processResultsAct = new QAction(tr("&Process results"), this);
    processResultsAct->setStatusTip(tr("Process results"));
    connect(processResultsAct, SIGNAL(triggered()), this, SLOT(processItem()));
    showResultsAct = new QAction(tr("&Show results"), this);
    showResultsAct->setStatusTip(tr("Show results"));
    connect(showResultsAct, SIGNAL(triggered()), this, SLOT(showResults()));
    processFolderAct = new QAction(tr("&Process folder"), this);
    processFolderAct->setStatusTip(tr("Process files in folder"));
    connect(processFolderAct, SIGNAL(triggered()), this, SLOT(processItem()));
    addFilesAct = new QAction(tr("&Add files"), this);
    addFilesAct->setStatusTip(tr("Add files to project"));
    connect(addFilesAct, SIGNAL(triggered()), this, SLOT(addFilesToFolder()));
    showFileAct = new QAction(tr("&Show source file"), this);
    showFileAct->setStatusTip(tr("Display source file"));
    connect(showFileAct, SIGNAL(triggered()), this, SLOT(showItem()));
    showPreProcFileAct = new QAction(tr("&Show preprocessed source file"), this);
    showPreProcFileAct->setStatusTip(tr("Display preprocessed source file"));
    connect(showPreProcFileAct, SIGNAL(triggered()), this, SLOT(showPreProcItem()));
    showInstrFileAct = new QAction(tr("&Show instrumented source file"), this);
    showInstrFileAct->setStatusTip(tr("Display instrumented source file"));
    connect(showInstrFileAct, SIGNAL(triggered()), this, SLOT(showInstrItem()));

    // Add Clear Output Contents to Output TextEdit
    ui->outputTextEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    clearOutputAct = new QAction(tr("&Erase"), this);
    clearOutputAct->setStatusTip(tr("Erase Output Contents"));
    connect(clearOutputAct, SIGNAL(triggered()), this, SLOT(clearOutput()));
//    connect(ui->outputTextEdit, SIGNAL(customContextMenuRequested(const QPoint&)),
//            this, SLOT(showOutputContextMenu(const QPoint &)));

    // Add Save Output As Contents to Output TextEdit
//    ui->outputTextEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    saveOutputAsAct = new QAction(tr("&Save As"), this);
    saveOutputAsAct->setStatusTip(tr("Save Output As"));
    connect(saveOutputAsAct, SIGNAL(triggered()), this, SLOT(saveOutputAs()));
    connect(ui->outputTextEdit, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(showOutputContextMenu(const QPoint &)));

    // Add Clear Error Contents to Error TextEdit
    ui->errorTextEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    clearErrorAct = new QAction(tr("&Erase"), this);
    clearErrorAct->setStatusTip(tr("Erase Errors Content"));
    connect(clearErrorAct, SIGNAL(triggered()), this, SLOT(clearError()));
    //connect(ui->errorTextEdit, SIGNAL(customContextMenuRequested(const QPoint&)),
    //        this, SLOT(showErrorContextMenu(const QPoint &)));

    // Add Save Error As Contents to Error TextEdit
    //ui->errorTextEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    saveErrorAsAct = new QAction(tr("&Save As"), this);
    saveErrorAsAct->setStatusTip(tr("Save Errors As"));
    connect(saveErrorAsAct, SIGNAL(triggered()), this, SLOT(saveErrorAs()));
    connect(ui->errorTextEdit, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(showErrorContextMenu(const QPoint &)));
}

// Add Erase option to context menu for Output TextEdit
void MainWindow::showOutputContextMenu(const QPoint &pt)
{
    QMenu *menu = ui->outputTextEdit->createStandardContextMenu();
    menu->addAction(clearOutputAct);
    menu->addAction(saveOutputAsAct);
    menu->exec(ui->outputTextEdit->mapToGlobal(pt));
    delete menu;
}

// Erase text from Output window
void MainWindow::clearOutput()
{
    ui->outputTextEdit->clear();
}

// Save text from Output window
void MainWindow::saveLogAs(QPlainTextEdit *edit)
{
    edit->selectAll();
    edit->copy();

    const QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();

    QString filter, sfilter;
#ifdef HANDLE_TXT
    if (mimeData->hasHtml())
    {
        filter = "HTML Files (*.html);;Text files (*.txt)";
    }
    else
    if (mimeData->hasText())
    {
        filter = "Text files (*.txt)";
    }
#else
    filter = "HTML Files (*.html)";
#endif

    // Deselect text
    QTextCursor cur = edit->textCursor();
    cur.clearSelection();
    edit->setTextCursor(cur);

    QString fileName = QFileDialog::getSaveFileName(this, "Save As", projectDirName,
                                                          filter, &sfilter);
    if (!fileName.isEmpty())
    {
        QFile outFile(fileName);
        if (!outFile.open(QFile::WriteOnly | QFile::Truncate))
        {
            QMessageBox::critical(this, tr("Error"), tr("Cannot create output file %1:\n%2")
                                 .arg(fileName)
                                 .arg(outFile.errorString()));
            return;
        }

        QTextStream out(&outFile);
#ifdef HANDLE_TXT
        if (sfilter.left(1) == "H")
            out << mimeData->html();
        else
            out << edit->toPlainText(); //mimeData->text();
#else
        out << mimeData->html();
#endif
        outFile.close();
    }
}

// Save text from Output window
void MainWindow::saveOutputAs()
{
    saveLogAs(ui->outputTextEdit);
}

// Add Erase option to context menu for Errors TextEdit
void MainWindow::showErrorContextMenu(const QPoint &pt)
{
    QMenu *menu = ui->errorTextEdit->createStandardContextMenu();
    menu->addAction(clearErrorAct);
    menu->addAction(saveErrorAsAct);
    menu->exec(ui->errorTextEdit->mapToGlobal(pt));
    delete menu;
}

// Erase text from Errors window
void MainWindow::clearError()
{
    ui->errorTextEdit->clear();
}

// Save text from Errors window
void MainWindow::saveErrorAs()
{
    saveLogAs(ui->errorTextEdit);
}

// Delete file or folder from tree
void MainWindow::removeItem()
{
    QAbstractItemModel *model = ui->projectTreeView->model();
    QModelIndex index = ui->projectTreeView->currentIndex();
    bool killParent;
    do {
        // See if parent folder has only one child
        // If so, delete parent
        killParent = false;
        QModelIndex parent = index.parent();
        if (parent.isValid())
        {
            killParent = (model->rowCount(parent) == 1);
        }
        model->removeRow(index.row(), parent);
        index = parent;
    } while (killParent);

    projectChanged = true;
}

// Return index for selected file.
// If user clicked on row r, column c, returns child as row r, column 0
// to point to file name
void MainWindow::getFileIndex(QModelIndex &index, QModelIndex &child)
{
    index = ui->projectTreeView->currentIndex();

    if (index.parent().isValid())
    {
        // Point to file name in case user clicked on other than filename's column
        index = index.parent().child(index.row(), 0);
        child = index.child(0, 0);
    }
    else
    {
        // Using root node
        child = index;
    }
}

// Preprocess file or folder
void MainWindow::preprocessItem()
{
    QModelIndex index;
    QModelIndex child;
    getFileIndex(index, child);

    QStringList fileList;

    // Make sure vamp.cfg (and project) have been saved
    saveProject(false);

    if (child.isValid())
    {
        // Folder selected - get list of files below folder
        genFileList("", index, fileList);
    }
    else
    {
        // File selected - place in list
        QVariant a = index.data();
        QString fileName = a.toString();
        fileList.push_back(fileName);
        index = index.parent();
    }

    // Create path to specified file/folder
    QString pathName;
    while (index.isValid() && (index.data().toString() == ""))
    {
        //cout << "Skipping PP..." << endl;
        index = index.parent();
    }
    while (index.isValid())
    {
        pathName = index.data().toString() + "/" + pathName;
        index = index.parent();
    }
#ifndef _WIN32
    // Insert starting '/' for non-WIN32 pathnames
    if (pathName[0] != '/')
        pathName = "/" + pathName;
#endif
    // Not sure why this doesn't work here:
//    ui->tabWidget->show();
    int cnt = 0;

    QProgressDialog progressDialog(this,
                                   Qt:: Dialog |
                                   //Qt:: FramelessWindowHint |
                                   Qt:: WindowTitleHint |
                                   Qt:: CustomizeWindowHint);

    progressDialog.setCancelButtonText(0);
    progressDialog.setRange(0, fileList.size());
    progressDialog.setWindowTitle(tr("Preprocessing Files"));

    int successCnt = 0;
    int failCnt = 0;

    for (int i = 0; i < fileList.size(); i++)
    {
        QString fileName = pathName + fileList[i];

        progressDialog.setValue(cnt++);
        progressDialog.setLabelText(tr("Preprocessing %1...")
                                    .arg(fileName));
        qApp->processEvents();

        // Now preprocess fileName, and force generation
        // (i.e. ignore if the current preprocessed file is "current"
        if (preprocessFile(fileName, true))
            ++successCnt;
        else
            ++failCnt;
    }

    ostringstream result;
    result << successCnt << " succeeded, " << failCnt << " failed.\n";
    QPlainTextEditAppendText(ui->outputTextEdit, QString::fromStdString(result.str()));

    if (failCnt)
    {
        QPlainTextEditAppendText(ui->outputTextEdit, "Errors occurred!\n", Qt::red);
    }
}

// Instrument file or folder
void MainWindow::instrumentItem()
{
    QModelIndex index;
    QModelIndex child;
    getFileIndex(index, child);

    QStringList fileList;

    // Make sure vamp.cfg (and project) have been saved
    saveProject(false);

    if (child.isValid())
    {
        // Folder selected - get list of files below folder
        genFileList("", index, fileList);
    }
    else
    {
        // File selected - place in list
        QVariant a = index.data();
        QString fileName = a.toString();
        //cout << "InstItem fileName=" << fileName.toStdString() << endl;
        fileList.push_back(fileName);
        index = index.parent();
    }

    // Create path to specified file/folder
    QString pathName;
    while (index.isValid() && (index.data().toString() == ""))
    {
        //cout << "Skipping..." << endl;
        index = index.parent();
    }
    while (index.isValid())
    {
        pathName = index.data().toString() + "/" + pathName;
        //cout << "instItem path=" << pathName.toStdString() << endl;
        index = index.parent();
    }
#ifndef _WIN32
    // Insert starting '/' for non-WIN32 pathnames
    if (pathName[0] != '/')
        pathName = "/" + pathName;
#endif

    // Not sure why this doesn't work here:
//    ui->tabWidget->show();
    int cnt = 0;

    QProgressDialog progressDialog(this,
                                   Qt:: Dialog |
                                   //Qt:: FramelessWindowHint |
                                   Qt:: WindowTitleHint |
                                   Qt:: CustomizeWindowHint);

    progressDialog.setCancelButtonText(0);
    progressDialog.setRange(0, fileList.size());
    progressDialog.setWindowTitle(tr("Instrumenting Files"));

    int successCnt = 0;
    int failCnt = 0;

    for (int i = 0; i < fileList.size(); i++)
    {
        QString fileName = pathName + fileList[i];

        progressDialog.setValue(cnt++);
        progressDialog.setLabelText(tr("Instrumenting %1...")
                                    .arg(fileName));
        qApp->processEvents();

        // Now instrument fileName
        if (instrumentFile(fileName))
            ++successCnt;
        else
            ++failCnt;
    }

    ostringstream result;
    result << successCnt << " succeeded, " << failCnt << " failed.\n";
    QPlainTextEditAppendText(ui->outputTextEdit, QString::fromStdString(result.str()));

    if (failCnt)
    {
        QPlainTextEditAppendText(ui->outputTextEdit, "Errors occurred!\n", Qt::red);
    }
}

// Process file or folder
void MainWindow::processItem()
{
    QModelIndex index;
    QModelIndex child;
    getFileIndex(index, child);

    QStringList fileList;
    if (child.isValid())
    {
        // Folder selected - get list of files below folder
        genFileList("", index, fileList);
    }
    else
    {
        // File selected - place in list
        QVariant a = index.data();
        QString fileName = a.toString();
        fileList.push_back(fileName);
        index = index.parent();
    }

    // Create path to specified file/folder
    QString pathName;
    while (index.isValid())
    {
        pathName = index.data().toString() + "/" + pathName;
        index = index.parent();
    }
#ifndef _WIN32
    // Insert starting '/' for non-WIN32 pathnames
    if (pathName[0] != '/')
        pathName = "/" + pathName;
#endif

    int cnt = 0;

    QProgressDialog progressDialog(this,
                                   Qt:: Dialog |
                                   //Qt:: FramelessWindowHint |
                                   Qt:: WindowTitleHint |
                                   Qt:: CustomizeWindowHint);

    progressDialog.setCancelButtonText(0);
    progressDialog.setRange(0, fileList.size());
    progressDialog.setWindowTitle(tr("Processing Files"));

    int successCnt = 0;
    int failCnt = 0;

    for (int i = 0; i < fileList.size(); i++)
    {
        QString fileName = pathName + fileList[i];

        progressDialog.setValue(cnt++);
        progressDialog.setLabelText(tr("Processing %1...")
                                    .arg(fileName));
        qApp->processEvents();

        // Now process fileName
        if (processFile(fileName))
            ++successCnt;
        else
            ++failCnt;

        // Update info for file
        QStringList fileInfo;
        fileInstInfo instInfo;
        // Load .vrpt for file
        loadFileInfo(fileName, instInfo);
        // Load any instrumentation info from .vrpt file
        getFileStats(instInfo, fileInfo);
        // Insert file and stats into project tree
        insertPath(fileName, fileInfo);
    }

    ostringstream result;
    result << successCnt << " succeeded, " << failCnt << " failed.\n";
    QPlainTextEditAppendText(ui->outputTextEdit, QString::fromStdString(result.str()));

    if (failCnt)
    {
        QPlainTextEditAppendText(ui->outputTextEdit, "Errors occurred!\n", Qt::red);
    }
}

// Add files to project
void MainWindow::addFilesToFolder()
{
    QModelIndex index;
    QModelIndex child;
    getFileIndex(index, child);

    // Create path to specified file/folder
    QString pathName;
    while (index.isValid())
    {
        pathName = index.data().toString() + "/" + pathName;
        index = index.parent();
    }

    if (pathName == "")
        // No path selected - use project directory
        pathName = projectDirName;
    else
    {
#ifndef _WIN32
        // Insert starting '/' for non-WIN32 pathnames
        if (pathName[0] != '/')
            pathName = "/" + pathName;
#endif
        if (!child.isValid())
        {
            // Filename selected instead of folder - remove file to get path
            // Note pathName ends with <filename>/ so we search inside of '/'
            // to get path (i.e. "<path>/file.c/")
            int dirIndex = pathName.lastIndexOf('/', -2);
            if (dirIndex == -1)
                // Can't find directory? Use project directory
                pathName = projectDirName;
            else
                pathName = pathName.left(dirIndex);
        }
    }

    addFiles(pathName);
}

// Show contents of specified source file
void MainWindow::showItem()
{
    QModelIndex index;
    QModelIndex child;
    getFileIndex(index, child);

    QString fileName;
    getFullFileName(index, fileName);
    showHtmlFile(fileName, false);
}

// Show results for specified source file
void MainWindow::showResults()
{
    QModelIndex index;
    QModelIndex child;
    getFileIndex(index, child);

    on_projectTreeView_doubleClicked(index);
}

// Show contents of preprocessed file
void MainWindow::showPreProcItem()
{
    QModelIndex index;
    QModelIndex child;
    getFileIndex(index, child);

    QString fileName;
    getFullFileName(index, fileName);

    QString pathName;
    QString preProcFileName;

    if (getPreProcFilename(fileName, pathName, preProcFileName))
    {
        showHtmlFile(preProcFileName, false);
    }
}

// Show contents of instrumented file
void MainWindow::showInstrItem()
{
    QModelIndex index;
    QModelIndex child;
    getFileIndex(index, child);

    QString instrFileName;
    QString instrFilePath;
    if (getInstrFilenameFromIndex(index, instrFilePath, instrFileName))
    {
        showHtmlFile(instrFileName, false);
    }
}

// Append pathToAdd to path.
// If pathToAdd is null, or "." or "./", ignore it.
// If path does not end with "/", append "/".
void MainWindow::addPath(string pathToAdd, QString &path)
{
    if ((pathToAdd != "") && (pathToAdd != "." && pathToAdd != "./"))
    {
        path += fixPath(pathToAdd, true);
    }
    else
    if (path.right(1) != "/")
        path += "/";
}

// Get full fileName including path from tree of file at specified index
void MainWindow::getFullFileName(QModelIndex index, QString &fileName)
{
    QAbstractItemModel *model = ui->projectTreeView->model();
    fileName = model->data(index).toString();

    while (model->parent(index).isValid())
    {
        index = model->parent(index);
        QVariant a = model->data(index);
        fileName = a.toString() + "/" + fileName;
    }
#ifndef _WIN32
    // Insert starting '/' for non-WIN32 pathnames
    if (fileName[0] != '/')
        fileName = "/" + fileName;
#endif
}

// Generate path and filename of preprocessed file based on original fileName.
// Returns:
// pathName - path taken from fileName.
// preProcFileName - full path to preprocessed file; if preprocessing not enabled,
//                   fileName is returned.
// Returns true if succeeded; false if filename is invalid
bool MainWindow::getPreProcFilename(QString fileName, QString &pathName, QString &preProcFileName)
{
    getDirFileNames(fileName, pathName, preProcFileName);
    //cout << "Dirpath=" << pathName.toStdString() << endl;
    // Determine if preprocessing is requested
    if (vcPreProcData.doPreProcess)
    {
        // Is the specified preprocessor directory absolute?
        if (isAbsolutePath(vcPreProcData.preProcDirectory))
        {
            int len = projectBaseDir.length();
            if (path.comparePath(projectBaseDir, pathName.left(len)))
            {
                // Should always be true
                pathName = pathName.mid(len);
            }
            else
            {
                QMessageBox::warning(0, QString("Warning"), QString("Specified file %1 not in project path %2")
                                      .arg(fileName)
                                      .arg(projectDirName));
                return false;
            }

            // Get absolute path
            QString startDir = QString::fromStdString(vcPreProcData.preProcDirectory);
            pathName = fixPath(startDir + pathName, true);
        }
        else
        {
            addPath(vcPreProcData.preProcDirectory, pathName);
        }
        //cout << "preproc path = " << pathName.toStdString() << endl << flush;

        int index = preProcFileName.lastIndexOf('.');
        if (index == -1)
            return false;

        preProcFileName = pathName + preProcFileName.left(index) + QString::fromStdString(vcPreProcData.preProcSuffix) + preProcFileName.mid(index);
    }
    else
    {
        // No - nothing needs to be done
        preProcFileName = fileName;
    }

    return true;
}

// Get path to where instrumented files go
// Used for generation of vamp_output.c and vamp_output.h
void MainWindow::getInstrPath(QString &instrPath)
{
    // Determine output path
    if (isAbsolutePath(vcData.saveDirectory))
    {
        // Absolute path
        instrPath = fixPath(vcData.saveDirectory, true);
    }
    else
    {
        // Get relative path
        QString preprocPathName;
        QString preProcFileName;
        getPreProcFilename(projectBaseDir, preprocPathName, preProcFileName);
        instrPath = preprocPathName + fixPath(vcData.saveDirectory, true);
    }
}

// Generate path and filename of instrumented file based on original fileName.
// Returns:
// outPath - output file path.
// instrFileName - full path to instrumented file.
// Returns true if succeeded; false if filename is invalid
bool MainWindow::getInstrFilename(QString fileName, QString &outPath, QString &instrFileName)
{
    // Determine path to new file
    QString pathName;
    QString shortFileName;
    getDirFileNames(fileName, pathName, shortFileName);

    int i = shortFileName.lastIndexOf(".");
    if (i == -1)
    {
        return false;
    }

    // Change shortFileName from <file>.c to <file><suffix>.c
    shortFileName = shortFileName.left(i) +
                    QString::fromStdString(vcData.saveSuffix) + shortFileName.mid(i);

    // Determine output path
    if (isAbsolutePath(vcData.saveDirectory))
    {
        // Absolute path - strip path directory
        int len = projectBaseDir.length();
        if (projectBaseDir == fileName.left(len))
        {
            outPath = fixPath(vcData.saveDirectory, true);
        }
        else
        {
            // Shouldn't happen
            //cout << "Input name " << fileName.toStdString() << " failed to match path " << projectBaseDir.toStdString() << endl;
            return false;
        }

        outPath += pathName.mid(len);
    }
    else
    {
        // Get relative path
        QString preprocPathName;
        QString preProcFileName;
        getPreProcFilename(fileName, preprocPathName, preProcFileName);
        outPath = preprocPathName + fixPath(vcData.saveDirectory, true);
    }

    instrFileName = outPath + shortFileName;
    return true;
}

// Get filename and path to instrumented file based on source fileName
bool MainWindow::getInstrFilenameFromIndex(QModelIndex index, QString &instrPathName, QString &instrFileName)
{
    QString fileName;

    getFullFileName(index, fileName);
#ifdef OLD_WAY
    QString pathName;
    QString preProcFileName;
    // Get path and filename of preprocessed file
    if (!getPreProcFilename(fileName, instrPathName, preProcFileName))
    {
        return false;
    }

    // Get path and shortened filename
    QString shortFileName;
    getDirFileNames(fileName, pathName, shortFileName);

    // Change filename to HTML directory/file.html
    int i = shortFileName.lastIndexOf(".");
    if (i == -1)
    {
        return false;
    }

    addPath(vcData.saveDirectory, instrPathName);
/*    instrPathName += vcData.saveDirectory;
    instrFileName = instrPathName + "/" + shortFileName.left(i) +
                      vcData.saveSuffix + shortFileName.mid(i);*/
    instrFileName = instrPathName + shortFileName.left(i) +
                    QString::fromStdString(vcData.saveSuffix) + shortFileName.mid(i);

    return true;
#else
    return getInstrFilename(fileName, instrPathName, instrFileName);
#endif
}

// Generate path and filename of HTML file based on original fileName.
// Returns:
// outPath - output file path.
// htmlFileName - full path to instrumented file.
// Returns true if succeeded; false if filename is invalid
bool MainWindow::getHtmlFilename(QString fileName, QString &outPath, QString &htmlFileName)
{
    // Determine path to new file
    QString pathName;
    QString shortFileName;
    getDirFileNames(fileName, pathName, shortFileName);

    int i = shortFileName.lastIndexOf(".");
    if (i == -1)
    {
        return false;
    }

    // Change shortFileName from <file>.c to <file><suffix>.c
    shortFileName = shortFileName.left(i) +
                    QString::fromStdString(vcReportData.htmlSuffix) + ".html";

    // Determine output path
    if (isAbsolutePath(vcReportData.htmlDirectory))
    {
        // Absolute path - strip path directory
        int len = projectBaseDir.length();
        if (projectBaseDir == fileName.left(len))
        {
            outPath = fixPath(vcReportData.htmlDirectory, false);
        }
        else
        {
            // Shouldn't happen
            //cout << "Input name " << fileName.toStdString() << " failed to match path " << projectBaseDir.toStdString() << endl;
            return false;
        }

        outPath += pathName.mid(len);
    }
    else
    {
        // Get relative path
        QString instrPathName;
        QString instrFileName;
        getInstrFilename(fileName, instrPathName, instrFileName);
        outPath = instrPathName + fixPath(vcReportData.htmlDirectory, true);
    }

    htmlFileName = outPath + shortFileName;
    return true;
}

// Take preprocessed source and remove all linemarkers of the form:
// # linenum filename flags
// Save useful linemarkers to a preprocessed map file in the same directory as the preprocessed file
bool MainWindow::mapPreprocessedSource(QString &source, QString fileName, QString &preProcFileName)
{
    // Remove file extension and add new .ppmap extension
    int index = preProcFileName.lastIndexOf(".");
    if (index < 0)
    {
        QMessageBox::critical(0, QString("Error"), QString("Illegal preprocessed filename %1")
                              .arg(preProcFileName));
        return false;
    }

    QString preprocMapName = preProcFileName.left(index) + ".ppmap";
    QFile ppMapFile(preprocMapName);
    if (!ppMapFile.open(QFile::WriteOnly | QFile::Truncate))
    {
        QMessageBox::critical(this, tr("Error"), tr("Cannot create map file %1:\n%2")
                             .arg(fileName)
                             .arg(ppMapFile.errorString()));
        return false;
    }

    QTextStream ppMapOut(&ppMapFile);
    ppMapOut << "{\n  \"filename\": \"" << fileName << "\",\n";
    ppMapOut << "  \"preproc_filename\": \"" << preProcFileName << "\",\n";
    ppMapOut << "  \"linemarkers\":\n  [";
    bool gotMarker = false;    // Flag when linemarker added

    QString newSrc;
    int lineNum = 1;
    bool startLine = true;

    // Search for # at start of line
    // Count line numbers whenever a '\n' is encountered
    for (int i = 0; i < source.size(); ++i)
    {
        if (startLine && (source[i] == '#') && (source[i + 1] == ' '))
        {
            // Linemarker found - handle it
            i += 2; // Skip # and space
            int curLine = 0;
            while ((source[i] >= '0') && (source[i] <= '9'))
                curLine = curLine * 10 + source[i++].digitValue();

            i += 2; // Skip space and "
            QString srcName;
            while (source[i] != '"')
                srcName += source[i++];

            // Skip to end of line
            while (source[i] != '\n')
                ++i;

            // Now do something useful with curLine and fileName
            // Weed out things like "<command-line" and "<built-in>"
            if (srcName[0] != '<')
            {
//                if (srcName == fileName)
//                    cout << "Match";
                if (gotMarker)
                    ppMapOut << ",";
                else
                    gotMarker = true;

                ppMapOut << "\n    [ " << lineNum << ", " << curLine << ", ";
                ppMapOut << "\"" << srcName << "\" ]";
            }
        }
        else
        {
            newSrc += source[i];
            if (source[i] == '\n')
            {
                ++lineNum;
                startLine = true;
            }
            else
                startLine = false;
        }
    }

    ppMapOut << "\n  ]\n}\n";
    ppMapOut.flush();
    ppMapFile.close();

    // Return preprocessed source minus the linemarkers
    source = newSrc;

    return true;
}

// Preprocess specified file
bool MainWindow::preprocessFile(QString fileName, bool force)
{
    QString pathName;
    QString preProcFileName;
    //cout << "preprocess" << fileName.toStdString() << endl;
    // Get path and filename of preprocessed file
    if (!getPreProcFilename(fileName, pathName, preProcFileName))
    {
        QMessageBox::critical(0, QString("Error"), QString("Illegal filename %1")
                              .arg(preProcFileName));
        return false;
    }
    //cout << "preprocess path" << pathName.toStdString() << endl;

    if (preProcFileName == fileName)
    {
        QMessageBox::critical(0, QString("Error"), QString("Will overwrite original - either directory or suffix must be non-null"));
        return false;
    }

    // Generate preprocessor directory path if needed
    QDir preProcDir(pathName);
    if (!preProcDir.exists(pathName))
    {
        QPlainTextEditAppendText(ui->outputTextEdit, "Creating directory " + pathName + "\n");
        if (!preProcDir.mkpath("."))
        {
            QMessageBox::critical(0, QString("Error"), QString("Could not create directory %1")
                                  .arg(pathName));
            return false;
        }
    }

    bool doPreProc = true;
    bool retVal = true;     // Assume preprocessing succeeds

    // Check if file exists and force preprocessing is false
    if (!force && QFile::exists(preProcFileName))
    {
        // Check time of modification
        QFileInfo fileModInfo(fileName);
        QDateTime fileModTime = fileModInfo.lastModified();
        QFileInfo preProcModInfo(preProcFileName);
        QDateTime preProcModTime = preProcModInfo.lastModified();

        if (preProcModTime > fileModTime)
        {
            // Preprocessed file is newer than original - skip preprocessing
            doPreProc = false;
//                QPlainTextEditAppendText(ui->outputTextEdit, "Preprocessing not needed\n");
        }
    }

    if (doPreProc)
    {
      QString ppSource;

      QPlainTextEditAppendText(ui->outputTextEdit, "Preprocessing ");
      QPlainTextEditAppendText(ui->outputTextEdit, fileName, Qt::blue);
      QPlainTextEditAppendText(ui->outputTextEdit, " ...\n");
      appendAnsiText(ui->errorTextEdit,  "\033[37m-----\033[0m\033[30m " + preProcFileName + " \033[0m\033[37m-----\033[0m\n\n");

      if (vcPreProcData.useVampPreprocessor)
      {
          // Use internal preprocessor
          VampPreprocessor vampPreproc;
          string outStr, errStr;

          retVal = vampPreproc.Preprocess(fileName.toUtf8().data(), outStr, errStr, vcPreProcData);
          appendAnsiText(ui->errorTextEdit, QString::fromStdString(errStr));

          if (retVal)
          {
              ppSource = QString::fromStdString(outStr.c_str());

          }
        }
      else
      {
        // Call external preprocessor
        // Create a process to perform preprocessing
        myProcess = new QProcess(this);
        // Set path to where project vamp.cfg lives
        myProcess->setWorkingDirectory(projectDirName);

//        connect (myProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(printOutput()));
//        connect (myProcess, SIGNAL(readyReadStandardError()), this, SLOT(printError()));

        QString preprocStr = QString::fromStdString(vcPreProcData.preProcArguments);

        // Add list of -I include directories
        if (preprocStr.indexOf("%includes%") >= 0)
        {
            QString incStr;
            for (int i = 0; i < vcPreProcData.includePaths.size(); ++i)
            {
                if (i > 0)
                    incStr += " ";
                incStr += "-I" + QString::fromStdString(vcPreProcData.includePaths[i]);
            }

            preprocStr.replace("%includes%", incStr);
        }

        // Add list of -D define directives
        if (preprocStr.indexOf("%defines%") >= 0)
        {
            QString defStr;
            for (int i = 0; i < vcPreProcData.definesList.size(); ++i)
            {
                if (i > 0)
                    defStr += " ";
                defStr += "-D" + QString::fromStdString(vcPreProcData.definesList[i]);
            }

            preprocStr.replace("%defines%", defStr);
        }

        // Generate list of arguments to pass to preprocessor
        QStringList preProcArgs;
        preProcArgs = preprocStr.split(" ");

        // Replace %infile% with filename
        // Replace %outfile% with output filename
        preProcArgs.replaceInStrings("%infile%", fileName, Qt::CaseInsensitive);
//        preProcArgs.replaceInStrings("%outfile%", preProcFileName, Qt::CaseInsensitive);

        // Start specified preprocessor
        myProcess->start(QString::fromStdString(vcPreProcData.preProcPath), preProcArgs);
        myProcess->waitForFinished();

        QByteArray ppSrcArray = myProcess->readAllStandardOutput();
        ppSource = QString(ppSrcArray);

        QByteArray ppErrArray = myProcess->readAllStandardError();
//        QString ppErr = QString(ppErrArray);

//        QPlainTextEditAppendText(ui->outputTextEdit, ppErrArray);
        QPlainTextEditAppendText(ui->errorTextEdit, ppErrArray);
/*
        cout << "Normal Exit = " << (int) (myProcess->exitStatus() == QProcess::NormalExit) << "\n";
        if (myProcess->exitStatus() != QProcess::NormalExit)
            cout << "Exit code = " << myProcess->exitCode() << "\n";
//        if (myProcess->exitStatus() != QProcess::NormalExit)
//        if (ppErrArray.size())
*/
        if (ppErrArray.contains("error:"))
            // Error occurred during preprocessing
            retVal = false;
      }

      if (retVal)
      {
          if (vcPreProcData.showLineMarkers)
          {
              // Strip linemarkers and build database mapping preprocessed lines to source lines
              if (!mapPreprocessedSource(ppSource, fileName, preProcFileName))
                  return false;
          }

          QFile ppoFile(preProcFileName);
          if (!ppoFile.open(QFile::WriteOnly | QFile::Truncate))
          {
              QMessageBox::critical(this, tr("Error"), tr("Cannot create file %1:\n%2")
                                   .arg(fileName)
                                   .arg(ppoFile.errorString()));
              return false;
          }

          QTextStream ppOut(&ppoFile);
          ppOut << ppSource;
          ppOut.flush();
          ppoFile.close();

          QPlainTextEditAppendText(ui->outputTextEdit, "Preprocessing complete.\n");
      }
      else
      {
          //QPlainTextEditAppendText(ui->outputTextEdit, "\nFatal error(s) occurred - instrumentation aborted\n");

          QPlainTextEditAppendText(ui->outputTextEdit, "Preprocessing ");
          QPlainTextEditAppendText(ui->outputTextEdit, "failed.\n", Qt::red);
      }
    }

    // Now set fileName to preprocessed name
//Why? Unused    fileName = preProcFileName;

    return retVal;
}

// Instrument specified file
bool MainWindow::instrumentFile(QString fileName)
{
    QString inName;     // Path to input file
    QString outPath;    // Path to output file

    // See if preprocessing needed
    if (vcPreProcData.doPreProcess)
    {
        if (!preprocessFile(fileName, false))
            return false; // Preprocessing failed
    }

    QString outName;
    getInstrFilename(fileName, outPath, outName);

    // Now set inName to preprocessed/original name
    QString pathName;
    getPreProcFilename(fileName, pathName, inName);

    QPlainTextEditAppendText(ui->outputTextEdit, "Instrumenting ");
    QPlainTextEditAppendText(ui->outputTextEdit, fileName, Qt::blue);
    QPlainTextEditAppendText(ui->outputTextEdit, " ...\n");

    // FIXME: Check dates and see if instrumentation needed

#ifdef USE_PROCESS
    // Create process to perform instrumentation
    myProcess = new QProcess(this);

    connect (myProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(printOutput()));
    connect (myProcess, SIGNAL(readyReadStandardError()), this, SLOT(printError()));

#ifdef VAMP_ADD_EXE_PATH
    QDir dir;
    QString path;
    path = dir.currentPath();
    QString program = path + "/vamp.exe";
#else
    QString program = "./vamp";
#endif
    QStringList arguments;
    arguments.push_back(inName);
    myProcess->start(program, arguments);
    myProcess->waitForFinished();
#else
    static char argv0[] = "./vamp";
    char *argv[2];
    argv[0] = argv0;//"./vamp";

    QByteArray fName = inName.toUtf8();
    argv[1] = fName.data();

#ifdef REDIRECT_COUT
    // remember cout's old streambuf
    streambuf * cout_strbuf(cout.rdbuf());

    // create the new stream to "redirect" cout's output to
    ostringstream coutOutput;

    // set cout's streambuf to be the ostringstream's streambuf
    cout.rdbuf(coutOutput.rdbuf());

    // remember cerr's old streambuf
    streambuf * cerr_strbuf(cout.rdbuf());

    // create the new stream to "redirect" cout's output to
    ostringstream cerrOutput;

    // set cerr's streambuf to be the ostringstream's streambuf
    cerr.rdbuf(cerrOutput.rdbuf());

//    StdCapture Cap;
//    Cap.BeginCapture();
//    Vamp vamp(this);
//    void (*outPtr)(char *) = &instOut;
//    void (*errPtr)(char *) = &instErr;
//    vamp.Instrument(2, argv, &instOut, &instErr);
    //vamp.Instrument(2, argv, ui->outputTextEdit, ui->errorTextEdit);
#endif

    // Set path to where project vamp.cfg lives
    QDir::setCurrent(projectDirName);
    ostringstream vOut;
    ostringstream vErr;
    Vamp *vamp = new Vamp(&vOut, &vErr);
    // Add a delimiter to error output
    //vErr << "\033[37m+++++\033[30m " << argv[1] << " \033[37m+++++\n\n";
    char respath[512];
    char *pth = realpath(argv[1], respath);
    vErr << "\033[37m+++++\033[0m\033[30m " << pth << " \033[0m\033[37m+++++\033[0m\n\n";
// FIXME: vcData is already computed. vamp.Instrument parses vamp.cfg!
//    vamp.Instrument(2, argv, vcData.langStandard);
    bool success = vamp->Instrument(2, argv, outName.toStdString(), vcData);
    QPlainTextEditAppendText(ui->outputTextEdit, QString::fromStdString(vOut.str()));
    appendAnsiText(ui->errorTextEdit, QString::fromStdString(vErr.str()));

    delete vamp;

#ifdef REDIRECT_COUT
//    Cap.EndCapture();
//    string coutString = Cap.GetOutCapture();
    //    QPlainTextEditAppendText(ui->outputTextEdit, QString::fromStdString(coutString));
    //    string cerrString = Cap.GetErrCapture();
    //    QPlainTextEditAppendText(ui->errorTextEdit, QString::fromStdString(cerrString));
    //    QPlainTextEditAppendText(ui->errorTextEdit, QString::fromStdString(errStr));

    string coutString = coutOutput.str();
    string cerrString = cerrOutput.str();
    QPlainTextEditAppendText(ui->outputTextEdit, QString::fromStdString(coutString));
    QPlainTextEditAppendText(ui->errorTextEdit, QString::fromStdString(cerrString));
    cout.rdbuf(cout_strbuf);
    cerr.rdbuf(cerr_strbuf);
#endif
#endif

    return success;
}

// Process results for specified file
bool MainWindow::processFile(QString fileName)
{
    // Get path to .json file (instrumentation)
    QString instrPath;
    QString instrFileName;
    getInstrFilename(fileName, instrPath, instrFileName);
//    QString pathName;
//    QString shortFileName;
//    getDirFileNames(instrFileName, pathName, shortFileName);
//    int i = shortFileName.lastIndexOf(".");
    int i = instrFileName.lastIndexOf(".");
    if (i == -1)
    {
        QMessageBox::critical(0, QString("Error"), QString("Illegal filename %1")
                              .arg(instrFileName));
        return false;
    }

//    addPath(vcData.saveDirectory, pathName);
/*    if (!vcData.saveDirectory.isEmpty())
        pathName += vcData.saveDirectory + "/";*/
//    QString procFileName = pathName + shortFileName.left(i) + QString::fromStdString(vcReportData.htmlSuffix) + ".json";
    QString procFileName = instrFileName.left(i) + ".json";

//    QPlainTextEditAppendText(ui->outputTextEdit, "Processing results from " + procFileName + " ...\n");
    QPlainTextEditAppendText(ui->outputTextEdit, "Processing results from ");
    QPlainTextEditAppendText(ui->outputTextEdit, procFileName + " ...\n", Qt::blue);

#ifdef USE_PROCESS
    // Create process to perform instrumentation
    myProcess = new QProcess(this);
    myProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect (myProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(printOutput()));
    connect (myProcess, SIGNAL(readyReadStandardError()), this, SLOT(printError()));

    QString program = "./vamp_process";
    QStringList arguments;
    arguments.push_back(procFileName);
    myProcess->start(program, arguments);
    myProcess->waitForFinished();
#else
/*    static char argv0[] = "./vamp_process";
    char *argv[2];
    argv[0] = argv0;//"./vamp_process";
    QByteArray fName = fileName.toUtf8();
    argv[1] = fName.data();
*/

    // Set path to where project vamp.cfg lives
    QDir::setCurrent(projectDirName);
    ostringstream vOut;
    ostringstream vErr;

    // Add a delimiter to error output
    char respath[512];
    char *pth = realpath(procFileName.toUtf8().data(), respath);
//    vErr << "\033[37m+++++\033[0m\033[30m " << procFileName.toUtf8().data() << " \033[0m\033[37m+++++\033[0m\n\n";
    vErr << "\033[37m+++++\033[0m\033[30m " << pth << " \033[0m\033[37m+++++\033[0m\n\n";

    VampProcess vampProcess(&vOut, &vErr);
    if (vcPreProcData.showLineMarkers)
    {
        QString preProcPath;
        QString preProcFileName;
        getInstrFilename(fileName, preProcPath, preProcFileName);

        vampProcess.processLineMarkers(preProcFileName.toUtf8().data());
    }
    QString htmlFileName;
    QString htmlPath;
    getHtmlFilename(fileName, htmlPath, htmlFileName);
    bool processSuccess = vampProcess.processFile(procFileName.toUtf8().data(), vcReportData);

    QPlainTextEditAppendText(ui->outputTextEdit, QString::fromStdString(vOut.str()));
    appendAnsiText(ui->errorTextEdit, QString::fromStdString(vErr.str()));
    if (!processSuccess)
    {
        QPlainTextEditAppendText(ui->outputTextEdit, "Process results failed\n");
    }
    return processSuccess;
#endif
}

// Show specified file in right-hand pane
// isHtml is true if file is in HTML format
// isHtml is false implies place file within <pre><file></pre>;
// used when file is a source file
void MainWindow::showHtmlFile(QString fileName, bool isHtml)
{
    // read from file
    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, tr("Unable to open file"), tr("%1:\n%2")
                                 .arg(fileName)
                                 .arg(file.errorString()));
        return;
    }

    // Set group box title to filename
    ui->resultsGroupBox->setTitle(fileName);

    // Enable display of file view
    ui->resultsGroupBox->show();
    ui->webView->show();
    if (isHtml)
    {
        ui->webView->setUrl("file:///" + fileName);
        return;
    }
    else
    {
#ifdef FORMAT_HTML
        QTextStream out(&file);
        QString output = out.readAll();
#ifndef ENCODE_HTML
        output.replace('<', "&lt;");
        output.replace('>', "&gt;");
        output.replace('&', "&amp;");
#endif

        output = "<html><body><pre>" + output + "</pre></body></html>";
        //output = "<textarea name=\"code\" class=\"c++\">" + output + "</textarea>";
        //output = "<html><body><pre name=\"code\" class=\"c++\">" + output + "</pre></body></html>";

        // display contents
        ui->webView->setHtml(output);
        ////ui->webView->setContent(output.toLocal8Bit());
        //ui->webView->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
#else
        ui->webView->setUrl("file:///" + fileName);
        return;
#endif
    }
}

// QPlainTextEdit doesn't autoscroll properly (probably when a line wraps
// If scrollbar is 95% down, force an autoscroll
void MainWindow::QPlainTextEditAppendText(QPlainTextEdit* editor, QString text, QColor color)
{
    bool flagAutoScroll = false;
    QScrollBar* scrollBar = editor->verticalScrollBar();
    int valueBeforeInsert = scrollBar->value();
    int max = scrollBar->maximum();
    if( valueBeforeInsert >= max * 95 / 100 )
    {
        flagAutoScroll = true;
    }
    QTextCharFormat format;
    format.setForeground(QBrush(color));
    QTextCursor cursor_old = editor->textCursor();
    QTextCursor cursor_new = cursor_old;
    cursor_new.movePosition( QTextCursor::End, QTextCursor::MoveAnchor, 1 );
    editor->setTextCursor( cursor_new );
    editor->setCurrentCharFormat(format);
    editor->insertPlainText( text );
    editor->setTextCursor( cursor_old );

    if( flagAutoScroll )
    {
        scrollBar->setValue( scrollBar->maximum() );
    }
    else
    {
        scrollBar->setValue( valueBeforeInsert );
    }
}

// Called when a process has output on stdout
void MainWindow::printOutput()
{
#ifndef LONG_READ
    QByteArray byteArray = myProcess->readAllStandardOutput();
    QStringList strLines = QString(byteArray).split("\n");

    // Add each line to output tab
    foreach (QString line, strLines)
    {
        QPlainTextEditAppendText(ui->outputTextEdit, line);
        ui->outputTextEdit->repaint();
    }
#else
    //ui->outputTextEdit->appendPlainText(myProcess->readAllStandardOutput());
    QPlainTextEditAppendText(ui->outputTextEdit, myProcess->readAllStandardOutput());
#endif
}

// Called when a process has output on stderr
void MainWindow::printError()
{
/*    QByteArray byteArray = myProcess->readAllStandardError();
    QStringList strLines = QString(byteArray).split("\n");

    foreach (QString line, strLines)
    {
        // Add each line to both output and error tabs
        QPlainTextEditAppendText(ui->outputTextEdit, line);
        ui->outputTextEdit->repaint();
        QPlainTextEditAppendText(ui->errorTextEdit, line);
        ui->errorTextEdit->repaint();
    }*/
    QByteArray byteArray = myProcess->readAllStandardError();
    ui->outputTextEdit->appendPlainText(byteArray);
//    ui->errorTextEdit->appendPlainText(byteArray);
    QPlainTextEditAppendText(ui->outputTextEdit, byteArray);
    QPlainTextEditAppendText(ui->errorTextEdit, byteArray);
}

void MainWindow::appendAnsiText(QPlainTextEdit *textEdit, QString str)
{
    QString tmpStr;
    QColor curColor = Qt::black;
    QTextCharFormat format;

    static QColor ansiColors[] = {
        Qt::blue,
        Qt::red,
        Qt::darkGreen,//QColor(0x00C000),   // green
        Qt::yellow,
        Qt::blue,
        Qt::magenta,
        Qt::cyan,
        Qt::black
    };

    for (int i = 0; i < str.size(); ++i)
    {
        if ((str[i] == '\033') && (str[i + 1] == '['))
        {
            // Start of ANSI escape sequence
            if ((str[i + 2] == '0') && (str[i + 3] == 'm'))
            {
                //textEdit->insertPlainText(tmpStr);
                QPlainTextEditAppendText(textEdit, tmpStr, curColor);
                tmpStr.clear();
                curColor = QColor(0x505050);
                i += 3;
            }
            else
            if (((str[i + 2] == '3') || (str[i + 2] == '4')) && (str[i + 4] == 'm'))
            {
                //textEdit->insertPlainText(tmpStr);
                QPlainTextEditAppendText(textEdit, tmpStr, curColor);
                tmpStr.clear();
                curColor = ansiColors[str[i + 3].digitValue() & 7];
                i += 4;
            }
        }
        else
            tmpStr += str[i];
    }

    if (tmpStr != "")
    {
        //textEdit->insertPlainText(tmpStr);
        QPlainTextEditAppendText(textEdit, tmpStr, curColor);
    }
//    textEdit->repaint();
}

void MainWindow::appendOutput(QString str)
{
    QPlainTextEditAppendText(ui->outputTextEdit, str);
}

void MainWindow::appendError(QString str)
{
    //QPlainTextEditAppendText(ui->errorTextEdit, str);
    appendAnsiText(ui->errorTextEdit, str);
}

// Add File(s) to project starting at specified directory
void MainWindow::addFiles(QString startDir)
{
    // Get list of files selected
    QStringList fileNames = QFileDialog::getOpenFileNames(this, tr("Add File"), startDir,
            tr("Project Files (*.c)"));

    // Add specified files to tree
    for (int i = 0; i < fileNames.size(); i++)
    {
        QStringList fileInfo;
        fileInstInfo instInfo;
        // Load .vrpt for file
        loadFileInfo(fileNames[i], instInfo);
        // Load any instrumentation info from .vrpt file
        getFileStats(instInfo, fileInfo);
        // Insert file and stats into project tree
        insertPath(fileNames[i], fileInfo);
        projectChanged = true;
    }

    // Recompute base path since something's been added to tree
    getBasePath();
    //cout << "BaseDir = " << baseDir.toStdString() << endl << flush;
}

// File->Exit called
void MainWindow::on_actionE_xit_triggered()
{
    if (saveProject(true))
        qApp->exit();
}

// Project->Add File called
void MainWindow::on_action_Add_triggered()
{
    addFiles(projectDirName);
}

// File in tree doubled-clicked
void MainWindow::on_projectTreeView_doubleClicked(const QModelIndex &index)
{
    QModelIndex parent;
    QModelIndex child;

    // Get index to filename
    getFileIndex(parent, child);

    QString fileName;

    // Get full filename with path
    getFullFileName(parent, fileName);

#ifdef OLD_WAY
    QString pathName;
    QString shortFileName;

    // Split filename into path/file
    getDirFileNames(fileName, pathName, shortFileName);

    QString preProcPathName;
    QString preProcFileName;

    // Get path to source used
    getPreProcFilename(fileName, preProcPathName, preProcFileName);

    // Change filename to HTML directory/file.html
    int i = shortFileName.lastIndexOf(".");
    if (i == -1)
    {
        QMessageBox::critical(0, QString("Error"), QString("Illegal filename %1")
                              .arg(fileName));
        return;
    }

//    fileName = preProcPathName + vcReportData.htmlDirectory + "/" + shortFileName.left(i) + vcReportData.htmlSuffix + ".html";
    addPath(vcData.saveDirectory, preProcPathName);
    addPath(vcReportData.htmlDirectory, preProcPathName);
    fileName = preProcPathName + shortFileName.left(i) + QString::fromStdString(vcReportData.htmlSuffix) + ".html";
#else
    QString htmlFileName;
    QString htmlPath;
    getHtmlFilename(fileName, htmlPath, htmlFileName);
#endif
    showHtmlFile(htmlFileName, true);
}

// File in tree right-clicked - create context menu
void MainWindow::on_projectTreeView_customContextMenuRequested(const QPoint &pos)
{
    QMenu menu(this);
    QModelIndex index;
    QModelIndex child;
    getFileIndex(index, child);

    QAbstractItemModel *model = ui->projectTreeView->model();

    if (model->rowCount(index) == 0)
    {
        // File selected
        menu.addAction(removeFileAct);
        menu.addAction(showFileAct);
        if (vcPreProcData.doPreProcess)
        {
            QString pathName;
            QString preProcFileName;
            QString fileName;
            getFullFileName(index, fileName);
            //QVariant a = index.data();
            //QString fileName = a.toString();

            if (getPreProcFilename(fileName, pathName, preProcFileName))
            {
                if (QFile::exists(preProcFileName))
                    menu.addAction(showPreProcFileAct);
            }

            menu.addAction(preprocessFileAct);
//QPlainTextEditAppendText(ui->outputTextEdit, "fileName " + fileName + ", pathName " + pathName + " = preProcFileName " + preProcFileName + "\n");
        }
        // Offer Show instrumented file (if exists)
        QString instrPathName;
        QString instrFileName;
        if (getInstrFilenameFromIndex(index, instrPathName, instrFileName))
        {
            if (QFile::exists(instrFileName))
                menu.addAction(showInstrFileAct);
        }

        menu.addAction(instrumentFileAct);
        menu.addAction(processResultsAct);
        // Only offer Process Results if Inst column isn't "NO".
        QVariant a = model->index(index.row(), 1, index.parent()).data();
        if (a.toString() != "NO")
            menu.addAction(showResultsAct);
        menu.addAction(addFilesAct);
    }
    else
    {
        // Folder selected
        menu.addAction(removeFolderAct);
        if (vcPreProcData.doPreProcess)
        {
            menu.addAction(preprocessFolderAct);
        }
        menu.addAction(instrumentFolderAct);
        menu.addAction(processFolderAct);
        menu.addAction(addFilesAct);
    }
    menu.exec(QCursor::pos());
}

// Project->New Project called
void MainWindow::on_actionNew_Project_triggered()
{
    QString filter = "Project Files (*.vproj)";
    QString fileName = QFileDialog::getSaveFileName(this, "New Project", QString(),
                                                          "Project Files (*.vproj)", &filter);

    if (!fileName.isEmpty())
    {
        // Clear splash screen
        ui->webView->setContent("");
        // Remove splash screen links
        ui->webView->page()->setLinkDelegationPolicy(QWebPage::DontDelegateLinks);

        // Act as if Close Project was selected first
        if (ui->projectTreeView->model() != NULL)
            on_action_Close_Project_triggered();

        // Kludge - Qt doesn't always return the extension
        QString checkExt = fileName.right(6);
        if (checkExt != ".vproj")
            fileName += ".vproj";
        checkProject(fileName, true);
    }
}

// Project->Open Project called
void MainWindow::on_actionOpen_Project_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Project"), QString(),
                                                          tr("Project Files (*.vproj)"));

    if (!fileName.isEmpty())
    {
        // Clear splash screen
        ui->webView->setContent("");
        // Remove splash screen links
        ui->webView->page()->setLinkDelegationPolicy(QWebPage::DontDelegateLinks);

        // Act as if Close Project was selected first
        if (ui->projectTreeView->model() != NULL)
            on_action_Close_Project_triggered();
        // Set a wait cursor while project is loaded
        this->setCursor(QCursor(Qt::WaitCursor));
        checkProject(fileName, false);
        loadProjectData(fileName);
        this->unsetCursor();
        //projectChanged = false;
    }
}

// Project->Save Project called
void MainWindow::on_actionSave_Project_triggered()
{
    saveProject(false);
}

// Project->Edit Configuration called
void MainWindow::on_actionEdit_Configuration_triggered()
{
    editConfig();
}

// Project->Close Project called
void MainWindow::on_action_Close_Project_triggered()
{
    if (saveProject(true))
    {
        // Empty tree
        QAbstractItemModel *model = ui->projectTreeView->model();
        int rowCnt = model->rowCount();
        if (rowCnt > 0)
            model->removeRows(0, rowCnt);
        // Reset column headers
        ui->projectTreeView->setModel(NULL);

        // Disable tree view for project
        ui->projectGroupBox->hide();
        ui->projectTreeView->hide();

        // Disable output/errors view for project
        ui->tabWidget->hide();

        // Disable results view
        ui->resultsGroupBox->hide();

        // Clear and reset Results view
        ui->webView->setHtml("");
        ui->resultsGroupBox->setTitle("Results");

        // Clear output and error text
        ui->outputTextEdit->clear();
        ui->errorTextEdit->clear();

        // Disable Project menu option "Add"
        ui->action_Add->setEnabled(false);
        ui->actionEdit_Configuration->setEnabled(false);

        // Disable File menu option "Save Project"
        ui->actionSave_Project->setEnabled(false);

        // Disable File menu option "Close Project"
        ui->action_Close_Project->setEnabled(false);

        // Disable Project menu option "Generate vamp_output.c"
        ui->action_Generate_vamp_output_c->setEnabled(false);

        projectChanged = false;
    }
}

// Project->Generate Summary called
void MainWindow::on_action_Generate_Summary_triggered()
{
    QString filter = "HTML Files (*.html)";
    QFileDialog dlg;
    dlg.setDirectory(projectPath);
    QString fileName = dlg.getSaveFileName(this, "Generate Summary", QString(),
                                                          "HTML Files (*.html)", &filter);

    if (!fileName.isEmpty())
    {
        // Kludge - Qt doesn't always return the extension
        QString checkExt = fileName.right(5);
        if (checkExt != ".html")
            fileName += ".html";

        // Open summary file for output
        QFile summaryFile(fileName);
        if (!summaryFile.open(QFile::WriteOnly | QFile::Truncate))
        {
            QMessageBox::critical(this, tr("Error"), tr("Cannot create file %1:\n%2")
                                 .arg(fileName)
                                 .arg(summaryFile.errorString()));
            return;
        }

        // Write project file as JSON database
        QTextStream out(&summaryFile);

        QString outStr;
        genSummary(outStr, 0, QModelIndex());
        out << outStr;
        out.flush();

        /* Test of printing QTreeView
        // Bottom only goes to what is displayed, not entire content
        // May be useful to render as HTML instead?
        #ifndef OLD_WAY
            QPrinter printer;
            QPrintDialog *printDialog = new QPrintDialog(&printer, this);
            if (printDialog->exec() == QDialog::Accepted)
            {
                QPainter p(&printer);
                QPixmap pm = QPixmap::grabWidget(ui->projectTreeView);
                p.drawPixmap(0, 0, pm);
            }
            //delete printer;
        #else
        // Old way:
        // Screws up left edge for some reason
            QPrinter printer;
            printer.setOutputFormat(QPrinter::PdfFormat);
            printer.setOutputFileName("c:/advent/nonwritable.pdf");
            painter.begin(&printer);
            double xscale = printer.pageRect().width() / double(ui->projectTreeView->width());
            double yscale = printer.pageRect().height() / double(ui->projectTreeView->height());
            double scale = qMin(xscale, yscale);
            painter.translate(printer.paperRect().x() + printer.pageRect().width() / 2,
                              printer.paperRect().y() + printer.pageRect().height() / 2);
            painter.scale(scale, scale);
            painter.translate(-width() / 2, -height() / 2);
            ui->projectTreeView->render(&painter);
        #endif
        //*/
    }
}

// Project->Print Summary called
void MainWindow::on_action_Print_Summary_triggered()
{
    QString outStr;
    genSummary(outStr, 0, QModelIndex());

    QTextDocument *document = new QTextDocument();
    document->setHtml(outStr);

    QPrinter printer;

    QPrintDialog *dialog = new QPrintDialog(&printer, this);
    if (dialog->exec() == QDialog::Accepted)
    {
        document->print(&printer);
    }

    delete document;
}

// Project->Generate vamp_output.c called
void MainWindow::on_action_Generate_vamp_output_c_triggered()
{
    // Get a list of files in project
    QStringList fileList;
    QStringList nameList;
    genFileList("", QModelIndex(), fileList);
    fileList.sort();
    for (int i = 0; i < fileList.size(); ++i)
    {
        QString dirName;
        QString fileName;
        getDirFileNames(fileList[i], dirName, fileName);
        int j = fileName.lastIndexOf(".");
        if (j == -1)
        {
            QMessageBox::critical(0, QString("Error"), QString("Illegal filename %1")
                                  .arg(fileList[i]));
            return;
        }
        nameList.push_back(fileName.left(j));
    }

    // Open vamp_output.c for output
    QString instrPath;
    getInstrPath(instrPath);
    //QString vampOutputName(projectDirName + "vamp_output.c");
    QString vampOutputName(instrPath + "vamp_output.c");
    QFile voFile(vampOutputName);
    if (!voFile.open(QFile::WriteOnly | QFile::Truncate))
    {
        QMessageBox::critical(this, tr("Error"), tr("Cannot create file %1:\n%2")
                             .arg(vampOutputName)
                             .arg(voFile.errorString()));
        return;
    }

    QTextStream out(&voFile);

    // Open vamp_output.h for output
    //QString vampHoutputName(projectDirName + "vamp_output.h");
    QString vampHoutputName(instrPath + "vamp_output.h");
    QFile vHoFile(vampHoutputName);
    if (!vHoFile.open(QFile::WriteOnly | QFile::Truncate))
    {
        QMessageBox::critical(this, tr("Error"), tr("Cannot create file %1:\n%2")
                             .arg(vampHoutputName)
                             .arg(vHoFile.errorString()));
        return;
    }

    QTextStream hOut(&vHoFile);

    out << "/* Auto-generated by VampGui.exe */\n\n";
    hOut << "/* Auto-generated by VampGui.exe */\n\n";
#ifdef OLD_VAMP_PROCESS
    for (int i = 0; i < nameList.size(); ++i)
    {
        out << "extern void _vamp_" << nameList[i] << "_output();\n";
    }

    out << "\nvoid _vamp_output()\n{\n";

    for (int i = 0; i < nameList.size(); ++i)
    {
        out << "  _vamp_" << nameList[i] << "_output();\n";
    }
#else
    // Build list of _VAMP_<file>_INDEX definitions
    for (int i = 0; i < nameList.size(); ++i)
    {
        hOut << "#define _VAMP_" << nameList[i].toUpper() << "_INDEX " << i << "\n";
    }

    out << "static char *_vamp_filenames[" << fileList.size() << "] = {\n";
    for (int i = 0; i < nameList.size(); ++i)
    {
        out << "  \"" << nameList[i] << "\"";
        if (i != fileList.size() - 1)
            out << ",";
        out << "\n";
    }
    out << "};\n\n";

    vector<int> stmtSize;
    vector<int> branchSize;
    vector<int> condSize;
    vector<int> mcdcSize;
    vector<int> mcdcOffsets;
    vector<int> mcdcOffsetsSize;

    // Walk through the .vinf files to build database
    for (int i = 0; i < fileList.size(); ++i)
    {
        QString dirName;
        QString fileName;
        getInstrFilename(fileList[i], dirName, fileName);
        int j = fileName.lastIndexOf(".");
        if (j == -1)
        {
            QMessageBox::critical(0, QString("Error"), QString("Illegal filename %1")
                                  .arg(fileList[i]));
            return;
        }
        QString infName = fileName.left(j) + ".vinf";
        QFile infFile(infName);
        if (!infFile.open(QFile::ReadOnly | QFile::Text))
        {
            QMessageBox::critical(this, tr("Error"), tr("Cannot open file %1:\n%2")
                                 .arg(infName)
                                 .arg(infFile.errorString()));
            return;
        }

        QTextStream inf(&infFile);

        // Read file into a string
        QString jsonSource = inf.readAll();

        infFile.close();

        // Now parse it
        ostringstream jsonErr;
        Json json(&jsonErr);
        try
        {
          json.ParseJson(jsonSource.toStdString());
        }
        catch(int i)
        {
            QMessageBox::warning(0, QString("Error"), QString("Bad .vinf file: %1\n%2").arg(infName).arg(QString::fromStdString(jsonErr.str())));
            return;
        }

        // Parse .vinf file
        vector<JsonNode>::iterator node = json.jsonNodes.begin();
        int cnt = 0;

        int val;

        while (node != json.jsonNodes.end())
        {
            // get the node name and value as a string
            string nodeName = node->name();

            if (nodeName == "stmt_size")
            {
                val = node->as_int();
                stmtSize.push_back(val);
            }
            else
            if (nodeName == "branch_size")
            {
                val = node->as_int();
                branchSize.push_back(val);
            }
            else
            if (nodeName == "cond_size")
            {
                val = node->as_int();
                condSize.push_back(val);
            }
            else
            if (nodeName == "mcdc_size")
            {
                // FIXME - mcdc_size not needed! (sum of mcdc_offsets[] array)
                val = node->as_int();
                mcdcSize.push_back(val);
            }
            else
            if (nodeName == "mcdc_offsets")
            {
                vector<JsonNode> offsets;
                json.ParseArray(node->as_string(), offsets);

                // Load include paths from JSON config file
                vector<JsonNode>::const_iterator offNode = offsets.begin();

                while (offNode != offsets.end())
                {
                    val = atoi(offNode->nodeData.c_str());
                    mcdcOffsets.push_back(val);
                    ++offNode;
                }

                mcdcOffsetsSize.push_back(offsets.size());
            }
            else
            {
                QMessageBox::warning(0, QString("Warning"), QString("Unknown .vinf node: %1")
                                     .arg(QString::fromStdString(nodeName)));
            }

            ++node;
        }
    }

    int stmtOffset = 0;
    int branchOffset = 0;
    int condOffset = 0;
    int mcdcOffset = 0;

    int stmtEol = 1;
    int branchEol = 1;
    int condEol = 1;
    int mcdcEol = 1;

    ostringstream stmtStr;
    ostringstream branchStr;
    ostringstream condStr;
    ostringstream mcdcStr;

    for (int i = 0; i < fileList.size(); ++i)
    {
        if (vcData.doStmtSingle || vcData.doStmtCount)
        {
            stmtStr << " " << stmtOffset << ",";
            stmtOffset += stmtSize[i];
            if ((stmtStr.str().length() - stmtEol) > 72)
            {
                stmtStr << "\n ";
                stmtEol = stmtStr.str().length();
            }
        }

        if (vcData.doBranch)
        {
           branchStr << " " << branchOffset << ",";
           branchOffset += branchSize[i];
           if ((branchStr.str().length() - branchEol) > 72)
           {
               branchStr << "\n ";
               branchEol = branchStr.str().length();
           }
        }

        if (vcData.doMCDC)
        {
#ifdef NEED_MCDC_SIZE
            mcdcStr << " " << mcdcOffset << ",";
#endif
            mcdcOffset += mcdcSize[i];
#ifdef NEED_MCDC_SIZE
            if ((mcdcStr.str().length() - mcdcEol) > 72)
            {
                mcdcStr << "\n ";
                mcdcEol = mcdcStr.str().length();
            }
#endif
        }

        if (vcData.doCC)
        {
            condStr << " " << condOffset << ",";
            condOffset += condSize[i];
            if ((condStr.str().length() - condEol) > 72)
            {
                condStr << "\n ";
                condEol = condStr.str().length();
            }
        }
    }

    stmtStr << " " << stmtOffset;
    branchStr << " " << branchOffset;
    mcdcStr << " " << mcdcOffset;
    condStr << " " << condOffset;

    if (stmtSize.size() > 0)
    {
        QString type = getMinType(stmtOffset);
        hOut << "extern unsigned char _vamp_stmt_array[" << stmtOffset << "];\n";
        out << "unsigned char _vamp_stmt_array[" << stmtOffset << "];\n";

        hOut << "extern " << type << " _vamp_stmt_index[" << fileList.size() + 1 << "];\n";
        out << type << " _vamp_stmt_index[" << fileList.size() + 1 << "] = {\n ";
        out << QString::fromStdString(stmtStr.str()) << "\n};\n";
    }

    if (branchSize.size() > 0)
    {
        QString type = getMinType(branchOffset);
        hOut << "extern unsigned char _vamp_branch_array[" << branchOffset << "];\n";
        out << "unsigned char _vamp_branch_array[" << branchOffset << "];\n";

        hOut << "extern " << type << " _vamp_branch_index[" << fileList.size() + 1 << "];\n";
        out << type << " _vamp_branch_index[" << fileList.size() + 1 << "] = {\n ";
        out << QString::fromStdString(branchStr.str()) << "\n};\n";
    }

    if (condSize.size() > 0)
    {
        QString type = getMinType(condOffset);
        hOut << "extern unsigned char _vamp_cond_array[" << condOffset << "];\n";
        out << "unsigned char _vamp_cond_array[" << condOffset << "];\n";

        hOut << "extern " << type << " _vamp_cond_index[" << fileList.size() + 1 << "];\n";
        out << type << " _vamp_cond_index[" << fileList.size() + 1 << "] = {\n ";
        out << QString::fromStdString(condStr.str()) << "\n};\n";
    }

    if (mcdcSize.size() > 0)
    {
        //hOut << "#define _vamp_mcdc_set_first(index, val, bit) _vamp_mcdc_set_first_func(index, (val) != 0, bit)\n";
        hOut << "extern unsigned char _vamp_mcdc_val_save[" << mcdcOffset << "];\n";
        //hOut << "extern unsigned int _vamp_mcdc_stack_overflow[" << fileList.size() << "];\n";
        hOut << "extern unsigned char _vamp_mcdc_set_first(unsigned int index, unsigned char val, unsigned char bit);\n";
        hOut << "extern unsigned char _vamp_mcdc_collect(unsigned int index, unsigned char result, unsigned short exprNum, unsigned char byteCnt);\n";
        out << "unsigned char _vamp_mcdc_val_save[" << mcdcOffset << "];\n";
        hOut << "extern unsigned int _vamp_mcdc_val[" << fileList.size() << "];\n";
        out << "unsigned int _vamp_mcdc_val[" << fileList.size() << "];\n";
        out << "static unsigned int _vamp_mcdc_stack[" << fileList.size() * 4 << "];\n";
        out << "static unsigned char _vamp_mcdc_stack_offset[" << fileList.size() << "];\n";
        out << "static unsigned int _vamp_mcdc_stack_overflow[" << fileList.size() << "];\n";
#ifdef NEED_MCDC_SIZE
        QString type = getMinType(mcdcOffset);
        //hOut << "extern " << type << " _vamp_mcdc_index[" << fileList.size() + 1 << "];\n";
        out << type << " _vamp_mcdc_index[" << fileList.size() + 1 << "] = {\n ";
        out << QString::fromStdString(mcdcStr.str()) << "\n};\n";
#endif
    }

    if (mcdcOffsets.size() > 0)
    {
        ostringstream mcdcOut;
        int cnt = 0;
        int off = 1;
        for (int i = 0; i < fileList.size(); ++i)
        {
            mcdcOut << " " << cnt << ",";
            cnt += mcdcOffsetsSize[i];
            if ((mcdcOut.str().length() - off) > 72)
            {
                mcdcOut << "\n ";
                off = mcdcOut.str().length();
            }
        }
        mcdcOut << "  " << cnt;

        // At this point mcdcOffset == cnt + 1
        //cout << "Check: " << mcdcOffset << " == " << cnt + 1 << "\n";

        QString type = getMinType(cnt);
        //hOut << "extern " << type << " _vamp_mcdc_val_offset_index[" << fileList.size() + 1 << "];\n";
        out << type << " _vamp_mcdc_val_offset_index[" << fileList.size() + 1 << "] = {\n  ";
        out << QString::fromStdString(mcdcOut.str()) << "\n};\n";
        cnt = 0;
        //hOut << "extern " << type << " _vamp_mcdc_val_offset[" << mcdcOffsets.size() + 1 << "];\n";
        out << type << " _vamp_mcdc_val_offset[" << mcdcOffsets.size() + 1 << "] = {\n ";

        ostringstream offsets;
        off = 1;
        for (int i = 0; i < mcdcOffsets.size(); ++i)
        {
            offsets << " " << cnt << ",";
            cnt += mcdcOffsets[i];
            if ((offsets.str().length() - off) > 72)
            {
                offsets << "\n ";
                off = offsets.str().length();
            }
        }
        offsets << " " << cnt;

        out << QString::fromStdString(offsets.str()) << "\n};\n";
        out << "\nunsigned char _vamp_mcdc_set_first(unsigned int index, unsigned char val, unsigned char bit)\n";
        out << "{\n  if (_vamp_mcdc_stack_offset[index] < " << vcData.mcdcStackSize << ")\n  {\n";
        out << "    _vamp_mcdc_stack[index * " << vcData.mcdcStackSize << " + _vamp_mcdc_stack_offset[index]++] = _vamp_mcdc_val[index];\n";
        out << "    _vamp_mcdc_val[index] = 1;\n";
        out << "    if (val)\n";
        out << "      _vamp_mcdc_val[index] |= (2 << bit);\n  }\n";
        out << "  else\n  {\n";
        out << "    _vamp_mcdc_stack_overflow[index] = 1;\n  }\n";
        out << "  return val;\n}\n";
        out << "\nunsigned char _vamp_mcdc_collect(unsigned int index, unsigned char result, unsigned short exprNum, unsigned char byteCnt)\n";
        out << "{\n";
        out << "  unsigned int testval;\n";
        out << "  int i, j;\n";
        out << "  if (_vamp_mcdc_stack_overflow[index] == 0)\n";
        out << "  {\n    i = _vamp_mcdc_val_offset[_vamp_mcdc_val_offset_index[index] + exprNum];\n";
        out << "    do {\n      testval = 0;\n";
        out << "      for (j = byteCnt - 1; j >= 0; j--)\n";
        out << "      {\n        testval = (testval << 8) | _vamp_mcdc_val_save[i + j];\n";
        out << "      }\n      i += byteCnt;\n";
        out << "    } while ((testval != 0) && (testval != _vamp_mcdc_val[index]));\n";
        out << "    if (testval == 0)\n";
        out << "    {\n      i -= byteCnt;\n";
        out << "      for (j = 0; j < byteCnt; j++)\n";
        out << "      {\n";
        out << "        _vamp_mcdc_val_save[i++] = _vamp_mcdc_val[index] & 0xff;\n";
        out << "        _vamp_mcdc_val[index] >>= 8;\n";
        out << "      }\n    }\n";
        out << "    if (_vamp_mcdc_stack_offset[index])\n";
        out << "    {\n";
        out << "      _vamp_mcdc_val[index] = _vamp_mcdc_stack[--_vamp_mcdc_stack_offset[index]];\n";
        out << "    }\n";
        out << "  }\n  else\n  {\n";
        out << "    _vamp_mcdc_stack_overflow[index] = exprNum + 1;\n";
        out << "  }\n\n  return result;\n";
        out << "}\n\n";
    }

    out << "void _vamp_output()\n{\n";
    out << "  int i, j, cnt;\n\n";
    out << "  for (i = 0; i < " << fileList.size() << "; ++i)\n";
    out << "  {\n";
    out << "    for (j = 0; _vamp_filenames[i][j]; ++j)\n";
    out << "      _vamp_send(_vamp_filenames[i][j]);\n";
    out << "    _vamp_send(0);\n\n";

    // Insert instrumentation data output routine
    int opts = (vcData.doStmtSingle ? DO_STATEMENT_SINGLE : 0) |
                         (vcData.doStmtCount ? DO_STATEMENT_COUNT : 0) |
                         (vcData.doBranch ? DO_BRANCH : 0) |
                         (vcData.doMCDC ? DO_MCDC : 0) |
                         (vcData.doCC ? DO_CONDITION : 0);
    //char optsStr[8];
    //sprintf(optsStr, "%#02x", opts);
    out << "    _vamp_send(" << opts << ");\n\n";
    if (vcData.doStmtSingle || vcData.doBranch)
    {
        out << "    cnt = _vamp_stmt_index[i + 1] - _vamp_stmt_index[i];\n";
        // Output instrumented statement data
        out << "    _vamp_send(cnt >> 8);\n";
        out << "    _vamp_send(cnt & 0xff);\n";
        out << "    for (j = _vamp_stmt_index[i]; j < _vamp_stmt_index[i + 1]; ++j)\n";
        out << "      _vamp_send(_vamp_stmt_array[j]);\n";
    }
    else
    // FIXME: Is " || vcData.doBranch" needed here?
    if (vcData.doStmtCount)
    {
        out << "    cnt = _vamp_stmt_index[i + 1] - _vamp_stmt_index[i];\n";
        // Output instrumented statement data
        out << "    _vamp_send(cnt >> 8);\n";
        out << "    _vamp_send(cnt & 0xff);\n";
        out << "    for (j = _vamp_stmt_index[i]; j < _vamp_stmt_index[i + 1]; ++j)\n";
        out << "    {\n";
        out << "      _vamp_send(_vamp_stmt_array[j] >> 24);\n";
        out << "      _vamp_send(_vamp_stmt_array[j] >> 16) & 0xff);\n";
        out << "      _vamp_send(_vamp_stmt_array[j] >> 8) & 0xff);\n";
        out << "      _vamp_send(_vamp_stmt_array[j] & 0xff);\n";
        out << "    }\n";
    }

    if (vcData.doBranch)
    {
        out << "    cnt = _vamp_branch_index[i + 1] - _vamp_branch_index[i];\n";
        // Output instrumented branch data
        out << "    _vamp_send(cnt >> 8);\n";
        out << "    _vamp_send(cnt & 0xff);\n";
        out << "    for (j = _vamp_branch_index[i]; j < _vamp_branch_index[i + 1]; ++j)\n";
        out << "      _vamp_send(_vamp_branch_array[j]);\n";
    }

    if (vcData.doCC)
    {
        out << "    cnt = _vamp_cond_index[i + 1] - _vamp_cond_index[i];\n";
        // Output instrumented condition data
        out << "    _vamp_send(cnt >> 8);\n";
        out << "    _vamp_send(cnt & 0xff);\n";
        out << "    for (j = _vamp_cond_index[i]; j < _vamp_cond_index[i + 1]; ++j)\n";
        out << "      _vamp_send(_vamp_cond_array[j]);\n";
    }

    if (vcData.doMCDC)
    {
//          out << "    cnt = _vamp_mcdc_index[i + 1] - _vamp_mcdc_index[i];\n";
        out << "    cnt = _vamp_mcdc_val_offset[_vamp_mcdc_val_offset_index[i + 1]] - _vamp_mcdc_val_offset[_vamp_mcdc_val_offset_index[i]];\n";
      out << "    _vamp_send(cnt >> 8);\n";
      out << "    _vamp_send(cnt & 0xff);\n";

      out << "    for (j = 0; j < cnt; ++j)\n";
      out << "      _vamp_send(_vamp_mcdc_val_save[_vamp_mcdc_val_offset[_vamp_mcdc_val_offset_index[i]] + j]);\n";

      // Output stack overflow status
      out << "    _vamp_send((_vamp_mcdc_stack_overflow[i] >> 8) & 0xff);\n";
      out << "    _vamp_send(_vamp_mcdc_stack_overflow[i] & 0xff);\n";
    }
    out << "  }\n";

#endif

    out << "}\n";
    out.flush();
    voFile.close();

    hOut.flush();
    vHoFile.close();
}

// Help->About called
void MainWindow::on_action_About_triggered()
{
    QMessageBox::about(this, tr("About Vamp"),
             tr("<center><h1>Vamp Version 2.10</h1><nb>"
                "<h3>Copyright 2016, Robert Ankeney</h3><nb>"
                "<h3>Licensed Under LGPLv3</h3><nb>"
                "<h3>See LICENSE.txt for Details</h3></center>"));
}

void MainWindow::on_forwardPushButton_clicked()
{
    QString findString = ui->findLineEdit->text();
    QPalette palette = ui->findLineEdit->palette();
    QWebPage::FindFlags flags = 0;
    if (ui->caseSensitiveCheckBox->isChecked())
        flags = QWebPage::FindCaseSensitively;

    if (ui->webView->findText(findString, flags))
    {
        palette.setColor(QPalette::Base, QColor(255, 255, 255)); // White
        ui->findLineEdit->setPalette(palette);
    }
    else
    {
        palette.setColor(QPalette::Base, QColor(255, 128, 128)); // Pink
        ui->findLineEdit->setPalette(palette);
    }
}

void MainWindow::on_backPushButton_clicked()
{
    QString findString = ui->findLineEdit->text();
    QPalette palette = ui->findLineEdit->palette();
    QWebPage::FindFlags flags = QWebPage::FindBackward;
    if (ui->caseSensitiveCheckBox->isChecked())
        flags |= QWebPage::FindCaseSensitively;

    if (ui->webView->findText(findString, flags))
    {
        palette.setColor(QPalette::Base, QColor(255, 255, 255)); // White
        ui->findLineEdit->setPalette(palette);
    }
    else
    {
        palette.setColor(QPalette::Base, QColor(255, 128, 128)); // Pink
        ui->findLineEdit->setPalette(palette);
    }
}

void MainWindow::on_findLineEdit_returnPressed()
{
    on_forwardPushButton_clicked();
}

void MainWindow::on_webView_linkClicked(const QUrl &arg1)
{
    QString url = arg1.toString();
    if (url == "New Project")
        on_actionNew_Project_triggered();
    else
    if (url == "Open Project")
        on_actionOpen_Project_triggered();
}
