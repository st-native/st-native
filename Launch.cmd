@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
setlocal
set "ROOT=%~dp0."
set "LAUNCHER=%ROOT%\.tools\upstream\ffcc\build\release\st_launcher.exe"
if not exist "%LAUNCHER%" (
  echo Build the Windows developer launcher first by running tools\build\build_program.cmd.
  pause
  exit /b 1
)
start "" /d "%ROOT%" "%LAUNCHER%"
