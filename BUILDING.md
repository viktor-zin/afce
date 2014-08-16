How to build AFCE?
==================

AFCE can be built for Microsoft Windows, GNU/Linux or MacOS X. Generally it uses Qt Library (http://qt-project.org).
You can use Qt 4 or Qt 5.

General pre-requirements
-----------------------
* GCC >= 4.7 (Windows: mingw-gcc)
* Make (Windows: mingw-make)
* Qt 4 or Qt 5


Qt configuration in Linux
-------------------------
AFCE uses following modules: QtGui, QtSvg, QtXml, QtWidgets, QtPrintSupport. To build AFCE you should to install corresponding "-devel" packages. To compile translations AFCE uses lupdate and lrelease tools. Therefore it may require additional packages, for example, 'qttools5'.

Building in Windows
-------------------
* Download sources from https://github.com/viktor-zin/afce/archive/master.zip
* Unpack master.zip to current folder
* cd afce-master
* mkdir build
* To run Qt tools it needs Qt 'bin' directories to be in PATH. For example:
* set PATH=C:\Qt\5.2.1\mingw48_32\bin;C:\Qt/Tools/mingw48_32\bin;%PATH%
* qmake ..\afce.pro
* mingw32-make -f MakeFile.Release

Building in Linux
-----------------
* Download sources from https://github.com/viktor-zin/afce/archive/master.zip
* Unpack master.zip to current folder
* cd afce-master
* mkdir build
* qmake ..\afce.pro
* make -f MakeFile.Release

Using Qt Creator
----------------
Also you can use Qt Creator. Just open afce.pro and build AFCE (Ctrl+B).

Creation Windows installer
--------------------------
There is afce.nsi file in the source files. This file can be compiled by Nullsoft Installation System (NSIS). You can correct some constants for your environment.
