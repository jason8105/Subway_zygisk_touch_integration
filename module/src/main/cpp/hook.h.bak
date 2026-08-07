#ifndef ZygiskImGui_HOOK_H
#define ZygiskImGui_HOOK_H

#include <jni.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

static int enable_hack = 0;
static char *game_data_dir = NULL;

int isGame(JNIEnv *env, jstring appDataDir);
void *hack_thread(void *arg);

extern EGLBoolean (*old_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
extern EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);

extern EGLBoolean (*old_eglSwapBuffersWithDamageKHR)(EGLDisplay dpy, EGLSurface surface, EGLint* rects, EGLint n_rects);
extern EGLBoolean hook_eglSwapBuffersWithDamageKHR(EGLDisplay dpy, EGLSurface surface, EGLint* rects, EGLint n_rects);

extern void (*old_eglMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
extern void hook_eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);

#define LOG_TAG "zyCheats"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#endif