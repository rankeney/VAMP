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

#ifndef VAMPCONFIGDIALOG_H
#define VAMPCONFIGDIALOG_H

#include "configfile.h"
#include <QDialog>

namespace Ui {
class vampConfigDialog;
}

class vampConfigDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit vampConfigDialog(QWidget *parent = 0);
    ~vampConfigDialog();
    void setProjectDirectory(QString projectDirName);
    void getVcData(VAMP_CONFIG &vcData, bool &vcDataChanged);
    void getVcReportData(VAMP_REPORT_CONFIG &vcReportData, bool &vcReportDataChanged);
    void getVcPreProcData(VAMP_PREPROC_CONFIG &vcReportData, bool &vcPreProcDataChanged);
    void setVcData(VAMP_CONFIG &vcData);
    void setVcReportData(VAMP_REPORT_CONFIG &vcReportData);
    void setVcPreProcData(VAMP_PREPROC_CONFIG &vcReportData);

private slots:
    void on_singleStmtCoverage_stateChanged(int arg1);

    void on_countStmtCoverage_stateChanged(int arg1);

    void on_mcdcCoverage_stateChanged(int arg1);

    void on_conditionCoverage_stateChanged(int arg1);

    void on_preProcCheckBox_stateChanged(int arg1);

    void on_useVampPreprocCheckBox_stateChanged(int arg1);

    void on_buttonBox_accepted();

    void on_addIncludeButton_clicked();

    void on_htmlDirButton_clicked();

    void on_preProcDirButton_clicked();

    void on_saveDirButton_clicked();

    void on_histDirButton_clicked();

private:
    Ui::vampConfigDialog *ui;
    QString projectDirectory;
    VAMP_CONFIG vc;
    VAMP_REPORT_CONFIG vcReport;
    VAMP_PREPROC_CONFIG vcPreProc;
};

#endif // VAMPCONFIGDIALOG_H
