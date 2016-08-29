@echo off
rem 'set /p' is effective in Windows2000 or later.
set /p MSYS_DIR="Where MSYS is installed[c:\MSYS\1.0]"
if "%MSYS_DIR%" == "" set MSYS_DIR=c:\MSYS\1.0
echo set HOME=%MSYS_DIR%\home\%USERNAME%> mlterm.bat
echo set PATH=%MSYS_DIR%\bin;%%PATH%%>> mlterm.bat
echo set CYGWIN=tty>> mlterm.bat
echo start mlterm -S sample -e /bin/sh --login -i>> mlterm.bat
echo exit>> mlterm.bat
