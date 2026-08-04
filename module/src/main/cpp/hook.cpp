#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <sys/system_properties.h>
#include <dlfcn.h>
#include <cstdlib>
#include <cinttypes>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/input.h>
#include <android/native_window.h>
#include <android/log.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"
#include "KittyMemory/KittyMemory.h"
#include "KittyMemory/MemoryPatch.h"
#include "KittyMemory/KittyScanner.h"
#include "KittyMemory/KittyUtils.h"
#include "Includes/Dobby/dobby.h"
#include "Include/Unity.h"
#include "Misc.h"
#include "hook.h"
#include "Include/Roboto-Regular.h"

#define GAME_PACKAGE_NAME "com.innersloth.spacemafia"
#define LOG_TAG "AmongUsZygisk"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// JNI Forward Declarations
extern "C" {
    jint get_unity_screen_width(JNIEnv *env, jobject context);
    jint get_unity_screen_height(JNIEnv *env, jobject context);
}

// Global State
bool g_isGameAttached = false;
bool g_imguiReady = false;
int glHeight = 0, glWidth = 0;
float g_displayWidth = 1920.0f; // Fallback
float g_displayHeight = 1080.0f;
jobject g_androidActivity = nullptr;

// Dobby Stubs
int (*orig_AInputQueue_getEvent)(AInputQueue* queue, AInputEvent** outEvent) = nullptr;
void (*orig_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface) = nullptr;

// --- FIXED Touch Handler ---
int hooked_AInputQueue_getEvent(AInputQueue* queue, AInputEvent** outEvent) {
    int result = orig_AInputQueue_getEvent(queue, outEvent);
    if (result >= 0 && *outEvent != nullptr && g_imguiReady) {
        int32_t type = AInputEvent_getType(*outEvent);
        if (type == AINPUT_EVENT_TYPE_MOTION) {
            AmotionEvent* motion = (AmotionEvent*)*outEvent;
            int32_t action = AMotionEvent_getAction(motion);
            int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;
            int32_t pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

            // Safe pointer ID extraction
            int32_t pointerId = AMotionEvent_getPointerId(motion, pointerIndex);
            if (pointerIndex >= 0 && pointerId >= 0) {
                float rawX = AMotionEvent_getX(motion, pointerIndex);
                float rawY = AMotionEvent_getY(motion, pointerIndex);

                // Normalize to 0.0-1.0 then map to ImGui DisplaySize
                float normX = (g_displayWidth > 0) ? (rawX / g_displayWidth) : 0.5f;
                float normY = (g_displayHeight > 0) ? (rawY / g_displayHeight) : 0.5f;

                ImGuiIO& io = ImGui::GetIO();
                io.MousePos = ImVec2(normX * io.DisplaySize.x, normY * io.DisplaySize.y);

                switch (actionMasked) {
                    case AMOTION_EVENT_ACTION_DOWN:
                    case AMOTION_EVENT_ACTION_POINTER_DOWN:
                        io.MouseDown[0] = true; break;
                    case AMOTION_EVENT_ACTION_UP:
                    case AMOTION_EVENT_ACTION_POINTER_UP:
                        io.MouseDown[0] = false; break;
                    case AMOTION_EVENT_ACTION_MOVE:
                        break;
                }
            }
        }
    }
    // CRITICAL: Finish event to prevent Android input stack corruption
    if (result >= 0 && *outEvent != nullptr) {
        AInputQueue_finishEvent(queue, *outEvent, 1);
    }
    return result;
}

// --- FIXED eglSwapBuffers Hook (Render Sync) ---
void hooked_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    // Sync ImGui frame with Unity's GL context
    if (g_imguiReady && dpy != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE) {
        ImGuiIO& io = ImGui::GetIO();
        io.DeltaTime = 1.0f / 60.0f; // Fallback, better to fetch actual
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplAndroid_NewFrame();
        ImGui::NewFrame();

        // Your Menu Drawing Here
        ImGui::Begin("Among Us Menu");
        ImGui::Text("Touch Fixed & Synced");
        ImGui::Text("GL Res: %dx%d", glWidth, glHeight);
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    orig_eglSwapBuffers(dpy, surface);
}

// --- Hook Initialization Thread ---
void *hack_thread(void *arg) {
    // Wait for libil2cpp.so
    do {
        sleep(1);
        KittyMemory::MapInfo il2cppInfo = KittyMemory::getLibraryBaseMap("libil2cpp.so");
        if (il2cppInfo.isValid()) {
            LOGI("il2cpp base: %p", (void*)il2cppInfo.startAddress);
            // Pointers(); Hooks(); // Add your IL2CPP hooks here
            break;
        }
    } while (true);

    // Hook eglSwapBuffers for render sync
    void *eglHandle = dlopen("libEGL.so", RTLD_LAZY);
    if (eglHandle) {
        void *sym = dlsym(eglHandle, "eglSwapBuffers");
        if (sym) {
            DobbyHook(sym, (void*)hooked_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
            LOGI("eglSwapBuffers hooked successfully");
        }
    }

    // Hook AInputQueue_getEvent for stable touch
    void *libAndroid = dlopen("libandroid.so", RTLD_LAZY);
    if (libAndroid) {
        void *symEvent = dlsym(libAndroid, "AInputQueue_getEvent");
        if (symEvent) {
            DobbyHook(symEvent, (void*)hooked_AInputQueue_getEvent, (void**)&orig_AInputQueue_getEvent);
            LOGI("AInputQueue_getEvent hooked successfully");
        } else {
            LOGE("Failed to resolve AInputQueue_getEvent");
        }
    }

    // Mark ready
    g_imguiReady = true;
    LOGI("Input & Render Hooks Active!");
    return nullptr;
}

// --- JNI Screen Sync Helper ---
int get_unity_screen_width(JNIEnv *env, jobject context) {
    jclass screenClass = env->FindClass("android/util/DisplayMetrics");
    if (screenClass) {
        // Fallback to real display metrics if Unity class missing
        jclass windowManager = env->FindClass("android/app/Activity");
        if (windowManager) {
            jmethodID getWindowManager = env->GetMethodID(windowManager, "getWindowManager", "()Landroid/view/WindowManager;");
            jmethodID getDefaultDisplay = env->GetMethodID(env->FindClass("android/view/WindowManager"), "getDefaultDisplay", "()Landroid/view/Display;");
            jmethodID getMetrics = env->GetMethodID(env->FindClass("android/view/Display"), "getMetrics", "(Landroid/util/DisplayMetrics;)V");
            
            jobject wm = env->CallObjectMethod(context, getWindowManager);
            jobject display = env->CallObjectMethod(wm, getDefaultDisplay);
            jobject metrics = env->NewObject(screenClass, env->GetMethodID(screenClass, "<init>", "()V"));
            env->CallVoidMethod(display, getMetrics, metrics);
            
            jfieldID w = env->GetFieldID(screenClass, "widthPixels", "I");
            jfieldID h = env->GetFieldID(screenClass, "heightPixels", "I");
            jint wVal = env->GetIntField(metrics, w);
            jint hVal = env->GetIntField(metrics, h);
            
            g_displayWidth = (float)wVal;
            g_displayHeight = (float)hVal;
            return wVal;
        }
    }
    return 1920;
}

int get_unity_screen_height(JNIEnv *env, jobject context) {
    return (int)g_displayHeight;
}