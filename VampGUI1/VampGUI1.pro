#-------------------------------------------------
#
# Project created by QtCreator 2013-04-21T20:34:41
#
#-------------------------------------------------
#QMAKE_CXX = clang++
#QMAKE_CC = clang
#QMAKE_LIB = llvm-ld -link-as-library -o
#QMAKE_RUN_CXX = $(CXX) $(CXXFLAGS) $(INCPATH) -c --target=i686-pc-mingw32 $src -o $obj
#QMAKE_RUN_CC = $(CC) $(CCFLAGS) $(INCPATH) -c --target=i686-pc-mingw32 $src -o $obj

QT       += webkitwidgets core gui printsupport
#QT     += network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = VampGUI
TEMPLATE = app

DEFINES += _GNU_SOURCE
DEFINES += __STDC_CONSTANT_MACROS
DEFINES += __STDC_FORMAT_MACROS
DEFINES += __STDC_LIMIT_MACROS
DEFINES += USE_QT

QMAKE_CXXFLAGS += -fvisibility-inlines-hidden -fno-rtti -fpermissive -Woverloaded-virtual -Wcast-qual
QMAKE_CXXFLAGS += -std=c++11
#QMAKE_CXXFLAGS_WARN_OFF -= -Wunused-parameter
QMAKE_CXXFLAGS_WARN_ON = ""
QMAKE_CXXFLAGS += -Wno-all
QMAKE_CXXFLAGS += -Wno-endif-labels
QMAKE_CXXFLAGS += -Wno-unused-variable
QMAKE_CXXFLAGS += -Wno-unused-parameter
QMAKE_CXXFLAGS += -Wno-switch
QMAKE_CXXFLAGS += -Wtrigraphs
QMAKE_CXXFLAGS += -Wreturn-type
QMAKE_CXXFLAGS += -Wnon-virtual-dtor
QMAKE_CXXFLAGS += -Woverloaded-virtual
#QMAKE_CXXFLAGS += -Wunused-variable
#QMAKE_CXXFLAGS += -Wunused-value
QMAKE_CXXFLAGS += -Wunknown-pragmas
QMAKE_CXXFLAGS += -Wno-shadow
QMAKE_CXXFLAGS += -Wno-deprecated-declarations
QMAKE_CXXFLAGS += -Wno-missing-braces

SOURCES += main.cpp\
    mainwindow.cpp \
    vampconfigdialog.cpp \
    json.cpp \
    configfile.cpp \
    projectitem.cpp \
    projectmodel.cpp \
    fileinstinfo.cpp \
    vamp.cpp \
    vamp_rewrite.cpp \
    mcdcExprTree.cpp \
    vamp_process.cpp \
    vamp_ostream.cpp \
    vamp_preprocessor.cpp \
    path.cpp

HEADERS  += mainwindow.h \
    vampconfigdialog.h \
    json.h \
    configfile.h \
    projectitem.h \
    projectmodel.h \
    fileinstinfo.h \
    vamp.h \
    vamp_rewrite.h \
    vamp_ostream.h \
    mcdcExprTree.h \
    stdcapture.h \
    vamp_process.h \
    vamp_preprocessor.h \
    splash.h \
    version.h \
    path.h

FORMS    += mainwindow.ui \
    vampconfigdialog.ui


#INCLUDEPATH += c:/cygwin/usr/local/include
#INCLUDEPATH += c:/cygwin/usr/local/include/clang/include
##INCLUDEPATH += c:/MinGW/lib/clang/include/3.3
INCLUDEPATH += c:/llvm/3.7/include
#INCLUDEPATH += /usr/local/include/llvm/3.4/include
INCLUDEPATH += /usr/include/dbus-1.0
INCLUDEPATH += /usr/lib/x86_64-linux-gnu/dbus-1.0/include
INCLUDEPATH += /usr/lib64/dbus-1.0/include

#LIBS += -Lc:/MinGW/msys/1.0/lib
##LIBS += -Lc:/MinGW/lib/clang/3.3/lib
LIBS += -Lc:/llvm/3.7/lib
#LIBS += -L/usr/lib/llvm-3.4/lib
#LIBS += -LC:/Qt/Qt5.4.1/5.4/mingw491_32/lib
LIBS += -lclangFrontend -lclangFrontendTool -lclangDriver -lclangSerialization
LIBS += -lclangCodeGen -lclangParse -lclangSema -lclangAnalysis -lclangEdit
LIBS += -lclangAST -lclangLex -lclangBasic -lclangRewriteFrontend
#LIBS += -lclangRewriteCore
LIBS += -lclangRewrite
LIBS += -lLLVMAsmParser -lLLVMTableGen -lLLVMX86Disassembler
#LIBS += -lLLVMDebugInfo
LIBS += -lLLVMX86AsmParser -lLLVMX86CodeGen -lLLVMSelectionDAG -lLLVMAsmPrinter
LIBS += -lLLVMX86Desc -lLLVMX86Info -lLLVMX86AsmPrinter -lLLVMX86Utils
LIBS += -lLLVMMCDisassembler -lLLVMMCParser -lLLVMInstrumentation -lLLVMOption
##LIBS += -lLLVMArchive
LIBS += -lLLVMBitReader -lLLVMInterpreter -lLLVMipo -lLLVMVectorize
LIBS += -lLLVMLinker -lLLVMBitWriter -lLLVMMCJIT
#LIBS += -lLLVMJIT
LIBS += -lLLVMCodeGen
LIBS += -lLLVMObjCARCOpts -lLLVMScalarOpts -lLLVMInstCombine -lLLVMTransformUtils
LIBS += -lLLVMipa -lLLVMAnalysis -lLLVMRuntimeDyld -lLLVMExecutionEngine
LIBS += -lLLVMTarget -lLLVMMC -lLLVMObject -lLLVMCore -lLLVMSupport
LIBS += -limagehlp
#LIBS += -lclangDiagnosticOptions
#LIBS += -ljson
LIBS += -lQt5PrintSupport
#LIBS += -ldl
#LIBS += -ltinfo
#LIBS += -ldbus-1
#LIBS += -lz

RC_FILE = vamp.rc
