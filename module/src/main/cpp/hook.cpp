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
#include "functions.h" // Include iske baad definition likhna
#include "Misc.h"
#include "zygisk.hpp" 

// ============================================================================
// 🔧 FIX: Definitions ab sirf yahan hongi (Function bodies)
// ============================================================================

bool stopZ = false;

void Pointers() {
    // Agar kuch pointers set karne hain toh yahan likho
}

void Patches() {
    // Patches ka code yahan
}

void InitWorker() {
    // 1. Wait safely for libil2cpp.so to load
    while (!IL2CPP::il2cpp_base) {
        if (IL2CPP::Init()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // 2. Wait safely for Unity to create the IL2CPP Domain
    while (!IL2CPP::domain) {
        if (IL2CPP::API::il2cpp_domain_get != nullptr) {
            Il2CppDomain* dom = IL2CPP::API::il2cpp_domain_get();
            if (dom) IL2CPP::domain = dom;
        }
        if (IL2CPP::domain) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (IL2CPP::domain) {
        IL2CPP::Attach();
    }
}

void Hooks() {
    std::thread(InitWorker).detach();
}

// ============================================================================
// 🔚 Yahan se aage tumhara baki ka XDL+Dobby ka code rahega
// ============================================================================

#define GamePackageName "com.innersloth.spacemafia"

// Global State
int glHeight = 0, glWidth = 0;
bool setupimg = false;
extern bool menuVisible;

// EGL Swap Buffers Hook (Render Loop)
EGLBoolean (*old_eglSwapBuffers)(EGLDisplay, EGLSurface);

EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    // Safety Check: Prevent 0x30 crash
    if (dpy == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) {
        if (old_eglSwapBuffers) return old_eglSwapBuffers(dpy, surface);
        return EGL_FALSE;
    }

    if (!setupimg) {
        eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
        eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

        // Agar size 0 hai toh init mat karo
        if (glWidth > 0 && glHeight > 0) {
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
    zygisk::Api *api = (zygisk::Api *)arg;

    // Wait for libil2cpp.so
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

    // ✅ Hook via XDL + Dobby (Bypasses PLT commit issue)
    void *libEGL = dlopen("libEGL.so", RTLD_LAZY);
    if (libEGL) {
        void *sym = xdl_sym(libEGL, "eglSwapBuffers", nullptr);
        if (sym) {
            if (DobbyHook(sym, (void*)hook_eglSwapBuffers, (void**)&old_eglSwapBuffers) == 0) {
                LOGI("eglSwapBuffers hooked via XDL+Dobby (libEGL.so)!");
            } else {
                LOGE("Dobby Hook Failed!");
            }
        } else {
            LOGE("eglSwapBuffers symbol not found in libEGL.so");
        }
    } else {
        LOGE("Failed to open libEGL.so");
    }

    LOGI("Hook thread completed successfully!");
    return nullptr;
}