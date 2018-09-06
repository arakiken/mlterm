#!/bin/sh

if [ $# -ne 1 ]; then
	echo "Usage: build-iphonesim.sh [install prefix]"
	exit 0
fi

dir=`dirname $0`

export SDKROOT="/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator4.3.sdk"
export CFLAGS="-g -fobjc-abi-version=2 -fobjc-legacy-dispatch -D__IPHONE_OS_VERSION_MIN_REQUIRED=40300"

$dir/../configure --prefix=$1 --with-gui=quartz --host=i686-apple-darwin --disable-shared --disable-fribidi --disable-ssh2 --disable-otl --without-tools --enable-debug --with-ios-sdk=iphonesimulator4.3 --with-ios-arch=i686
