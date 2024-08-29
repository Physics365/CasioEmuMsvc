//
// Created by 15874 on 2024/8/9.
//

#include "vibration.h"
#include <SDL.h>
#include <jni.h>

void Vibration::vibrate(long milliseconds) {
    // 获取JNI环境
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (env == NULL) {
        return;
    }

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
    env->CallStaticVoidMethod(activityClass, vibrateMethod, (jlong)milliseconds);
}