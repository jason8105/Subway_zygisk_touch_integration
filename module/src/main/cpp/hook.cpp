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
#include <android/input.h>

#include "dobby.h"

// ImGui
#include "ImGui/imgui.h"
#include "ImGui/backends/imgui_impl_opengl3.h"
#include "ImGui/backends/imgui_impl_android.h"

#include "KittyMemory/KittyMemory.h"
#include "KittyMemory/KittyScanner.h"

// Project headers
#include "hook.h"
#include "menu.h"
#include "functions.h"
#include "Misc.h"
#include "zygisk.hpp"
#include "SetUp.h"

#define GamePackageName "com.innersloth.spacemafia"

// ---------------------------------------------------------------------------
//  Globals
// ---------------------------------------------------------------------------
int enable_hack = 0;
char* game_data_dir = nullptr;
bool setupimg = false;

// Touch input hooks
static int32_t (*old_AInputQueue_getEvent)(AInputQueue* queue, AInputEvent** outEvent);
static void    (*old_AInputQueue_finishEvent)(AInputQueue* queue, AInputEvent* event, int32_t handled);

// ---------------------------------------------------------------------------
//  Function bodies
// ---------------------------------------------------------------------------
bool stopZ = false;

void Pointers() { LOGI("Pointers() called"); }
void Patches() { LOGI("Patches() called"); }

void InitWorker() {
    LOGI("InitWorker() called");
    // Wait for il2cpp_base
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
}

// ---------------------------------------------------------------------------
//  Universal touch input hook
// ---------------------------------------------------------------------------
int32_t hook_AInputQueue_getEvent(AInputQueue* queue, AInputEvent** outEvent) {
    int32_t result = old_AInputQueue_getEvent(queue, outEvent);
    if (result >= 0 && *outEvent != nullptr) {
        ImGui_ImplAndroid_HandleInputEvent(*outEvent);
    }
    return result;
}

void hook_AInputQueue_finishEvent(AInputQueue* queue, AInputEvent* event, int32_t handled) {
    old_AInputQueue_finishEvent(queue, event, handled);
}

// ---------------------------------------------------------------------------
//  OpenGL ES hook: eglSwapBuffers (KenzGUI method)
// ---------------------------------------------------------------------------
static EGLBoolean (*old_eglSwapBuffers)(EGLDisplay, EGLSurface) = nullptr;

EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    if (dpy == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) {
        if (old_eglSwapBuffers) return old_eglSwapBuffers(dpy, surface);
        return EGL_FALSE;
    }

// In hook_eglSwapBuffers, change:
// In hook_eglSwapBuffers, change the init block:
if (!setupimg) {
    LOGI("Initializing ImGui using KenzGUI method");
    SetGUI(dpy, surface);  // <-- pass EGL display/surface
    ImGui_ImplAndroid_Init(nullptr);
    setupimg = true;
    LOGI("ImGui init OK");
}

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
    if (menuVisible) RenderMenu();
    ImGui::Render();
    
    glViewport(0, 0, static_cast<GLsizei>(ImGui::GetIO().DisplaySize.x),
               static_cast<GLsizei>(ImGui::GetIO().DisplaySize.y));
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return old_eglSwapBuffers ? old_eglSwapBuffers(dpy, surface) : EGL_FALSE;
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
        if (game_data_dir) delete[] game_data_dir;
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

    // Install touch hooks (universal)
    void* getEventAddr = dlsym(RTLD_DEFAULT, "AInputQueue_getEvent");
    void* finishEventAddr = dlsym(RTLD_DEFAULT, "AInputQueue_finishEvent");
    if (getEventAddr && finishEventAddr) {
        DobbyHook(getEventAddr, (void*)hook_AInputQueue_getEvent, (void**)&old_AInputQueue_getEvent);
        DobbyHook(finishEventAddr, (void*)hook_AInputQueue_finishEvent, (void**)&old_AInputQueue_finishEvent);
        LOGI("AInputQueue hooks installed");
    } else {
        LOGW("AInputQueue symbols not found, touch may not work");
    }

    // Wait for libil2cpp.so
    while (true) {
        sleep(1);
        KittyMemory::ProcMap map = KittyMemory::getLibraryBaseMap("libil2cpp.so");
        if (map.isValid()) {
            LOGI("libil2cpp.so found @ %p", (void*)map.startAddress);
            break;
        }
    }

    // Attach to IL2CPP so SetGUI() can call Unity methods
    InitWorker();

    // Hook eglSwapBuffers - works on both OpenGL and Vulkan games
    while (true) {
        sleep(1);
        void* eglSwapAddr = dlsym(RTLD_DEFAULT, "eglSwapBuffers");
        if (eglSwapAddr) {
            LOGI("Found eglSwapBuffers at %p, hooking...", eglSwapAddr);
            DobbyHook(eglSwapAddr, (void*)hook_eglSwapBuffers, (void**)&old_eglSwapBuffers);
            LOGI("eglSwapBuffers hooked successfully");
            break;
        } else {
            LOGW("eglSwapBuffers not found yet, retrying...");
        }
    }

    Pointers();
    Hooks();

    LOGI("hack_thread finished");
    return nullptr;
}