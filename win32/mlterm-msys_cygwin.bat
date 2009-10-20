rem Modify [] according to your environment.

set HOME=[HOMEPATH]
set PATH=%PATH%;[MSYS or CYGWIN DIR]\bin
start mlterm -S sample -e /bin/sh --login -i
exit
