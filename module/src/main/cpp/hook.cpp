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

// ✅ Among Us Package Name
#define GamePackageName "com.innersloth.spacemafia"

// Global State
int glHeight = 0, glWidth = 0;
bool setupimg = false;

// EGL Swap Buffers Hook (Render Loop)
EGLBoolean (*old_eglSwapBuffers)(EGLDisplay, EGLSurface);

EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    // ✅ FIX 1: Ensure we have a valid context and display
    if (dpy == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) {
        if (old_eglSwapBuffers) return old_eglSwapBuffers(dpy, surface);
        return 0;
    }

    if (!setupimg) {
        // ✅ FIX 2: Initialize Menu ONLY if valid surface dimensions are returned
        EGLBoolean w = eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
        EGLBoolean h = eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

        if (w && h && (glWidth > 0 && glHeight > 0)) {
            InitMenu();
            setupimg = true;
            LOGI("Menu Init Success | GL: %dx%d", glWidth, glHeight);
        }
    }

    ImGuiIO &io = ImGui::GetIO();
    
    // ✅ FIX 3: Ensure valid display size to prevent 0x0 draw calls
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
        // Do not close handle, Dobby needs it
    }
    
    if (symEgl) {
        DobbyHook(symEgl, (void*)hook_eglSwapBuffers, (void**)&old_eglSwapBuffers);
        LOGI("eglSwapBuffers hooked from libEGL!");
    } else {
        LOGE("Failed to find eglSwapBuffers in libEGL!");
    }

    // ❌ REMOVED: Input Hook (AInputQueue_getEvent)
    // Input hook was causing the main thread panic (0x135 crash) which cascaded to render crash.
    // We rely on standard Android input handling now to keep the game stable.

    LOGI("Hook thread completed successfully!");
    return nullptr;
}