@echo off
chcp 65001 >nul
echo Compiling TFTP Server...

:: Create build directory
if not exist build mkdir build

:: Compile all source files
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/main.c -o build/main.o
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/tftp_utils.c -o build/tftp_utils.o
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/tftp_handlers.c -o build/tftp_handlers.o

:: Link to create executable
gcc build/main.o build/tftp_utils.o build/tftp_handlers.o -o tftp_server.exe -lws2_32

if exist tftp_server.exe (
    echo Build successful! Executable: tftp_server.exe
    echo Run server: tftp_server.exe
) else (
    echo Build failed! Please check error messages.
)

pause