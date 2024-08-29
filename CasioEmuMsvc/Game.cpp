//
// Created by 15874 on 2024/8/9.
//
#include <jni.h>

extern "C" {
JNIEXPORT void JNICALL Java_com_tele_u8emulator_Game_nativeVibrate(JNIEnv* env, jclass cls, jlong milliseconds) {
// 获取SDLActivity类
jclass activityClass = env->FindClass("com/tele/u8emulator/Game");
if (activityClass == NULL) {
return;
}

// 获取nativeVibrate静态方法的ID
jmethodID vibrateMethod = env->GetStaticMethodID(activityClass, "nativeVibrate", "(J)V");
if (vibrateMethod == NULL) {
return;
}

// 调用nativeVibrate静态方法
env->CallStaticVoidMethod(activityClass, vibrateMethod, milliseconds);
}
}