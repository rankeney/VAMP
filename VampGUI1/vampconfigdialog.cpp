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

#include "vampconfigdialog.h"
#include "ui_vampconfigdialog.h"
#include <QFileDialog>

vampConfigDialog::vampConfigDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::vampConfigDialog)
{
    ui->setupUi(this);
}

vampConfigDialog::~vampConfigDialog()
{
    delete ui;
}

void vampConfigDialog::setProjectDirectory(QString projectDirName)
{
    projectDirectory = projectDirName;
}

void vampConfigDialog::getVcData(VAMP_CONFIG &vcData, bool &vcDataChanged)
{
    vcDataChanged = (vc.doStmtSingle != vcData.doStmtSingle) ||
                    (vc.doStmtCount != vcData.doStmtCount) ||
                    (vc.doBranch != vcData.doBranch) ||
                    (vc.doMCDC != vcData.doMCDC) ||
                    (vc.doCC != vcData.doCC) ||
                    (vc.saveDirectory != vcData.saveDirectory) ||
                    (vc.saveSuffix != vcData.saveSuffix) ||
                    (vc.mcdcStackSize != vcData.mcdcStackSize) ||
                    (vc.langStandard != vcData.langStandard);

    if (vcDataChanged)
        vcData = vc;
}

void vampConfigDialog::getVcReportData(VAMP_REPORT_CONFIG &vcReportData, bool &vcReportDataChanged)
{
    vcReportDataChanged = (vcReportData.histDirectory != vcReport.histDirectory) ||
                          (vcReportData.combineHistory != vcReport.combineHistory) ||
                          (vcReportData.generateReport != vcReport.generateReport) ||
                          (vcReportData.showTestCases != vcReport.showTestCases) ||
                          (vcReportData.reportSeparator != vcReport.reportSeparator) ||
                          (vcReportData.htmlDirectory != vcReport.htmlDirectory) ||
                          (vcReportData.htmlSuffix != vcReport.htmlSuffix);

    if (vcReportDataChanged)
       vcReportData = vcReport;
}

void vampConfigDialog::getVcPreProcData(VAMP_PREPROC_CONFIG &vcPreProcData, bool &vcPreProcDataChanged)
{
    vcPreProcDataChanged = (vcPreProcData.doPreProcess != vcPreProc.doPreProcess) ||
                           (vcPreProcData.useVampPreprocessor != vcPreProc.useVampPreprocessor) ||
                           (vcPreProcData.retainComments != vcPreProc.retainComments) ||
                           (vcPreProcData.showLineMarkers != vcPreProc.showLineMarkers) ||
                           (vcPreProcData.preProcPath != vcPreProc.preProcPath) ||
                           (vcPreProcData.preProcArguments != vcPreProc.preProcArguments) ||
                           (vcPreProcData.preProcDirectory != vcPreProc.preProcDirectory) ||
                           (vcPreProcData.preProcSuffix != vcPreProc.preProcSuffix) ||
                           (vcPreProcData.includePaths != vcPreProc.includePaths) ||
                           (vcPreProcData.definesList != vcPreProc.definesList);
/* For long comparison of vectors is not needed
                           (vcPreProcData.definesList.size() != vcPreProc.definesList.size());

    if (!vcPreProcDataChanged)
    {
        for (int i = 0; i < vcPreProc.includePaths.size(); ++i)
        {
            if (vcPreProcData.includePaths[i] != vcPreProc.includePaths[i])
                vcPreProcDataChanged = true;
        }
    }

    if (!vcPreProcDataChanged)
    {
        for (int i = 0; i < vcPreProc.definesList.size(); ++i)
        {
            if (vcPreProcData.definesList[i] != vcPreProc.definesList[i])
                vcPreProcDataChanged = true;
        }
    }
*/
    if (vcPreProcDataChanged)
        vcPreProcData = vcPreProc;
}

void vampConfigDialog::setVcData(VAMP_CONFIG &vcData)
{
    // vamp.cfg data
    ui->singleStmtCoverage->setChecked(vcData.doStmtSingle);
    ui->countStmtCoverage->setChecked(vcData.doStmtCount);
    ui->mcdcCoverage->setChecked(vcData.doMCDC);
    ui->conditionCoverage->setChecked(vcData.doCC);
    ui->branchCoverage->setChecked(vcData.doBranch);
    ui->saveDirectory->setText(QString::fromStdString(vcData.saveDirectory));
    ui->saveSuffix->setText(QString::fromStdString(vcData.saveSuffix));
    ui->mcdcStackSize->setText(QString::number(vcData.mcdcStackSize));
    if (vcData.langStandard == clang::LangStandard::lang_c89)
    {
        ui->C1990_RadioButton->setChecked(true);
        ui->gnuExt_checkBox->setChecked(false);
    }
    else
    if (vcData.langStandard == clang::LangStandard::lang_gnu89)
    {
        ui->C1990_RadioButton->setChecked(true);
        ui->gnuExt_checkBox->setChecked(true);
    }
    else
    if (vcData.langStandard == clang::LangStandard::lang_c99)
    {
        ui->C1999_RadioButton->setChecked(true);
        ui->gnuExt_checkBox->setChecked(false);
    }
    else
    if (vcData.langStandard == clang::LangStandard::lang_gnu99)
    {
        ui->C1999_RadioButton->setChecked(true);
        ui->gnuExt_checkBox->setChecked(true);
    }
    if (vcData.langStandard == clang::LangStandard::lang_c11)
    {
        ui->C2011_RadioButton->setChecked(true);
        ui->gnuExt_checkBox->setChecked(false);
    }
    if (vcData.langStandard == clang::LangStandard::lang_gnu11)
    {
        ui->C2011_RadioButton->setChecked(true);
        ui->gnuExt_checkBox->setChecked(true);
    }
}

void vampConfigDialog::setVcReportData(VAMP_REPORT_CONFIG &vcReportData)
{
    // vamp_process.cfg data
    ui->histDirectory->setText(QString::fromStdString(vcReportData.histDirectory));
    ui->combineHistory->setChecked(vcReportData.combineHistory);
    ui->showTestCases->setChecked(vcReportData.showTestCases);
    ui->generateReport->setChecked(vcReportData.generateReport);
    ui->reportSeparator->setText(QString::fromStdString(vcReportData.reportSeparator));
    ui->htmlDirectory->setText(QString::fromStdString(vcReportData.htmlDirectory));
    ui->htmlSuffix->setText(QString::fromStdString(vcReportData.htmlSuffix));
}

void vampConfigDialog::setVcPreProcData(VAMP_PREPROC_CONFIG &vcPreProcData)
{
    ui->preProcCheckBox->setChecked(vcPreProcData.doPreProcess);
    ui->useVampPreprocCheckBox->setChecked(vcPreProcData.useVampPreprocessor);
    ui->retainCommentsCheckBox->setChecked(vcPreProcData.retainComments);
    ui->buildPPMapCheckBox->setChecked(vcPreProcData.showLineMarkers);
    ui->preProcPathEdit->setText(QString::fromStdString(vcPreProcData.preProcPath));
    ui->preProcArgsEdit->setText(QString::fromStdString(vcPreProcData.preProcArguments));
    ui->preProcDirectory->setText(QString::fromStdString(vcPreProcData.preProcDirectory));
    ui->preProcSuffix->setText(QString::fromStdString(vcPreProcData.preProcSuffix));

    ui->addIncludeTextEdit->clear();
    for (int i = 0; i < vcPreProcData.includePaths.size(); i++)
    {
        ui->addIncludeTextEdit->insertPlainText(QString::fromStdString(vcPreProcData.includePaths[i]) + "\n");
    }

    ui->definesTextEdit->clear();
    for (int i = 0; i < vcPreProcData.definesList.size(); i++)
    {
        ui->definesTextEdit->insertPlainText(QString::fromStdString(vcPreProcData.definesList[i]) + "\n");
    }

    on_preProcCheckBox_stateChanged(vcPreProcData.doPreProcess);
    if (vcPreProcData.doPreProcess)
       on_useVampPreprocCheckBox_stateChanged(vcPreProcData.useVampPreprocessor);
}

void vampConfigDialog::on_singleStmtCoverage_stateChanged(int arg1)
{
    if (arg1)
        ui->countStmtCoverage->setChecked(false);
}

void vampConfigDialog::on_countStmtCoverage_stateChanged(int arg1)
{
    if (arg1)
        ui->singleStmtCoverage->setChecked(false);
}

void vampConfigDialog::on_mcdcCoverage_stateChanged(int arg1)
{
    if (arg1)
        ui->conditionCoverage->setChecked(false);
}

void vampConfigDialog::on_conditionCoverage_stateChanged(int arg1)
{
    if (arg1)
        ui->mcdcCoverage->setChecked(false);
}

void vampConfigDialog::on_preProcCheckBox_stateChanged(int arg1)
{
    if (arg1)
    {
        ui->useVampPreprocCheckBox->show();
        ui->preprocOptionsTabWidget->show();

        ui->preProcDirLabel->show();
        ui->preProcDirectory->show();
        ui->preProcSuffixLabel->show();
        ui->preProcSuffix->show();
        ui->preProcDirButton->show();
        on_useVampPreprocCheckBox_stateChanged(ui->useVampPreprocCheckBox->isChecked());
    }
    else
    {
        ui->useVampPreprocCheckBox->hide();
        ui->preprocOptionsTabWidget->hide();
        ui->preProcPathEdit->hide();
        ui->preProcArgsEdit->hide();
        ui->pathToPreProcLabel->hide();
        ui->preProcArgsLabel->hide();
        ui->retainCommentsCheckBox->hide();
        ui->buildPPMapCheckBox->hide();

        ui->preProcDirLabel->hide();
        ui->preProcDirectory->hide();
        ui->preProcSuffixLabel->hide();
        ui->preProcSuffix->hide();
        ui->preProcDirButton->hide();
    }
}

void vampConfigDialog::on_useVampPreprocCheckBox_stateChanged(int arg1)
{
    if (arg1)
    {
        ui->preProcPathEdit->hide();
        ui->preProcArgsEdit->hide();
        ui->pathToPreProcLabel->hide();
        ui->preProcArgsLabel->hide();
        ui->retainCommentsCheckBox->show();
        ui->buildPPMapCheckBox->show();
    }
    else
    {
        ui->preProcPathEdit->show();
        ui->preProcArgsEdit->show();
        ui->pathToPreProcLabel->show();
        ui->preProcArgsLabel->show();
        //ui->retainCommentsCheckBox->setChecked(false);
        ui->retainCommentsCheckBox->hide();
//        ui->buildPPMapCheckBox->hide();
    }
}

void vampConfigDialog::on_buttonBox_accepted()
{
    // vamp.cfg data
    vc.doBranch = ui->branchCoverage->isChecked();
    vc.doCC = ui->conditionCoverage->isChecked();
    vc.doMCDC = ui->mcdcCoverage->isChecked();
    vc.doStmtCount = ui->countStmtCoverage->isChecked();
    vc.doStmtSingle = ui->singleStmtCoverage->isChecked();
    vc.mcdcStackSize = ui->mcdcStackSize->text().toInt();
    vc.saveDirectory = ui->saveDirectory->text().toStdString();
    vc.saveSuffix = ui->saveSuffix->text().toStdString();
    if (ui->C1990_RadioButton->isChecked() && !ui->gnuExt_checkBox->isChecked())
        vc.langStandard = clang::LangStandard::lang_c89;
    else
    if (ui->C1990_RadioButton->isChecked() && ui->gnuExt_checkBox->isChecked())
        vc.langStandard = clang::LangStandard::lang_gnu89;
    else
    if (ui->C1999_RadioButton->isChecked() && !ui->gnuExt_checkBox->isChecked())
        vc.langStandard = clang::LangStandard::lang_c99;
    else
    if (ui->C1999_RadioButton->isChecked() && ui->gnuExt_checkBox->isChecked())
        vc.langStandard = clang::LangStandard::lang_gnu99;
    else
    if (ui->C2011_RadioButton->isChecked() && !ui->gnuExt_checkBox->isChecked())
        vc.langStandard = clang::LangStandard::lang_c11;
    else
    if (ui->C2011_RadioButton->isChecked() && ui->gnuExt_checkBox->isChecked())
        vc.langStandard = clang::LangStandard::lang_gnu11;

    // vamp_process.cfg data
    vcReport.histDirectory = ui->histDirectory->text().toStdString();
    vcReport.combineHistory = ui->combineHistory->isChecked();
    vcReport.showTestCases = ui->showTestCases->isChecked();
    vcReport.generateReport = ui->generateReport->isChecked();
    vcReport.reportSeparator = ui->reportSeparator->text().toStdString();
    vcReport.htmlDirectory = ui->htmlDirectory->text().toStdString();
    vcReport.htmlSuffix = ui->htmlSuffix->text().toStdString();

    // Preprocessor data
    vcPreProc.doPreProcess = ui->preProcCheckBox->isChecked();
    vcPreProc.useVampPreprocessor = ui->useVampPreprocCheckBox->isChecked();
    vcPreProc.retainComments = ui->retainCommentsCheckBox->isChecked();
    vcPreProc.showLineMarkers = ui->buildPPMapCheckBox->isChecked();
    vcPreProc.preProcPath = ui->preProcPathEdit->text().toStdString();
    vcPreProc.preProcArguments = ui->preProcArgsEdit->text().toStdString();
    vcPreProc.preProcDirectory = ui->preProcDirectory->text().toStdString();
    vcPreProc.preProcSuffix = ui->preProcSuffix->text().toStdString();

    //QTextCursor foo()
    QString incDirs = ui->addIncludeTextEdit->toPlainText();
/*    vcPreProc.includePaths = incDirs.split("\n");
    for (int i = 0; i < vcPreProc.includePaths.size(); ++i)
    {
        if (vcPreProc.includePaths[i] == "")
            vcPreProc.includePaths.removeAt(i--);
    }
*/
    QStringList incPaths = incDirs.split("\n");
    for (int i = 0; i < incPaths.size(); ++i)
    {
        if (incPaths[i] != "")
            vcPreProc.includePaths.push_back(incPaths[i].toStdString());
    }

    QString defines = ui->definesTextEdit->toPlainText();
/*    vcPreProc.definesList = defines.split("\n");
    for (int i = 0; i < vcPreProc.definesList.size(); ++i)
    {
        if (vcPreProc.definesList[i] == "")
            vcPreProc.definesList.removeAt(i--);
    }*/
    QStringList definesList = defines.split("\n");
    for (int i = 0; i < definesList.size(); ++i)
    {
        if (definesList[i] != "")
            vcPreProc.definesList.push_back(definesList[i].toStdString());
    }
}

void vampConfigDialog::on_addIncludeButton_clicked()
{
    QString dirName = QFileDialog::getExistingDirectory(this, tr("Add Directory"), projectDirectory);
    if (!dirName.isEmpty())
    {
        // Move to start of line before inserting directory name
        QTextCursor qc = ui->addIncludeTextEdit->textCursor();
        qc.movePosition(QTextCursor::StartOfLine);
        ui->addIncludeTextEdit->setTextCursor(qc);
        ui->addIncludeTextEdit->insertPlainText(dirName + "\n");
    }
}

void vampConfigDialog::on_htmlDirButton_clicked()
{
    QString dirName = QFileDialog::getExistingDirectory(this, tr("Select HTML Directory"), projectDirectory);
    if (!dirName.isEmpty())
    {
        ui->htmlDirectory->setText(dirName);
    }
}

void vampConfigDialog::on_preProcDirButton_clicked()
{
    QString dirName = QFileDialog::getExistingDirectory(this, tr("Select Preprocessed File Directory"), projectDirectory);
    if (!dirName.isEmpty())
    {
        ui->preProcDirectory->setText(dirName);
    }
}

void vampConfigDialog::on_saveDirButton_clicked()
{
    QString dirName = QFileDialog::getExistingDirectory(this, tr("Select Save Directory"), projectDirectory);
    if (!dirName.isEmpty())
    {
        ui->saveDirectory->setText(dirName);
    }
}

void vampConfigDialog::on_histDirButton_clicked()
{
    QString dirName = QFileDialog::getExistingDirectory(this, tr("Select History Directory"), projectDirectory);
    if (!dirName.isEmpty())
    {
        ui->histDirectory->setText(dirName);
    }
}
