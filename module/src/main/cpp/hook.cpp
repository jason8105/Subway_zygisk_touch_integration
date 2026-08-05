#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <dlfcn.h>
#include <cstdlib>
#include <cinttypes>
#include <string>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/log.h>
#include <pthread.h>

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"
#include "KittyMemory/KittyMemory.h"
#include "KittyMemory/KittyScanner.h"
#include "hook.h"
#include "menu.h"
#include "functions.h"
#include "Misc.h"

#define GamePackageName "com.innersloth.spacemafia"

int glHeight = 0, glWidth = 0;
bool setupimg = false;
extern bool menuVisible;

// EGL Swap Buffers Hook (Render Loop)
EGLBoolean (*old_eglSwapBuffers)(EGLDisplay, EGLSurface);

EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    // ✅ SAFETY CHECK: Crash fix for 0x30
    if (dpy == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) {
        if (old_eglSwapBuffers) return old_eglSwapBuffers(dpy, surface);
        return EGL_FALSE;
    }

    if (!setupimg) {
        eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
        eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

        // Agar size 0 hai toh init mat karo
        if (glWidth == 0 || glHeight == 0) {
            return old_eglSwapBuffers(dpy, surface);
        }

        InitMenu();
        setupimg = true;
        LOGI("Menu Init Success | GL: %dx%d", glWidth, glHeight);
    }

    ImGuiIO &io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0 || io.DisplaySize.y <= 0) {
        io.DisplaySize = ImVec2((float)glWidth, (float)glHeight);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    if (menuVisible) RenderMenu();

    ImGui::Render();
    
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (old_eglSwapBuffers) return old_eglSwapBuffers(dpy, surface);
    return EGL_FALSE;
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
            env->ReleaseStringUTFChars(appDataDir, app_data_dir);
            return 0;
        }
    }
    if (strcmp(package_name, GamePackageName) == 0) {
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
    // ✅ API Pointer retrieve karna zaroori hai
    zygisk::Api *api = (zygisk::Api *)arg;

    // Wait for libil2cpp.so (For Game Logic Hooks later)
    do {
        sleep(1);
        KittyMemory::ProcMap il2cppMap = KittyMemory::getLibraryBaseMap("libil2cpp.so");
        if (il2cppMap.isValid()) {
            g_il2cppBaseMap = il2cppMap;
            KITTY_LOGI("il2cpp base: %p", (void*)il2cppMap.startAddress);
            break;
        }
    } while (true);

    Pointers();
    Hooks();

    // ✅ NEW METHOD: Use PLT Hook (Safe)
    // Yeh register corruption nahi karta, isliye crash nahi hoga
    if (api) {
        api->pltHookRegister("libEGL.so", "eglSwapBuffers", (void*)hook_eglSwapBuffers, (void**)&old_eglSwapBuffers);
        
        if (!api->pltHookCommit()) {
            LOGE("PLT Hook Commit Failed!");
        } else {
            LOGI("eglSwapBuffers hooked via PLT (Safe)!");
        }
    }

    LOGI("Hook thread completed successfully!");
    return nullptr;
}