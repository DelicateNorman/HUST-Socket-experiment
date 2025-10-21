@echo off
chcp 65001 >nul
echo ======================================
echo          TFTP Server Test Script
echo ======================================
echo.

:: Check if executable exists
if not exist tftp_server.exe (
    echo Error: tftp_server.exe not found
    echo Please run build.bat first to compile the project
    pause
    exit /b 1
)

:: Create necessary directories
if not exist tftp_root mkdir tftp_root
if not exist logs mkdir logs

:: Create test file
echo Creating test file...
echo Hello, this is a test file for TFTP transfer! > tftp_root\test.txt
echo TFTP (Trivial File Transfer Protocol) is a simple protocol. >> tftp_root\test.txt
echo This file can be downloaded using standard TFTP clients. >> tftp_root\test.txt

echo Test file created: tftp_root\test.txt
echo.

:: Show usage instructions
echo ======================================
echo              Usage Guide
echo ======================================
echo 1. Start server: tftp_server.exe
echo 2. Test in another command window:
echo.
echo    Download file examples:
echo    tftp -i 127.0.0.1 get test.txt
echo    tftp -i 127.0.0.1 get test.txt downloaded.txt
echo.
echo    Upload file examples:
echo    tftp -i 127.0.0.1 put myfile.txt
echo.
echo 3. Check logs: logs\tftp_server.log
echo 4. File location: tftp_root\ directory
echo ======================================
echo.

:: Ask if user wants to start server
set /p choice="Start TFTP server now? (y/n): "
if /i "%choice%"=="y" (
    echo Starting TFTP server...
    echo Press Ctrl+C to stop server
    echo.
    tftp_server.exe
) else (
    echo Use 'tftp_server.exe' command to start server
)

pause