#!/bin/sh

apply_cfg()
{
    if [ -f ~/.config/mlterm/$1 ]; then
	FILE=~/.config/mlterm/$1
    elif [ -f ~/.mlterm/$1 ]; then
	FILE=~/.mlterm/$1
    else
	echo "~/.mlterm/$1 is not found"
	return
    fi

    # GNU sed
    cat $FILE | sed -n "s/\\([^ =]*\\)*[ =]*\\(.*\\)/\\x1b]5379;$1:\\1=\\2\\x07/p" | tr -d '\n'
}

for f in font vfont aafont vaafont tfont taafont; do
    apply_cfg $f
done
