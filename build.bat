@echo off
rem Builds LogBook with MSYS2 UCRT64 (g++ + Qt6) and copies the DLLs it needs next to the exe.
cd /d "%~dp0"
set PATH=C:\msys64\ucrt64\bin;%PATH%
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build || exit /b 1
windeployqt6 --no-translations --no-opengl-sw --no-system-d3d-compiler build\LogBook.exe >nul 2>&1 || exit /b 1
rem windeployqt doesn't copy the MinGW runtime / third-party DLLs Qt depends on.
C:\msys64\usr\bin\bash.exe ./deploy-deps.sh || exit /b 1
echo.
echo Built: build\LogBook.exe
