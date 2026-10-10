@echo off
setlocal
cd /d "%~dp0"
set "PATH=C:\msys64\ucrt64\bin;%PATH%"
if not exist "C:\msys64\ucrt64\bin\g++.exe" (
    echo The MSYS2 UCRT64 compiler is missing. See README-Build.md.
    pause
    exit /b 1
)
cmake --preset default
if errorlevel 1 goto failed
cmake --build --preset default --target Cuebase --parallel 1
if errorlevel 1 goto failed
echo Build completed successfully.
start "Cuebase" "%~dp0build\Cuebase.exe"
exit /b 0
:failed
echo.
echo Build failed. Cuebase was not launched. Keep this window open to read the error above.
pause
exit /b 1
