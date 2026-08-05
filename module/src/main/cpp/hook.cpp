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
#include "dobby.h"

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

KittyMemory::ProcMap g_il2cppBaseMap;

// Graphics API detection
enum GraphicsAPI { API_UNKNOWN, API_OPENGL_ES, API_VULKAN };
static GraphicsAPI g_graphicsAPI = API_UNKNOWN;

// ---------------------------------------------------------------------------
//  Function bodies
// ---------------------------------------------------------------------------
bool stopZ = false;

void Pointers() {
    LOGI("Pointers() called");
}

void Patches() {
    LOGI("Patches() called");
}

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
//  OpenGL ES hook (eglSwapBuffers)
// ---------------------------------------------------------------------------
static EGLBoolean (*old_eglSwapBuffers)(EGLDisplay, EGLSurface) = nullptr;

EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    LOGI("hook_eglSwapBuffers called: dpy=%p, surface=%p, setupimg=%d", dpy, surface, setupimg);

    if (dpy == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) {
        return old_eglSwapBuffers ? old_eglSwapBuffers(dpy, surface) : EGL_FALSE;
    }

    if (!setupimg) {
        EGLint w = 0, h = 0;
        if (eglQuerySurface(dpy, surface, EGL_WIDTH, &w) &&
            eglQuerySurface(dpy, surface, EGL_HEIGHT, &h) &&
            w > 0 && h > 0) {
            glWidth  = w;
            glHeight = h;
            LOGI("hook_eglSwapBuffers: surface size %dx%d, calling InitMenu()", glWidth, glHeight);
            InitMenu();
            setupimg = true;
            LOGI("ImGui init OK – %dx%d", glWidth, glHeight);
        } else {
            LOGW("hook_eglSwapBuffers: eglQuerySurface failed or invalid size");
        }
    }

    if (!setupimg) {
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
//  Vulkan hook (vkQueuePresentKHR) – placeholder
// ---------------------------------------------------------------------------
// You'll need to include vulkan headers and define the function pointer types.
// For now, we just log that Vulkan is detected.
/*
static VkResult (*old_vkQueuePresentKHR)(VkQueue, const VkPresentInfoKHR*) = nullptr;

VkResult hook_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    LOGI("hook_vkQueuePresentKHR called");
    // ImGui rendering for Vulkan would go here (using ImGui_ImplVulkan)
    return old_vkQueuePresentKHR(queue, pPresentInfo);
}
*/

// ---------------------------------------------------------------------------
//  Detect graphics API by checking loaded libraries
// ---------------------------------------------------------------------------
static void detectGraphicsAPI() {
    if (KittyMemory::getLibraryBaseMap("libvulkan.so").isValid()) {
        g_graphicsAPI = API_VULKAN;
        LOGI("Graphics API: Vulkan");
    } else if (KittyMemory::getLibraryBaseMap("libGLESv2.so").isValid()) {
        g_graphicsAPI = API_OPENGL_ES;
        LOGI("Graphics API: OpenGL ES");
    } else {
        g_graphicsAPI = API_UNKNOWN;
        LOGW("Graphics API: Unknown");
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

    // Wait for libil2cpp.so
    while (true) {
        sleep(1);
        KittyMemory::ProcMap map = KittyMemory::getLibraryBaseMap("libil2cpp.so");
        if (map.isValid()) {
            g_il2cppBaseMap = map;
            LOGI("libil2cpp.so @ %p", (void*)map.startAddress);
            break;
        }
    }

    // Detect graphics API
    detectGraphicsAPI();

    // Hook based on detected API
    if (g_graphicsAPI == API_OPENGL_ES) {
        // Wait for libEGL.so and hook eglSwapBuffers using Dobby
        while (true) {
            sleep(1);
            KittyMemory::ProcMap eglMap = KittyMemory::getLibraryBaseMap("libEGL.so");
            if (eglMap.isValid()) {
                LOGI("libEGL.so @ %p", (void*)eglMap.startAddress);
                void* eglSwapBuffersAddr = dlsym(RTLD_DEFAULT, "eglSwapBuffers");
                if (eglSwapBuffersAddr) {
                    LOGI("eglSwapBuffers address: %p", eglSwapBuffersAddr);
                    int ret = DobbyHook(eglSwapBuffersAddr,
                                        reinterpret_cast<void*>(hook_eglSwapBuffers),
                                        reinterpret_cast<void**>(&old_eglSwapBuffers));
                    if (ret == 0) {
                        LOGI("Dobby hook on eglSwapBuffers succeeded");
                    } else {
                        LOGE("Dobby hook on eglSwapBuffers failed with code %d", ret);
                    }
                } else {
                    LOGE("dlsym failed to find eglSwapBuffers: %s", dlerror());
                }
                break;
            }
        }
    } else if (g_graphicsAPI == API_VULKAN) {
        LOGI("Vulkan detected – Vulkan hook not yet implemented");
        // TODO: Implement Vulkan hook (vkQueuePresentKHR)
    } else {
        LOGE("Unknown graphics API – cannot hook rendering");
    }

    Pointers();
    Hooks();

    LOGI("hack_thread finished");
    return nullptr;
}