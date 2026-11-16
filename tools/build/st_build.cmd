@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
rem Windows x64: build the Spirit Tracks game module from your ROM into a new folder (tools\build\st_build.py),
rem in a VS 2022 x64 developer environment with LLVM on PATH.
rem   tools\build\st_build.cmd "<your.nds>" <new output folder> [more st_build.py options, e.g. --extra-hooks <file>]
setlocal
if "%~2"=="" (echo usage: tools\build\st_build.cmd "^<your.nds^>" ^<new output folder^> [options] & exit /b 2)
set VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
if not exist "%VSDIR%" set VSDIR=C:\Program Files\Microsoft Visual Studio\2022\Community
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || (echo VS 2022 C++ build tools not found & exit /b 1)
set PATH=C:\Program Files\LLVM\bin;%PATH%
set PY=%~dp0..\..\.venv\Scripts\python.exe
if not exist "%PY%" set PY=python
set EXTRA=
:args
if "%~3"=="" goto run
set EXTRA=%EXTRA% %3
shift /3
goto args
:run
"%PY%" -I "%~dp0st_build.py" --rom "%~1" --out "%~2"%EXTRA%
