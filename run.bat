@echo off
setlocal
cd /d "%~dp0"

where gcc >nul 2>&1
if errorlevel 1 (
  echo gcc not found. Install MinGW-w64 OR build with Visual Studio.
  echo See OFFICE_SETUP.md
  exit /b 1
)

if not exist build mkdir build
if not exist config mkdir config
if not exist reports mkdir reports

echo Building...
gcc -std=c11 -Wall -Wextra -O2 -Iinclude -Ithird_party/miniz -o build\uds_tester.exe ^
  src\main.c src\hexutil.c src\config_csv.c src\xlsx_config.c src\bus.c src\isotp.c src\pcan_bus.c src\runner.c src\report.c ^
  third_party\miniz\miniz.c third_party\miniz\miniz_tdef.c third_party\miniz\miniz_tinfl.c third_party\miniz\miniz_zip.c
if errorlevel 1 exit /b 1

if not exist config\test_cases.xlsx (
  build\uds_tester.exe --create-config
  echo Edit config\test_cases.xlsx ^(Setup + TestCases sheets^), then run run.bat again.
  exit /b 0
)

echo Running...
build\uds_tester.exe %*
exit /b %ERRORLEVEL%
