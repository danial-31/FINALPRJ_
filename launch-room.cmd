@echo off
setlocal
set "roomExe=%~dp0build\Release\finalprj.exe"
if not exist "%roomExe%" set "roomExe=%~dp0build\Debug\finalprj.exe"
if not exist "%roomExe%" (
    echo THE OSSUARY: no built application was found.
    echo Build the project in Release mode first. See README.md.
    echo Expected: build\Release\finalprj.exe or build\Debug\finalprj.exe
    pause
    exit /b 1
)
for %%I in ("%roomExe%") do pushd "%%~dpI"
if errorlevel 1 (
    echo THE OSSUARY: could not open the application folder.
    pause
    exit /b 1
)
"%roomExe%"
set "roomExitCode=%errorlevel%"
popd
if not "%roomExitCode%"=="0" (
    echo.
    echo THE OSSUARY stopped with error %roomExitCode%. See the messages above.
    pause
)
exit /b %roomExitCode%
