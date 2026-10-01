@echo off
rem Builds and runs the Windows harness with the Visual Studio C++ tools.
rem Usage: test\windows\run.cmd  (from the repository root)
setlocal

set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSINSTALL=%%i
if not defined VSINSTALL (
  echo Visual Studio with the C++ tools was not found
  exit /b 1
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

set BUILD=%TEMP%\rnzeroconf-harness
if not exist "%BUILD%" mkdir "%BUILD%"
cl /nologo /EHsc /std:c++17 /W4 /DUNICODE /D_UNICODE /I windows\RNZeroconf ^
  windows\RNZeroconf\ZeroconfCore.cpp test\windows\harness.cpp ^
  /Fo"%BUILD%\\" /Fe"%BUILD%\harness.exe" || exit /b 1

"%BUILD%\harness.exe"
