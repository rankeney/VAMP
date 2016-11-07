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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "configfile.h"
#include "projectmodel.h"
#include "fileinstinfo.h"
#include "path.h"
#include <QMainWindow>
#include <QHBoxLayout>
#include <QProcess>
#include <QPlainTextEdit>
#include <QScrollBar>

#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
private:
    bool isAbsolutePath(string path);
    QString fixPath(QString qpath, bool addSlash);
    QString fixPath(string path, bool addSlash);
    QString getMinType(int val);
    void closeEvent(QCloseEvent *event);
    void setDefaultVcData(VAMP_CONFIG &vcData);
    void setDefaultVcReportData(VAMP_REPORT_CONFIG &vcReportData);
    void setDefaultVcPreProcData(VAMP_PREPROC_CONFIG &vcPreProcData);
    void saveVcData(void);
    void saveVcReportData(void);
    void saveProjectData(void);
    void loadProjectData(QString fileName);
    void getDirFileNames(QString pathName, QString &dirName, QString &fileName);
    void loadFileInfo(QString fileName, fileInstInfo &fileInfo);
    void getFileStats(fileInstInfo &fileInfo, QStringList &stats);
    void insertRow(QStringList &list);
    void insertChild(QStringList nameList);
    void insertPath(QString pathName, QStringList fileInfo);
    void getCommonPath(QString curPath, QModelIndex parent, QString &path);
    void getBasePath();
    void genFileList(QString curPath, QModelIndex parent, QStringList &results);
    void genSummary(QString &outStr, int level, QModelIndex parent);
    void editConfig(void);
    void checkProject(QString fileName, bool isNew);
    bool saveProject(bool showDialog);
    void createContextActions();
    void addPath(string pathToAdd, QString &path);
    void getFullFileName(QModelIndex index, QString &fileName);
    bool getPreProcFilename(QString fileName, QString &pathName, QString &preProcFileName);
    void getInstrPath(QString &instrPath);
    bool getInstrFilename(QString fileName, QString &outPath, QString &instrFileName);
    bool getInstrFilenameFromIndex(QModelIndex index, QString &instrPathName, QString &instrFileName);
    bool getHtmlFilename(QString fileName, QString &outPath, QString &htmlFileName);
    bool mapPreprocessedSource(QString &source, QString fileName, QString &preProcFileName);
    bool preprocessFile(QString fileName, bool force);
    bool instrumentFile(QString fileName);
    bool processFile(QString fileName);
    void showHtmlFile(QString fileName, bool isHtml);
    void QPlainTextEditAppendText(QPlainTextEdit* editor, QString text, QColor color = Qt::black);
    void getFileIndex(QModelIndex &index, QModelIndex &child);
    void appendAnsiText(QPlainTextEdit *textEdit, QString str);
    void appendOutput(QString str);
    void appendError(QString str);
    void addFiles(QString startDir);

private slots:
    void on_actionE_xit_triggered();

    void on_action_Add_triggered();

    void on_projectTreeView_doubleClicked(const QModelIndex &index);

    void on_projectTreeView_customContextMenuRequested(const QPoint &pos);

    void on_actionNew_Project_triggered();

    void on_actionOpen_Project_triggered();

    void on_actionSave_Project_triggered();

    void on_actionEdit_Configuration_triggered();

    void on_action_Close_Project_triggered();

    void on_action_Generate_Summary_triggered();

    void on_action_Print_Summary_triggered();

    void on_action_Generate_vamp_output_c_triggered();

    void on_action_About_triggered();

    void on_forwardPushButton_clicked();

    void on_backPushButton_clicked();

    void on_findLineEdit_returnPressed();

    void on_webView_linkClicked(const QUrl &arg1);

public slots:
    void removeItem();
    void preprocessItem();
    void instrumentItem();
    void processItem();
    void addFilesToFolder();
    void showItem();
    void showResults();
    void showPreProcItem();
    void showInstrItem();
    void printOutput();
    void printError();
    void clearOutput();
    void clearError();
    void saveLogAs(QPlainTextEdit *edit);
    void saveOutputAs();
    void saveErrorAs();
    void showOutputContextMenu(const QPoint &pt);
    void showErrorContextMenu(const QPoint &pt);

private:
    Ui::MainWindow *ui;
    VAMP_CONFIG vcData;
    VAMP_REPORT_CONFIG vcReportData;
    VAMP_PREPROC_CONFIG vcPreProcData;
    QString projectBaseDir;
    //ConfigFile vampConfig;
    QString projectDirName;     // Full path to where .vproj project is kept
    QString projectPath;        // Full path and filename of .vproj file
    QString projectName;        // Name of .vproj file
    QString baseDir;            // Path to project or base of where project begins
    ProjectModel *projectModel;
    bool projectChanged;
    bool vcDataChanged;
    bool vcReportDataChanged;
    bool vcPreProcDataChanged;
    QAction *removeFileAct;
    QAction *removeFolderAct;
    QAction *preprocessFileAct;
    QAction *preprocessFolderAct;
    QAction *instrumentFileAct;
    QAction *instrumentFolderAct;
    QAction *processResultsAct;
    QAction *showResultsAct;
    QAction *processFolderAct;
    QAction *addFilesAct;
    QAction *showFileAct;
    QAction *showPreProcFileAct;
    QAction *showInstrFileAct;

    QAction *clearOutputAct;
    QAction *clearErrorAct;
    QAction *saveOutputAsAct;
    QAction *saveErrorAsAct;

    QProcess *myProcess;

    Path path;
};

#endif // MAINWINDOW_H
