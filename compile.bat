@echo off
echo Compiling TFTP Server...

:: Create build directory
if not exist build mkdir build

:: Compile all source files
echo Compiling main.c...
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/main.c -o build/main.o

echo Compiling tftp_utils.c...
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/tftp_utils.c -o build/tftp_utils.o

echo Compiling tftp_handlers.c...
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/tftp_handlers.c -o build/tftp_handlers.o

echo Compiling gui_app.c...
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/gui_app.c -o build/gui_app.o

:: Link to create executable
echo Linking...
gcc build/main.o build/tftp_utils.o build/tftp_handlers.o -o tftp_server.exe -lws2_32

echo Linking GUI...
gcc build/gui_app.o -o tftp_gui.exe -mwindows -lcomctl32 -lshlwapi -lcomdlg32

if exist tftp_server.exe (
    echo.
    echo ====================================
    echo   Build Successful!
    echo ====================================
    echo Executable: tftp_server.exe
    if exist tftp_gui.exe (
        echo   GUI Panel: tftp_gui.exe
    )
    echo.
    echo To run the server:
    echo   tftp_server.exe
    echo.
    echo To test the server:
    echo   test.bat
    echo ====================================
) else (
    echo.
    echo Build failed! Please check error messages above.
    echo Make sure you have GCC installed and available in PATH.
)

echo.
pause