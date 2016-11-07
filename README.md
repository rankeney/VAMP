# VAMP

VAMP is a C Code Coverage Analysis Tool that helps you determine how much of your C source code has been tested.

VAMP performs statement coverage, branch coverage and MC/DC coverage. It has a graphical user interface that provides
an at-a-glance view of coverage across the entire project.

VAMP offers near-instantaneous performance during:
    * Code instrumentation
    * Code preprocessing with built-in preprocessor
    * Report generation
    * Combining run histories

As part of instrumenting code, VAMP uses a powerful source code parser, which is part of the LLVM/Clang compiler.
This parser may detect coding errors that were missed by the user's compiler. Any errors or warnings detected will be
displayed during instrumentation of the code. Such diagnostics are designed to be helpful in resolving the issue.

VAMP currently runs on Windows and Linux platforms. The GUI is Qt-based, so VAMP can be easily ported to other platforms.
