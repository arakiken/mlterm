#!/bin/sh

if ! test -f src/mlterm/MLTermPty.java ; then
	echo "Execute prepare.sh in advance."
	exit 1
fi

${ANDROID_SDK_PATH}/tools/android.bat update project --path . --target android-15
(cd jni ; ${ANDROID_NDK_PATH}/ndk-build V=1)
ant release
