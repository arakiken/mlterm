#!/bin/sh

if test "$ANDROID_SDK_PATH" = ""; then
	ANDROID_SDK_PATH=/cygdrive/c/Program\ Files/Android/android-sdk
fi
if test "$ANDROID_NDK_PATH" = ""; then
	ANDROID_NDK_PATH=/cygdrive/c/Users/${USER}/workspace/android-ndk-r8
fi
if test "$JAVA_HOME" = ""; then
	export JAVA_HOME=c:\\Program\ Files\\Java\\jdk1.8.0_51
fi

# Requires android-11 or later.
"${ANDROID_SDK_PATH}/tools/android.bat" update project --path . --target android-11
(cd jni ; ${ANDROID_NDK_PATH}/ndk-build APP_ABI=all V=1)
ant release

jarsigner -verbose -sigalg MD5withRSA -digestalg SHA1 bin/mlterm-release-unsigned.apk mlterm
#"${ANDROID_SDK_PATH}/platform-tools/adb" connect localhost
#"${ANDROID_SDK_PATH}/platform-tools/adb" uninstall mlterm.native_activity
"${ANDROID_SDK_PATH}/platform-tools/adb" install -r bin/mlterm-release-unsigned.apk
"${ANDROID_SDK_PATH}/platform-tools/adb" logcat
