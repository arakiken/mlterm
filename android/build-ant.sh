#!/bin/sh

if test "$ANDROID_HOME" = ""; then
	echo "export ANDROID_HOME=<Android SDK path> in avance."
	exit 1
	#ANDROID_HOME=/cygdrive/c/Program\ Files/Android/android-sdk
fi
if test "$ANDROID_NDK_HOME" = ""; then
	echo "export ANDROID_NDK_HOME=<Android NDK path> in avance."
	exit 1
	#ANDROID_NDK_HOME=/cygdrive/c/Users/${USER}/workspace/android-ndk-r8e
fi
if test "$JAVA_HOME" = ""; then
	echo "export JAVA_HOME=<JDK path> in advance."
	exit 1
	#export JAVA_HOME=c:\\Program\ Files\ \(x86\)\\Java\\jdk1.8.0_172
fi

# Requires android-11 or later.
"${ANDROID_HOME}/tools/android.bat" update project --path . --target android-11
(cd jni ; ${ANDROID_NDK_HOME}/ndk-build APP_ABI=all V=1)
ant release

jarsigner -verbose -sigalg MD5withRSA -digestalg SHA1 bin/mlterm-release-unsigned.apk mlterm
#"${ANDROID_HOME}/platform-tools/adb" connect localhost
#"${ANDROID_HOME}/platform-tools/adb" uninstall mlterm.native_activity
"${ANDROID_HOME}/platform-tools/adb" install -r bin/mlterm-release-unsigned.apk
"${ANDROID_HOME}/platform-tools/adb" logcat
