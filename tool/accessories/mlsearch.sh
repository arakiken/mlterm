#!/bin/sh

reset_state()
{
	printf "\x1b]5379;search_$1=\x07"
	stty echo
	exit 0
}

if test ${#} = 0 ; then
	echo "Reset searching position."
	printf "\x1b]5379;search_$1=\x07"
	exit  0
elif test "$1" = "-h" -o ${#} != 2 ; then
	echo "Usage: mlsearch (prev|next) [keyword]"
	exit  0
fi

echo "Press Enter key to continue searching $1. Press ^C to exit."

trap "reset_state $1" 2
stty -echo 
word=$2
printf "\x1b]5379;search_$1=$word\x07"
read input
while  true
do printf "\x1b]5379;search_$1=$word\x07" ; read input
done

reset_state $1
