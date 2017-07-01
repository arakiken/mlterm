#!/bin/sh

if [ -e /tmp/.mlterm-* ]; then
	cmd="mlclient"
else
	cmd="mlterm"
fi

${cmd} --initstr 'printf "\\x1b[1m"\nprintf "\\x1b[2J"\ncat /dev/console||exit 0\n'
