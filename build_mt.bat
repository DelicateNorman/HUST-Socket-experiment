@echo off
echo Building Multi-threaded TFTP Server...

REM Create build directory if not exists
if not exist build mkdir build

REM Compile multi-threaded TFTP server
gcc -Wall -Wextra -O2 src/tftp_server_mt.c src/tftp_utils.c -o tftp_server_mt.exe -lws2_32

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo   Multi-threaded TFTP Server Build SUCCESS!
    echo ========================================
    echo Executable: tftp_server_mt.exe
    echo.
    echo Usage:
    echo   .\tftp_server_mt.exe
    echo.
    echo Features:
    echo   - Multi-client concurrent access
    echo   - Thread-safe logging
    echo   - Automatic thread creation
    echo   - Complete error handling
    echo ========================================
) else (
    echo.
    echo ========================================
    echo   Build FAILED! Please check error messages.
    echo ========================================
)

pause