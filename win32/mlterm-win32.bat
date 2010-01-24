rem Modify [] according to your environment.

set HOMEPATH=[PATH]
rem start mlterm.exe -S sample -e plink.exe [-telnet/-ssh/-rlogin/-raw] [HOST]
start mlterm.exe -S sample --serv [<user>@<proto>:<host>:<encoding>]
exit
