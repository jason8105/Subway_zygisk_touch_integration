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
bool menuVisible = true;                 // definition – menu.h only has extern
KittyMemory::ProcMap g_il2cppBaseMap;    // used by Misc.h helpers

// ---------------------------------------------------------------------------
//  Function bodies (previously empty – now minimal but functional)
// ---------------------------------------------------------------------------
bool stopZ = false;                      // definition for the extern in functions.h

void Pointers() {
    // Resolve IL2CPP method pointers here if you need them.
}

void Patches() {
    // Apply any memory patches here.
}

void InitWorker() {
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
    if (IL2CPP::domain) IL2CPP::Attach();
    // Install your Dobby / PLT hooks here.
}

// ---------------------------------------------------------------------------
//  eglSwapBuffers PLT hook (safe, no register corruption)
// ---------------------------------------------------------------------------
static EGLBoolean (*old_eglSwapBuffers)(EGLDisplay, EGLSurface) = nullptr;

EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    if (dpy == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) {
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
            InitMenu();                     // creates ImGui context
            setupimg = true;
            __android_log_print(ANDROID_LOG_INFO, "zyCheats",
                                "ImGui init OK – %dx%d", glWidth, glHeight);
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0 || io.DisplaySize.y <= 0)
        io.DisplaySize = ImVec2(static_cast<float>(glWidth),
                                static_cast<float>(glHeight));

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    if (menuVisible) RenderMenu();

    ImGui::Render();
    glViewport(0, 0, static_cast<GLsizei>(io.DisplaySize.x),
               static_cast<GLsizei>(io.DisplaySize.y));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return old_eglSwapBuffers ? old_eglSwapBuffers(dpy, surface) : EGL_FALSE;
}

// ---------------------------------------------------------------------------
//  Helper used by Zygisk entry point
// ---------------------------------------------------------------------------
int isGame(JNIEnv* env, jstring appDataDir) {
    if (!appDataDir) return 0;
    const char* dir = env->GetStringUTFChars(appDataDir, nullptr);
    int user = 0;
    char pkg[256] = {0};

    // /data/user/0/com.pkg  OR  /data/data/com.pkg
    if (sscanf(dir, "/data/%*[^/]/%d/%255s", &user, pkg) != 2) {
        if (sscanf(dir, "/data/%*[^/]/%255s", pkg) != 1) {
            env->ReleaseStringUTFChars(appDataDir, dir);
            return 0;
        }
    }
    env->ReleaseStringUTFChars(appDataDir, dir);

    if (strcmp(pkg, GamePackageName) == 0) {
        delete[] game_data_dir;
        game_data_dir = new char[strlen(dir) + 1];
        strcpy(game_data_dir, dir);
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
//  Thread started from postAppSpecialize
// ---------------------------------------------------------------------------
void* hack_thread(void* arg) {
    zygisk::Api* api = static_cast<zygisk::Api*>(arg);

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

    // Register the PLT hook via Zygisk (no Dobby needed for eglSwapBuffers)
    api->pltHookRegister("libEGL", "eglSwapBuffers",
                         reinterpret_cast<void*>(hook_eglSwapBuffers),
                         reinterpret_cast<void**>(&old_eglSwapBuffers));

    if (api->pltHookCommit()) {
        __android_log_print(ANDROID_LOG_INFO, "zyCheats",
                            "✅ PLT hook committed");
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "zyCheats",
                            "❌ PLT hook commit failed");
    }

    __android_log_print(ANDROID_LOG_INFO, "zyCheats",
                        "hack_thread finished");
    return nullptr;
}