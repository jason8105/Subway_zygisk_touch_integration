#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <dlfcn.h>
#include <cstdlib>
#include <cinttypes>
#include <string>
#include <thread>
#include <chrono>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/log.h>
#include <pthread.h>

#include "xdl.h"

// ImGui & KittyMemory
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"
#include "KittyMemory/KittyMemory.h"
#include "KittyMemory/KittyScanner.h"

// Project headers
#include "hook.h"
#include "menu.h"
#include "functions.h"
#include "Misc.h"
#include "zygisk.hpp"

#define GamePackageName "com.innersloth.spacemafia"

// ---------------------------------------------------------------------------
//  Globals
// ---------------------------------------------------------------------------
int glHeight = 0, glWidth = 0;
bool setupimg = false;

int enable_hack = 0;
char* game_data_dir = nullptr;

KittyMemory::ProcMap g_il2cppBaseMap;    // used by Misc.h helpers

// ---------------------------------------------------------------------------
//  Function bodies
// ---------------------------------------------------------------------------
bool stopZ = false;                      // definition for the extern in functions.h

void Pointers() {
    LOGI("Pointers() called");
    // Resolve IL2CPP method pointers here if you need them.
}

void Patches() {
    LOGI("Patches() called");
    // Apply any memory patches here.
}

void InitWorker() {
    LOGI("InitWorker() called");
    // Wait until IL2CPP is ready.
    while (!IL2CPP::il2cpp_base) {
        if (IL2CPP::Init()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    while (!IL2CPP::domain) {
        if (IL2CPP::API::il2cpp_domain_get) {
            IL2CPP::domain = IL2CPP::API::il2cpp_domain_get();
        }
        if (IL2CPP::domain) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    if (IL2CPP::domain) IL2CPP::Attach();
}

void Hooks() {
    LOGI("Hooks() called");
    if (IL2CPP::domain) IL2CPP::Attach();
    // Install your Dobby / IL2CPP hooks here.
}

// ---------------------------------------------------------------------------
//  eglSwapBuffers PLT hook (safe, no register corruption)
// ---------------------------------------------------------------------------
static EGLBoolean (*old_eglSwapBuffers)(EGLDisplay, EGLSurface) = nullptr;

EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    // Log every call to see if the hook is actually triggered
    LOGI("hook_eglSwapBuffers called: dpy=%p, surface=%p, setupimg=%d", dpy, surface, setupimg);

    if (dpy == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) {
        LOGW("hook_eglSwapBuffers: invalid params");
        return old_eglSwapBuffers ? old_eglSwapBuffers(dpy, surface) : EGL_FALSE;
    }

    // One‑time ImGui initialisation
    if (!setupimg) {
        EGLint w = 0, h = 0;
        if (eglQuerySurface(dpy, surface, EGL_WIDTH, &w) &&
            eglQuerySurface(dpy, surface, EGL_HEIGHT, &h) &&
            w > 0 && h > 0) {
            glWidth  = w;
            glHeight = h;
            LOGI("hook_eglSwapBuffers: surface size %dx%d, calling InitMenu()", glWidth, glHeight);
            InitMenu();                     // creates ImGui context
            setupimg = true;
            __android_log_print(ANDROID_LOG_INFO, "zyCheats",
                                "ImGui init OK – %dx%d", glWidth, glHeight);
        } else {
            LOGW("hook_eglSwapBuffers: eglQuerySurface failed or invalid size");
        }
    }

    if (!setupimg) {
        // If still not initialized, just pass through
        return old_eglSwapBuffers ? old_eglSwapBuffers(dpy, surface) : EGL_FALSE;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0 || io.DisplaySize.y <= 0)
        io.DisplaySize = ImVec2(static_cast<float>(glWidth),
                                static_cast<float>(glHeight));

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    if (menuVisible) {
        LOGI("hook_eglSwapBuffers: rendering menu");
        RenderMenu();
    }

    ImGui::Render();
    glViewport(0, 0, static_cast<GLsizei>(io.DisplaySize.x),
               static_cast<GLsizei>(io.DisplaySize.y));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return old_eglSwapBuffers ? old_eglSwapBuffers(dpy, surface) : EGL_FALSE;
}

// ---------------------------------------------------------------------------
//  PLT hook registration (called from preAppSpecialize)
// ---------------------------------------------------------------------------
void registerPltHook(zygisk::Api* api) {
    LOGI("registerPltHook called with api=%p", (void*)api);
    if (!api) return;
    // Try with exact library name (no regex dot escape needed for literal dot)
    api->pltHookRegister("libEGL.so", "eglSwapBuffers",
                         reinterpret_cast<void*>(hook_eglSwapBuffers),
                         reinterpret_cast<void**>(&old_eglSwapBuffers));
    if (api->pltHookCommit()) {
        LOGI("PLT hook committed in registerPltHook");
    } else {
        LOGE("PLT hook commit failed in registerPltHook");
    }
}

// ---------------------------------------------------------------------------
//  Helper used by Zygisk entry point
// ---------------------------------------------------------------------------
int isGame(JNIEnv* env, jstring appDataDir) {
    if (!appDataDir) {
        LOGW("isGame: appDataDir is null");
        return 0;
    }
    const char* dir = env->GetStringUTFChars(appDataDir, nullptr);
    LOGI("isGame: full dir=%s", dir);

    int user = 0;
    char pkg[256] = {0};

    // /data/user/0/com.pkg  OR  /data/data/com.pkg
    if (sscanf(dir, "/data/%*[^/]/%d/%255s", &user, pkg) != 2) {
        if (sscanf(dir, "/data/%*[^/]/%255s", pkg) != 1) {
            LOGW("isGame: failed to parse package from path: %s", dir);
            env->ReleaseStringUTFChars(appDataDir, dir);
            return 0;
        }
    }
    env->ReleaseStringUTFChars(appDataDir, dir);

    LOGI("isGame: parsed pkg=%s", pkg);
    if (strcmp(pkg, GamePackageName) == 0) {
        LOGI("isGame: match! package=%s", pkg);
        delete[] game_data_dir;
        game_data_dir = new char[strlen(dir) + 1];
        strcpy(game_data_dir, dir);
        return 1;
    }
    LOGI("isGame: no match, pkg=%s", pkg);
    return 0;
}

// ---------------------------------------------------------------------------
//  Thread started from postAppSpecialize
// ---------------------------------------------------------------------------
void* hack_thread(void* /*arg*/) {
    LOGI("hack_thread started");
    // Wait for libil2cpp.so to be mapped
    while (true) {
        sleep(1);
        KittyMemory::ProcMap map = KittyMemory::getLibraryBaseMap("libil2cpp.so");
        if (map.isValid()) {
            g_il2cppBaseMap = map;
            __android_log_print(ANDROID_LOG_INFO, "zyCheats",
                                "libil2cpp.so @ %p", (void*)map.startAddress);
            break;
        }
    }

    Pointers();
    Hooks();

    // PLT hook already registered in preAppSpecialize – do not repeat here

    LOGI("hack_thread finished");
    return nullptr;
}