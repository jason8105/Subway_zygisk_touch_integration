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

// ADD THIS LINE:
ProcMap g_il2cppBaseMap;   // <-- definition of the extern variable

extern int glWidth, glHeight;
extern bool setupimg;

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
    // Wait for libil2cpp.so
    do {
        sleep(1);
        g_il2cppBaseMap = KittyMemory::getLibraryBaseMap("libil2cpp.so");
    } while (!g_il2cppBaseMap.isValid());
    LOGI("il2cpp base: %p", (void*)(g_il2cppBaseMap.startAddress));
    
    // Start IL2CPP init in background (function pointers only, no domain)
    Pointers();
    Hooks();
    
    // Wait for IL2CPP domain to be ready (for KenzGUI method)
    // This runs in the background and doesn't block the overlay
    std::thread([]() {
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
        LOGW("KenzGUI: Failed to get IL2CPP domain - using EGL fallback");
    }).detach();
    
    // Hook eglSwapBuffers from libunity.so
    auto eglhandle = dlopen("libunity.so", RTLD_LAZY);
    auto eglSwapBuffers = dlsym(eglhandle, "eglSwapBuffers");
    if (eglSwapBuffers) {
        DobbyHook((void*)eglSwapBuffers, (void*)hook_eglSwapBuffers,
                  (void**)&old_eglSwapBuffers);
        LOGI("eglSwapBuffers hooked");
    } else {
        auto eglhandle2 = dlopen("libEGL.so", RTLD_LAZY);
        auto eglSwapBuffers2 = dlsym(eglhandle2, "eglSwapBuffers");
        if (eglSwapBuffers2) {
            DobbyHook((void*)eglSwapBuffers2, (void*)hook_eglSwapBuffers,
                      (void**)&old_eglSwapBuffers);
            LOGI("eglSwapBuffers hooked from libEGL.so");
        }
    }
    
    LOGI("Draw Done!");
    return nullptr;
}