#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <sys/system_properties.h>
#include <dlfcn.h>
#include <cstdlib>
#include <cinttypes>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/input.h>
#include <android/native_window.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"
#include "KittyMemory/KittyMemory.h"
#include "KittyMemory/MemoryPatch.h"
#include "KittyMemory/KittyScanner.h"
#include "KittyMemory/KittyUtils.h"
#include "Includes/Dobby/dobby.h"
#include "Include/Unity.h"
#include "Misc.h"
#include "hook.h"
#include "Include/Roboto-Regular.h"
#define GamePackageName "com.innersloth.spacemafia" // Define your package name here

int glHeight = 0, glWidth = 0;
int32_t screenWidth = 0;
int32_t screenHeight = 0;

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
            LOGW(OBFUSCATE("can't parse %s"), app_data_dir);
            return 0;
        }
    }
    if (strcmp(package_name, GamePackageName) == 0) {
        LOGI(OBFUSCATE("detect game: %s"), package_name);
        game_data_dir = new char[strlen(app_data_dir) + 1];
        strcpy(game_data_dir, app_data_dir);
        env->ReleaseStringUTFChars(appDataDir, app_data_dir);
        return 1;
    } else {
        env->ReleaseStringUTFChars(appDataDir, app_data_dir);
        return 0;
    }
}

bool setupimg = false;

// --- Dobby Touch & Resolution Hooks ---
int (*orig_AInputQueue_getEvent)(AInputQueue* queue, AInputEvent** outEvent);
int hooked_AInputQueue_getEvent(AInputQueue* queue, AInputEvent** outEvent) {
    int result = orig_AInputQueue_getEvent(queue, outEvent);
    if (result >= 0 && *outEvent != nullptr) {
        int32_t type = AInputEvent_getType(*outEvent);
        if (type == AINPUT_EVENT_TYPE_MOTION) {
            int32_t action = AMotionEvent_getAction(*outEvent);
            int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;
            
            // Use safe standard NDK function to get the pointer index
            int32_t pointerIndex = AMotionEvent_getActionIndex(*outEvent);

            float rawX = AMotionEvent_getX(*outEvent, pointerIndex);
            float rawY = AMotionEvent_getY(*outEvent, pointerIndex);
            
            float targetScreenWidth = screenWidth > 0 ? (float)screenWidth : (float)glWidth;
            float targetScreenHeight = screenHeight > 0 ? (float)screenHeight : (float)glHeight;

            float scaledX = (rawX * (float)glWidth) / targetScreenWidth;
            float scaledY = (rawY * (float)glHeight) / targetScreenHeight;

            ImGuiIO& io = ImGui::GetIO();
            switch (actionMasked) {
                case AMOTION_EVENT_ACTION_DOWN:
                case AMOTION_EVENT_ACTION_POINTER_DOWN:
                    io.MousePos = ImVec2(scaledX, scaledY);
                    io.MouseDown[0] = true;
                    break;
                case AMOTION_EVENT_ACTION_UP:
                case AMOTION_EVENT_ACTION_POINTER_UP:
                    io.MousePos = ImVec2(scaledX, scaledY);
                    io.MouseDown[0] = false;
                    break;
                case AMOTION_EVENT_ACTION_MOVE:
                    io.MousePos = ImVec2(scaledX, scaledY);
                    break;
                default:
                    break;
            }
        }
    }
    return result;
}

#include "functions.h"
#include "menu.h"

void *hack_thread(void *arg) {
    do {
        sleep(1);
        g_il2cppBaseMap = KittyMemory::getLibraryBaseMap("libil2cpp.so");
    } while (!g_il2cppBaseMap.isValid());
    
    KITTY_LOGI("il2cpp base: %p", (void*)(g_il2cppBaseMap.startAddress));
    Pointers();
    Hooks();

    // Hook eglSwapBuffers via Dobby
    auto eglhandle = dlopen("libEGL.so", RTLD_LAZY);
    if (eglhandle) {
        auto eglSwapBuffersSym = dlsym(eglhandle, "eglSwapBuffers");
        if (eglSwapBuffersSym) {
            DobbyHook((void*)eglSwapBuffersSym, (void*)hook_eglSwapBuffers, (void**)&old_eglSwapBuffers);
        }
    }

    // Hook AInputQueue_getEvent via Dobby (Stable NDK Touch Fix)
    void *libAndroid = dlopen("libandroid.so", RTLD_LAZY);
    if (libAndroid) {
        void *symEvent = dlsym(libAndroid, "AInputQueue_getEvent");
        if (symEvent) {
            DobbyHook(symEvent, (void *)hooked_AInputQueue_getEvent, (void **)&orig_AInputQueue_getEvent);
            LOGI("Dobby successfully hooked AInputQueue_getEvent!");
        } else {
            LOGE("Failed to resolve AInputQueue_getEvent symbol!");
        }
    }

    LOGI("Draw & Touch Hooks Setup Done!");
    return nullptr;
}