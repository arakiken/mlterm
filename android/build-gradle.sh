#!/bin/sh

if test "$ANDROID_HOME" = ""; then
	echo "export ANDROID_HOME=<Android SDK path> in avance."
	exit 1
fi
if test "$ANDROID_NDK_HOME" = ""; then
	echo "export ANDROID_NDK_HOME=<Android NDK path> in avance."
	exit 1
fi
if test "$JAVA_HOME" = ""; then
	echo "export JAVA_HOME=<JDK path> in advance."
	exit 1
fi

V=1 ./gradlew assembleRelease --overwrite --scan

jarsigner -verbose -sigalg MD5withRSA -digestalg SHA1 app/build/outputs/apk/release/app-release-unsigned.apk mlterm
#"${ANDROID_HOME}/platform-tools/adb" connect localhost
#"${ANDROID_HOME}/platform-tools/adb" uninstall mlterm.native_activity
"${ANDROID_HOME}/platform-tools/adb" install -r bin/mlterm-release-unsigned.apk
"${ANDROID_HOME}/platform-tools/adb" logcat
