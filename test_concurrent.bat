@echo off
echo ========================================
echo     Multi-threaded TFTP Server Test
echo ========================================
echo.

REM Check if server is compiled
if not exist tftp_server_mt.exe (
    echo Error: tftp_server_mt.exe not found
    echo Please run build_mt.bat first
    pause
    exit /b 1
)

REM Create test files
echo Creating test files...
echo This is test file 1 for concurrent download testing. > tftp_root\test1.txt
echo This is test file 2 for concurrent download testing. > tftp_root\test2.txt
echo This is test file 3 for concurrent download testing. > tftp_root\test3.txt

echo Content for upload test 1 > upload_test1.txt
echo Content for upload test 2 > upload_test2.txt
echo Content for upload test 3 > upload_test3.txt

echo.
echo Test files created successfully.
echo.
echo ========================================
echo Please follow these steps:
echo ========================================
echo.
echo 1. Start the server in a new window:
echo    .\tftp_server_mt.exe
echo.
echo 2. Wait for server startup, then press any key...
pause

echo.
echo 3. Starting concurrent tests...
echo.

REM Concurrent download test
echo Starting download test...
start /B cmd /c "tftp -i 127.0.0.1 get test1.txt download1.txt"
start /B cmd /c "tftp -i 127.0.0.1 get test2.txt download2.txt"
start /B cmd /c "tftp -i 127.0.0.1 get test3.txt download3.txt"

echo Waiting for downloads...
timeout /t 3 /nobreak > nul

echo.
echo Starting upload test...
start /B cmd /c "tftp -i 127.0.0.1 put upload_test1.txt uploaded1.txt"
start /B cmd /c "tftp -i 127.0.0.1 put upload_test2.txt uploaded2.txt"
start /B cmd /c "tftp -i 127.0.0.1 put upload_test3.txt uploaded3.txt"

echo Waiting for uploads...
timeout /t 3 /nobreak > nul

echo.
echo ========================================
echo Test completed!
echo ========================================
echo.
echo Check results:
echo.
echo Downloaded files:
if exist download1.txt echo   OK download1.txt
if exist download2.txt echo   OK download2.txt
if exist download3.txt echo   OK download3.txt

echo.
echo Uploaded files in tftp_root:
if exist tftp_root\uploaded1.txt echo   OK uploaded1.txt
if exist tftp_root\uploaded2.txt echo   OK uploaded2.txt
if exist tftp_root\uploaded3.txt echo   OK uploaded3.txt

echo.
echo Check server logs: logs\tftp_server_mt.log
echo.

if exist logs\tftp_server_mt.log (
    echo Recent server logs:
    echo ----------------------------------------
    powershell "Get-Content logs\tftp_server_mt.log | Select-Object -Last 10"
    echo ----------------------------------------
)

echo.
echo Press any key to exit...
pause

REM Clean up
echo.
echo Clean up test files? (y/n)
set /p cleanup=
if /i "%cleanup%"=="y" (
    del /q download*.txt 2>nul
    del /q upload_test*.txt 2>nul
    del /q tftp_root\uploaded*.txt 2>nul
    del /q tftp_root\test1.txt 2>nul
    del /q tftp_root\test2.txt 2>nul
    del /q tftp_root\test3.txt 2>nul
    echo Test files cleaned up.
)