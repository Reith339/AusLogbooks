@echo off
rem Builds the Windows installer: dist\PilotLogbook-<version>-Windows-Setup.exe
rem Needs MSYS2 UCRT64 (g++, Qt6, CMake, Ninja) and Inno Setup 6.
rem Set MSYS2_ROOT if MSYS2 isn't installed in C:\msys64.
rem Google Sheets backup: set GOOGLE_CLIENT_ID / GOOGLE_CLIENT_SECRET, or use google-oauth.cmake.
setlocal
cd /d "%~dp0.."
if not defined MSYS2_ROOT set MSYS2_ROOT=C:\msys64
set PATH=%MSYS2_ROOT%\ucrt64\bin;%PATH%

set GOOGLE_OPTS=
if defined GOOGLE_CLIENT_ID set GOOGLE_OPTS="-DGOOGLE_CLIENT_ID=%GOOGLE_CLIENT_ID%" "-DGOOGLE_CLIENT_SECRET=%GOOGLE_CLIENT_SECRET%"

cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release %GOOGLE_OPTS% || exit /b 1
cmake --build build-release || exit /b 1
set /p VERSION=<build-release\version.txt

rem Stage the exe with everything it needs.
set STAGE=build-release\stage
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"
copy /y build-release\LogBook.exe "%STAGE%\" >nul
windeployqt6 --release --no-translations --no-opengl-sw --no-system-d3d-compiler --no-compiler-runtime "%STAGE%\LogBook.exe" >nul 2>&1 || exit /b 1
"%MSYS2_ROOT%\usr\bin\bash.exe" ./deploy-deps.sh "%STAGE%" || exit /b 1

rem Find Inno Setup's compiler.
set ISCC=
for %%P in ("%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" "%ProgramFiles%\Inno Setup 6\ISCC.exe") do (
    if exist %%P set ISCC=%%P
)
if not defined ISCC (
    echo Inno Setup 6 not found. Install it with: winget install JRSoftware.InnoSetup
    exit /b 1
)

if not exist dist mkdir dist
%ISCC% /Q /DAppVersion=%VERSION% "/DStageDir=%CD%\%STAGE%" /O"%CD%\dist" packaging\windows\LogBook.iss || exit /b 1
echo.
echo Built: dist\PilotLogbook-%VERSION%-Windows-Setup.exe
