#!/bin/sh

if [ $# -ne 1 ]; then
	echo "Usage: build-iphonesim.sh [install prefix]"
	exit 0
fi

dir=`dirname $0`

export SDKROOT="/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS4.3.sdk"
export CFLAGS="-g -D__arm__=1"

$dir/../configure --prefix=$1 --with-gui=quartz --host=arm-apple-darwin --disable-shared --disable-fribidi --disable-ssh2 --disable-otl --without-tools --enable-debug --with-ios-sdk=iphoneos4.3 --with-ios-arch=armv7
