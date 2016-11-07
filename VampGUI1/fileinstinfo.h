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

#ifndef FILEINSTINFO_H
#define FILEINSTINFO_H

#include <QString>
#include <QVector>

enum INST_INFO {
    INST_NO,
    INST_YES,
    INST_OLD
};

typedef struct {
    QString functionName;
    int statementCount;
    int statementsCovered;
    int branchCount;
    int branchesCovered;
    int decisionCount;
    int decisionsCovered;
} FUNCTION_INFO;

class fileInstInfo
{
public:
    fileInstInfo();

    QString fileName;
    INST_INFO instInfo;
    bool mcdcStackOverflow;
    int totalStatementCount;
    int totalStatementsCovered;
    int totalBranchCount;
    int totalBranchesCovered;
    int totalMcdcCount;
    int totalMcdcCovered;
    int totalConditionCount;
    int totalConditionsCovered;
    QVector<FUNCTION_INFO> functionInfo;
};

#endif // FILEINSTINFO_H
