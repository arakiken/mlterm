#!/bin/sh

ANDROID_SDK_PATH=/cygdrive/c/Program\ Files/Android/android-sdk
ANDROID_NDK_PATH=/cygdrive/c/Users/ken/workspace/android-ndk-r8
export JAVA_HOME=c:\\Program\ Files\\Java\\jdk1.7.0_02

${ANDROID_SDK_PATH}/tools/android.bat update project --path . --target android-15
(cd jni ; ${ANDROID_NDK_PATH}/ndk-build V=1)
ant release
