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
#include <android/log.h>
#include <android/input.h>

// ImGui
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"

// Libraries
#include "KittyMemory/KittyMemory.h"
#include "KittyMemory/MemoryPatch.h"
#include "KittyMemory/KittyScanner.h"
#include "KittyMemory/KittyUtils.h"
#include "Includes/Dobby/dobby.h"

// Project Headers
#include "hook.h"
#include "menu.h"
#include "functions.h"
#include "Misc.h"
#include "Include/Roboto-Regular.h"

// ✅ Correct Package Name for Among Us
#define GamePackageName "com.innersloth.spacemafia"

// Global State
int glHeight = 0, glWidth = 0;
bool setupimg = false;

// --- Safe Input Stubs ---
typedef int (*AInputQueue_getEvent_t)(AInputQueue* queue, AInputEvent** outEvent);
AInputQueue_getEvent_t orig_AInputQueue_getEvent = nullptr;

// Input Hook Handler (Safe version)
int hooked_AInputQueue_getEvent(AInputQueue* queue, AInputEvent** outEvent) {
    int result = 0;
    // ✅ FIX: Check if original function is valid before calling
    if (orig_AInputQueue_getEvent) {
        result = orig_AInputQueue_getEvent(queue, outEvent);
    }
    
    if (result >= 0 && *outEvent != nullptr) {
        ImGui_ImplAndroid_HandleInputEvent(*outEvent);
    }
    return result;
}

// EGL Swap Buffers Hook (Render Loop)
EGLBoolean (*old_eglSwapBuffers)(EGLDisplay, EGLSurface);
EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    // ✅ FIX 1: Strict Null Check to prevent UnityGfxDeviceW crash
    if (dpy == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) {
        if (old_eglSwapBuffers) return old_eglSwapBuffers(dpy, surface);
        return 0;
    }

    if (!setupimg) {
        // ✅ FIX 2: Initialize Menu ONLY if valid surface exists
        EGLBoolean w = eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
        EGLBoolean h = eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

        if (w && h && (glWidth > 0 && glHeight > 0)) {
            InitMenu();
            setupimg = true;
            LOGI("Menu Init Success | GL: %dx%d", glWidth, glHeight);
        }
    }

    ImGuiIO &io = ImGui::GetIO();
    
    // ✅ FIX 3: Ensure valid display size
    if (glWidth > 0 && glHeight > 0) {
        io.DisplaySize = ImVec2((float)glWidth, (float)glHeight);
    }
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    if (menuVisible) {
        RenderMenu();
    }

    ImGui::Render();
    glViewport(0, 0, glWidth, glHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); 
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // ✅ FIX 4: Safe call to original function
    if (old_eglSwapBuffers) {
        return old_eglSwapBuffers(dpy, surface);
    }
    return 0;
}

// Game Detection
int isGame(JNIEnv *env, jstring appDataDir) {
    if (!appDataDir) return 0;
    const char *app_data_dir = env->GetStringUTFChars(appDataDir, nullptr);
    int user = 0;
    static char package_name[256];
    
    if (sscanf(app_data_dir, "/data/%*[^/]/%d/%s", &user, package_name) != 2) {
        if (sscanf(app_data_dir, "/data/%*[^/]/%s", package_name) != 1) {
            package_name[0] = '\0';
            LOGW(OBFUSCATE("can't parse %s"), app_data_dir);
            env->ReleaseStringUTFChars(appDataDir, app_data_dir);
            return 0;
        }
    }
    if (strcmp(package_name, GamePackageName) == 0) {
        LOGI(OBFUSCATE("detect game: %s"), package_name);
        game_data_dir = new char[strlen(app_data_dir) + 1];
        strcpy(game_data_dir, app_data_dir);
        env->ReleaseStringUTFChars(appDataDir, app_data_dir);
        return 1;
    }
    env->ReleaseStringUTFChars(appDataDir, app_data_dir);
    return 0;
}

// Main Hook Thread
void *hack_thread(void *arg) {
    // Wait for libil2cpp.so
    do {
        sleep(1);
        KittyMemory::ProcMap il2cppMap = KittyMemory::getLibraryBaseMap("libil2cpp.so");
        
        if (il2cppMap.isValid()) {
            uintptr_t il2cppBase = static_cast<uintptr_t>(il2cppMap.startAddress);
            g_il2cppBaseMap = il2cppMap;
            LOGI("il2cpp base: %p", (void*)il2cppBase);
            break;
        }
    } while (true);

    Pointers();
    Hooks();

    // ✅ FIX 5: Hook from libEGL.so (Correct Library)
    void *eglhandle = dlopen("libEGL.so", RTLD_LAZY);
    void *symEgl = NULL;
    
    if (eglhandle) {
        symEgl = dlsym(eglhandle, "eglSwapBuffers");
        // Do not close handle here, we need Dobby to access it
    }
    
    if (symEgl) {
        DobbyHook(symEgl, (void*)hook_eglSwapBuffers, (void**)&old_eglSwapBuffers);
        LOGI("eglSwapBuffers hooked from libEGL!");
    } else {
        LOGE("Failed to find eglSwapBuffers in libEGL!");
    }

    // 2. Hook AInputQueue (Input) - Safer than InputConsumer
    void *libAndroid = dlopen("libandroid.so", RTLD_LAZY);
    if (libAndroid) {
        void *symInput = dlsym(libAndroid, "AInputQueue_getEvent");
        if (symInput) {
            DobbyHook(symInput, (void *)hooked_AInputQueue_getEvent, (void **)&orig_AInputQueue_getEvent);
            LOGI("AInputQueue_getEvent hooked!");
        }
    }

    LOGI("Hook thread completed successfully!");
    return nullptr;
}