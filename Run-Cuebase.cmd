@echo off
setlocal
set "PATH=C:\msys64\ucrt64\bin;%PATH%"
if exist "%~dp0build-final\Cuebase.exe" (
    start "Cuebase" "%~dp0build-final\Cuebase.exe"
    exit /b
)
if exist "%~dp0build\Cuebase.exe" (
    start "Cuebase" "%~dp0build\Cuebase.exe"
    exit /b
)
echo Build Cuebase first using the instructions in README-Build.md.
pause
