#ifndef ZYGISK_IMGUI_HOOK_H
#define ZYGISK_IMGUI_HOOK_H

#include <jni.h>
#include "zygisk.hpp"
#include <android/log.h>

// ---------------------------------------------------------------------------
//  Logging macros (single definition)
// ---------------------------------------------------------------------------
#define LOG_TAG "zyCheats"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ---------------------------------------------------------------------------
//  Global variables – **declare only** (definitions live in hook.cpp)
// ---------------------------------------------------------------------------
extern int        enable_hack;
extern char*      game_data_dir;

// ---------------------------------------------------------------------------
//  Forward declarations
// ---------------------------------------------------------------------------
int  isGame(JNIEnv* env, jstring appDataDir);
void* hack_thread(void* arg);

void registerPltHook(zygisk::Api* api);

#endif // ZYGISK_IMGUI_HOOK_H