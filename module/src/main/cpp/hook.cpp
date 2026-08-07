#define _GNU_SOURCE
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <dlfcn.h>
#include <cstdlib>
#include <cinttypes>
#include <string>
#include <vector>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"
#include "KittyMemory/KittyMemory.h"
#include "KittyMemory/MemoryPatch.h"
#include "KittyMemory/KittyScanner.h"
#include "KittyMemory/KittyUtils.h"
#include "Includes/Dobby/dobby.h"
#include "Misc.h"
#include "hook.h"
#include "functions.h"
#include "menu.h"
#include "il2cpp.h"

#define GamePackageName "com.innersloth.spacemafia"

ProcMap g_il2cppBaseMap;
int glWidth, glHeight;
bool setupimg = false;

void DumpAvailableSymbols() {
    LOGI("=== Dumping available symbols ===");
    
    // Check which libraries are loaded
    const char* libs[] = {"libunity.so", "libEGL.so", "libGLESv2.so", "libvulkan.so", "libnativewindow.so"};
    for (auto lib : libs) {
        auto handle = dlopen(lib, RTLD_LAZY | RTLD_NOLOAD);
        if (handle) {
            LOGI("Library loaded: %s", lib);
            dlclose(handle);
        } else {
            LOGI("Library NOT loaded: %s", lib);
        }
    }
    
    // Check specific symbols
    const char* symbols[] = {
        "eglSwapBuffers", "glDrawElements", "glDrawArrays",
        "vkQueuePresentKHR", "vkQueueSubmit", "vkAcquireNextImageKHR",
        "ANativeWindow_lock", "eglMakeCurrent"
    };
    
    for (auto sym : symbols) {
        auto ptr = dlsym(RTLD_DEFAULT, sym);
        if (ptr) {
            LOGI("Symbol found: %s at %p", sym, ptr);
        } else {
            LOGI("Symbol NOT found: %s", sym);
        }
    }
    
    LOGI("=== Dump complete ===");
}

int isGame(JNIEnv *env, jstring appDataDir)
{
    if (!appDataDir)
        return 0;
    const char *app_data_dir = env->GetStringUTFChars(appDataDir, nullptr);
    int user = 0;
    static char package_name[256];
    if (sscanf(app_data_dir, "/data/%*[^/]/%d/%s", &user, package_name) != 2) {
        if (sscanf(app_data_dir, "/data/%*[^/]/%s", package_name) != 1) {
            package_name[0] = '\0';
            LOGW("can't parse %s", app_data_dir);
            return 0;
        }
    }
    if (strcmp(package_name, GamePackageName) == 0) {
        LOGI("detect game: %s", package_name);
        game_data_dir = new char[strlen(app_data_dir) + 1];
        strcpy(game_data_dir, app_data_dir);
        env->ReleaseStringUTFChars(appDataDir, app_data_dir);
        return 1;
    } else {
        env->ReleaseStringUTFChars(appDataDir, app_data_dir);
        return 0;
    }
}

void *hack_thread(void *arg) {
    DumpAvailableSymbols();  // <-- ADD THIS LINE
    
    do {
        sleep(1);
        g_il2cppBaseMap = KittyMemory::getLibraryBaseMap("libil2cpp.so");
    } while (!g_il2cppBaseMap.isValid());
    LOGI("il2cpp base: %p", (void*)(g_il2cppBaseMap.startAddress));

    std::thread([]() {
        sleep(20);
        IL2CPP::Init();
        for (int i = 0; i < 30; i++) {
            if (IL2CPP::API::il2cpp_domain_get != nullptr) {
                IL2CPP::domain = IL2CPP::API::il2cpp_domain_get();
                if (IL2CPP::domain != nullptr) {
                    LOGI("KenzGUI: IL2CPP Domain obtained: %p", IL2CPP::domain);
                    IL2CPP::Attach();
                    return;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        LOGW("KenzGUI: Failed to get IL2CPP domain");
    }).detach();

    // Try Vulkan first (64-bit Unity games)
    auto vkQueuePresentKHR = dlsym(RTLD_NEXT, "vkQueuePresentKHR");
    if (vkQueuePresentKHR) {
        DobbyHook((void*)vkQueuePresentKHR, (void*)hook_vkQueuePresentKHR,
                  (void**)&old_vkQueuePresentKHR);
        LOGI("vkQueuePresentKHR hooked via RTLD_NEXT");
    } else {
        LOGW("Vulkan not found - trying OpenGL");
        auto glDrawElements = dlsym(RTLD_NEXT, "glDrawElements");
        if (glDrawElements) {
            DobbyHook((void*)glDrawElements, (void*)hook_glDrawElements,
                      (void**)&old_glDrawElements);
            LOGI("glDrawElements hooked via RTLD_NEXT");
        } else {
            LOGW("glDrawElements not found - trying eglSwapBuffers");
            auto eglSwapBuffers = dlsym(RTLD_NEXT, "eglSwapBuffers");
            if (eglSwapBuffers) {
                DobbyHook((void*)eglSwapBuffers, (void*)hook_eglSwapBuffers,
                          (void**)&old_eglSwapBuffers);
                LOGI("eglSwapBuffers hooked via RTLD_NEXT (fallback)");
            }
        }
    }

    LOGI("Draw Done!");
    return nullptr;
}