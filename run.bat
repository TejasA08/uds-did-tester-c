@echo off
setlocal
cd /d "%~dp0"

where gcc >nul 2>&1
if errorlevel 1 (
  echo gcc not found. Install MinGW-w64 OR build with Visual Studio using:
  echo   cl /Iinclude src\*.c /Fe:build\uds_tester.exe
  echo For Peak hardware also install PCAN-Basic and build with UDS_HAS_PCAN.
  exit /b 1
)

if not exist build mkdir build
if not exist config mkdir config
if not exist reports mkdir reports

echo Building...
gcc -std=c11 -Wall -Wextra -O2 -Iinclude -o build\uds_tester.exe src\main.c src\hexutil.c src\config_csv.c src\bus.c src\isotp.c src\pcan_bus.c src\runner.c src\report.c
if errorlevel 1 exit /b 1

if not exist config\setup.csv (
  build\uds_tester.exe --create-config
  echo Edit config\setup.csv and config\test_cases.csv then run run.bat again.
  exit /b 0
)

echo Running...
build\uds_tester.exe %*
exit /b %ERRORLEVEL%
