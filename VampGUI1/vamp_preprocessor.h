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

#ifndef VAMP_PREPROCESSOR_H
#define VAMP_PREPROCESSOR_H

#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"

#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/Utils.h"

#include "configfile.h"
#include "vamp_ostream.h"
#include <string>

using namespace std;
using namespace clang;

class VampPreprocessor
{
public:
    VampPreprocessor();
    bool Preprocess(string fileName,
                    string &outString,
                    string &errString,
                    VAMP_PREPROC_CONFIG &preprocOpts);
};

#endif // VAMP_PREPROCESSOR_H
