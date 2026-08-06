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
#include <android_native_app_glue.h>   // for android_app struct

#include <android/log.h>
#include <pthread.h>

#include "dobby.h"

// ImGui
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
int enable_hack = 0;
char* game_data_dir = nullptr;
KittyMemory::ProcMap g_il2cppBaseMap;
bool setupimg = false;

// OpenGL state
static EGLDisplay g_eglDisplay = EGL_NO_DISPLAY;
static EGLSurface g_eglSurface = EGL_NO_SURFACE;

// Touch input state
static struct android_app* g_app = nullptr;
static int32_t (*old_onInputEvent)(struct android_app* app, AInputEvent* event);

// ---------------------------------------------------------------------------
//  Function bodies
// ---------------------------------------------------------------------------
bool stopZ = false;

void Pointers() { LOGI("Pointers() called"); }
void Patches() { LOGI("Patches() called"); }

void InitWorker() {
    LOGI("InitWorker() called");
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
//  Touch input hook (replaces android_app::onInputEvent)
// ---------------------------------------------------------------------------
int32_t hook_onInputEvent(struct android_app* app, AInputEvent* event) {
    // Forward event to ImGui
    if (ImGui_ImplAndroid_HandleInputEvent(event)) {
        // ImGui consumed the event – block game from receiving it
        return 1;
    }
    // Otherwise, let the game handle it
    return old_onInputEvent(app, event);
}

// ---------------------------------------------------------------------------
//  Hook android_main to capture android_app pointer and replace onInputEvent
// ---------------------------------------------------------------------------
static void (*old_android_main)(struct android_app* app);
void hook_android_main(struct android_app* app) {
    LOGI("android_main hooked, capturing app=%p", (void*)app);
    g_app = app;
    // Save original onInputEvent
    old_onInputEvent = app->onInputEvent;
    // Replace with our hook
    app->onInputEvent = hook_onInputEvent;
    // Call original main
    old_android_main(app);
}

// ---------------------------------------------------------------------------
//  OpenGL ES hook: eglSwapBuffers
// ---------------------------------------------------------------------------
static EGLBoolean (*old_eglSwapBuffers)(EGLDisplay, EGLSurface) = nullptr;

EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    // Store display/surface for later use
    if (g_eglDisplay == EGL_NO_DISPLAY) {
        g_eglDisplay = dpy;
        g_eglSurface = surface;
    }

    // Initialize ImGui on first call
    if (!setupimg) {
        LOGI("Initializing ImGui for OpenGL ES");

        // Get screen size from EGL
        EGLint w, h;
        eglQuerySurface(dpy, surface, EGL_WIDTH, &w);
        eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)w, (float)h);
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
        io.Fonts->AddFontDefault();
        ImGui::StyleColorsDark();

        // Init OpenGL3 backend
        if (ImGui_ImplOpenGL3_Init("#version 300 es")) {
            // Init Android backend (handles touch input via AInputEvent)
            ImGui_ImplAndroid_Init(nullptr);
            setupimg = true;
            LOGI("ImGui OpenGL ES init OK");
        } else {
            LOGE("ImGui_ImplOpenGL3_Init failed");
        }
    }

    // Call original (game renders its frame)
    EGLBoolean result = old_eglSwapBuffers(dpy, surface);

    // Render ImGui overlay
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    if (menuVisible) {
        RenderMenu();
    }

    ImGui::Render();
    glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return result;
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

    // Hook android_main to capture app and replace onInputEvent
    while (true) {
        void* android_main_addr = dlsym(RTLD_DEFAULT, "android_main");
        if (android_main_addr) {
            DobbyHook(android_main_addr, (void*)hook_android_main, (void**)&old_android_main);
            LOGI("android_main hooked");
            break;
        }
        sleep(1);
    }

    // Hook eglSwapBuffers for rendering
    while (true) {
        void* eglSwapAddr = dlsym(RTLD_DEFAULT, "eglSwapBuffers");
        if (eglSwapAddr) {
            DobbyHook(eglSwapAddr, (void*)hook_eglSwapBuffers, (void**)&old_eglSwapBuffers);
            LOGI("eglSwapBuffers hooked");
            break;
        }
        sleep(1);
    }

    // Wait for libil2cpp.so (if needed for IL2CPP hooks)
    while (true) {
        sleep(1);
        KittyMemory::ProcMap map = KittyMemory::getLibraryBaseMap("libil2cpp.so");
        if (map.isValid()) {
            g_il2cppBaseMap = map;
            LOGI("libil2cpp.so @ %p", (void*)map.startAddress);
            break;
        }
    }

    Pointers();
    Hooks();

    LOGI("hack_thread finished");
    return nullptr;
}