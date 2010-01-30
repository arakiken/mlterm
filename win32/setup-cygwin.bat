@echo off
rem 'set /p' is effective in Windows2000 or later.
set /p CYGWIN_DIR="Where CYGWIN is installed[c:\cygwin]"
if "%CYGWIN_DIR%" == "" set CYGWIN_DIR=c:\cygwin
echo set HOME=%CYGWIN_DIR%\home\%USERNAME%> mlterm.bat
echo set PATH=%CYGWIN_DIR%\bin;%%PATH%%>> mlterm.bat
echo start mlterm -S sample -e /bin/bash --login -i>> mlterm.bat
echo exit>> mlterm.bat
