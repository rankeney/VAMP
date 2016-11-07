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

#include "vamp_preprocessor.h"

VampPreprocessor::VampPreprocessor()
{
}

// Preprocess designated file
bool VampPreprocessor::Preprocess(string fileName,
                                  string &outString,
                                  string &errString,
                                  VAMP_PREPROC_CONFIG &preprocOpts)
{
  CompilerInstance ci;

  DiagnosticOptions *diagOptions = new clang::DiagnosticOptions();
  diagOptions->ShowColors = true;
  vamp_string_ostream preProcOut(outString);
  vamp_string_ostream preProcErr(errString);

  TextDiagnosticPrinter *diagPrinter = new TextDiagnosticPrinter(preProcErr, diagOptions);
  llvm::IntrusiveRefCntPtr<DiagnosticIDs> diagIDs(new DiagnosticIDs);
  DiagnosticsEngine *diagEngine = new DiagnosticsEngine(diagIDs, diagOptions, diagPrinter);
  ci.setDiagnostics(diagEngine);

  shared_ptr<TargetOptions> pto( new TargetOptions());
  pto->Triple = llvm::sys::getDefaultTargetTriple();
  TargetInfo *pti = TargetInfo::CreateTargetInfo(ci.getDiagnostics(), pto);
  ci.setTarget(pti);

  ci.createFileManager();
  ci.createSourceManager(ci.getFileManager());

//  std::string fileName(argv[argc - 1]);
  FrontendInputFile inputFile(fileName, IK_None);
  ci.InitializeSourceManager(inputFile);

  HeaderSearchOptions &headerOpts = ci.getHeaderSearchOpts();
  headerOpts.UseBuiltinIncludes = 0;
  headerOpts.UseStandardSystemIncludes = 0;
  headerOpts.UseStandardCXXIncludes = 0;
//headerOpts.Verbose = 1;
//  if (g->debugPrint)
//  headerOpts.Verbose = 1;
  for (int i = 0; i < preprocOpts.includePaths.size(); ++i)
  {
    headerOpts.AddPath(preprocOpts.includePaths[i].data(),
                       clang::frontend::Angled,
                       false /* not a framework */,
                       true /* ignore sys root */);
  }

  PreprocessorOptions &ppOpts = ci.getPreprocessorOpts();
  //ppOpts.UsePredefines = 1;
  // Force "predefined" macro definition
  ppOpts.addMacroDef("__NO_INLINE__");
  for (int i = 0; i < preprocOpts.definesList.size(); ++i)
  {
    ppOpts.addMacroDef(preprocOpts.definesList[i].data());
  }

  PreprocessorOutputOptions &Opts = ci.getPreprocessorOutputOpts();
  Opts.ShowCPP = 1;
  Opts.ShowComments = preprocOpts.retainComments ? 1 : 0;
  Opts.ShowLineMarkers = preprocOpts.showLineMarkers ? 1 : 0;
  Opts.ShowMacroComments = 0;
  Opts.ShowMacros = 0;
  Opts.RewriteIncludes = 0;

  ci.createPreprocessor(TU_Module);
//InitializePreprocessor(ci.getPreprocessor(), ppOpts, headerOpts, ci.getFrontendOpts());
  diagPrinter->BeginSourceFile(ci.getLangOpts(), &ci.getPreprocessor());
  DoPrintPreprocessedInput(ci.getPreprocessor(),
                           //&llvm::outs(),
                           &preProcOut,
                           ci.getPreprocessorOutputOpts());
  diagPrinter->EndSourceFile();

  if (ci.getDiagnostics().hasErrorOccurred())
  {
    //*vampOut << ENDL << "Fatal error(s) occurred - instrumentation aborted" << ENDL;
    //preProcOut << ENDL << "Fatal error(s) occurred - instrumentation aborted" << ENDL;
    return false;
  }
  // Fixme - check for errors
  return true;
}

