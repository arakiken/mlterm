#!/bin/sh

ANDROID_SDK_PATH=/cygdrive/c/Program\ Files/Android/android-sdk
ANDROID_NDK_PATH=/cygdrive/c/Users/${USER}/workspace/android-ndk-r8
export JAVA_HOME=c:\\Program\ Files\\Java\\jdk1.7.0_02

"${ANDROID_SDK_PATH}/tools/android.bat" update project --path . --target android-10
(cd jni ; ${ANDROID_NDK_PATH}/ndk-build APP_ABI=all V=1)
ant release

jarsigner -verbose -sigalg MD5withRSA -digestalg SHA1 bin/mlterm-release-unsigned.apk mlterm
"${ANDROID_SDK_PATH}/platform-tools/adb" connect localhost
"${ANDROID_SDK_PATH}/platform-tools/adb" uninstall mlterm.native_activity
"${ANDROID_SDK_PATH}/platform-tools/adb" install bin/mlterm-release-unsigned.apk
"${ANDROID_SDK_PATH}/platform-tools/adb" logcat
