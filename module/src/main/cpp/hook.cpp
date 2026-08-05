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
#include "xdl.h"

// ✅ Includes
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"
#include "KittyMemory/KittyMemory.h"
#include "KittyMemory/KittyScanner.h"
#include "hook.h"
#include "menu.h"
#include "functions.h"
#include "Misc.h"
#include "zygisk.hpp"

#define GamePackageName "com.innersloth.spacemafia"

// Global State
int glHeight = 0, glWidth = 0;
bool setupimg = false;
extern bool menuVisible;

// ============================================================================
// 🔧 FIX: Function Implementations (Previously missing -> Linker Error)
// ============================================================================

bool stopZ = false;
void Pointers() {}
void Patches() {}

void InitWorker() {
    while (!IL2CPP::il2cpp_base) {
        if (IL2CPP::Init()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    while (!IL2CPP::domain) {
        if (IL2CPP::API::il2cpp_domain_get != nullptr) {
            IL2CPP::domain = IL2CPP::API::il2cpp_domain_get();
        }
        if (IL2CPP::domain) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    if (IL2CPP::domain) IL2CPP::Attach();
}

void Hooks() {
    // No more thread.detach(). IL2CPP attach synchronously handle karo.
    if (IL2CPP::domain) IL2CPP::Attach();
}

// ============================================================================
// 🔚 Render Hook (PLT Hooking - Crash Fixed)
// ============================================================================

EGLBoolean (*old_eglSwapBuffers)(EGLDisplay, EGLSurface);

EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    if (dpy == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) {
        if (old_eglSwapBuffers) return old_eglSwapBuffers(dpy, surface);
        return EGL_FALSE;
    }

    if (!setupimg) {
        EGLBoolean w = eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
        EGLBoolean h = eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);
        if (w && h && glWidth > 0 && glHeight > 0) {
            InitMenu();
            setupimg = true;
            LOGI("Menu Init Success | GL: %dx%d", glWidth, glHeight);
        }
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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (old_eglSwapBuffers) return old_eglSwapBuffers(dpy, surface);
    return EGL_FALSE;
}

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

void *hack_thread(void *arg) {
    zygisk::Api *api = (zygisk::Api *)arg;

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

    // ✅ FIX: Use Zygisk PLT Hooking (Safe, No Register Corruption)
    api->pltHookRegister("libEGL", "eglSwapBuffers", (void*)hook_eglSwapBuffers, (void**)&old_eglSwapBuffers);
    
    if (api->pltHookCommit()) {
        LOGI("✅ PLT Hook Applied Successfully! (No SIGABRT)");
    } else {
        LOGE("❌ PLT Hook Commit Failed");
    }

    LOGI("Hook thread completed successfully!");
    return nullptr;
}