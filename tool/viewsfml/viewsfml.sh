#!/bin/sh

if [ ${#} -ne 1 ]; then
	echo "viewsfml.sh [thread_id]"
	exit  1
fi

wget -O - "http://sourceforge.net/mailarchive/forum.php?thread_id=${1}&forum_id=3020" | \
sed 's/\&amp;/\&/g' | sed 's/\&gt;/>/g' | sed 's/\&lt;/</g' > ~/.mlterm/${1}.html
