@echo off
setlocal
call "%~1\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b %errorlevel%
cd /d "%~2\win"
nmake -f makefile.vc INSTALLDIR="C:\Program Files\tcl" release
if errorlevel 1 exit /b %errorlevel%
nmake -f makefile.vc INSTALLDIR="C:\Program Files\tcl" install
if errorlevel 1 exit /b %errorlevel%
cd /d "%~3\win"
nmake -f makefile.vc TCLDIR="%~2" INSTALLDIR="C:\Program Files\tcl" release
if errorlevel 1 exit /b %errorlevel%
nmake -f makefile.vc TCLDIR="%~2" INSTALLDIR="C:\Program Files\tcl" install
if errorlevel 1 exit /b %errorlevel%
exit /b 0
