#!/bin/sh

if [ $# != 3 ]; then
	echo "Create multi-arch binary"
	echo "usage: lipo.sh [binary1] [binary2] [output]"
	exit
fi

/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/lipo -create $1 $2 -output $3
