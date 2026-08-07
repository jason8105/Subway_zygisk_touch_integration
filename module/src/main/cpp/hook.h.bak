#ifndef ZygiskImGui_HOOK_H
#define ZygiskImGui_HOOK_H

#include <jni.h>
#include <android/log.h>
#include <EGL/egl.h>

static int enable_hack = 0;
static char *game_data_dir = NULL;

int isGame(JNIEnv *env, jstring appDataDir);
void *hack_thread(void *arg);

// These are now defined in GUI.cpp
extern EGLBoolean (*old_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
extern EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);

#define LOG_TAG "zyCheats"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#endif //ZygiskImGui_HOOK_H