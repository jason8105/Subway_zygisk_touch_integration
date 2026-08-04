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
#include <android/log.h>
#include <android/input.h>

// ImGui
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"

// Libraries
#include "KittyMemory/KittyMemory.h"
#include "KittyMemory/MemoryPatch.h"
#include "KittyMemory/KittyScanner.h"
#include "KittyMemory/KittyUtils.h"
#include "Includes/Dobby/dobby.h"

// Project Headers
#include "hook.h"
#include "menu.h"
#include "functions.h"
#include "Misc.h"
#include "Include/Roboto-Regular.h"

#define GamePackageName "com.kiloo.subwaysurf"

// Global State
int glHeight = 0, glWidth = 0;
bool setupimg = false;
// ❌ REMOVED: bool menuVisible = true; (Ab sirf menu.cpp mein rahega)

// Dobby Input Stubs
typedef void (*InputFn)(void*, void*, void*);
InputFn origInput = nullptr;
typedef int32_t (*ConsumeFn)(void*, void*, bool, long, uint32_t*, AInputEvent**);
ConsumeFn origConsume = nullptr;

// Input Hook Handlers
void myInput(void *thiz, void *ex_ab, void *ex_ac) {
    if (origInput) origInput(thiz, ex_ab, ex_ac);
    ImGui_ImplAndroid_HandleInputEvent((AInputEvent *)thiz);
}

int32_t myConsume(void *thiz, void *arg1, bool arg2, long arg3, uint32_t *arg4, AInputEvent **input_event) {
    int32_t result = 0;
    if (origConsume) result = origConsume(thiz, arg1, arg2, arg3, arg4, input_event);
    if (result != 0 || !input_event || !*input_event) return result;
    ImGui_ImplAndroid_HandleInputEvent(*input_event);
    return result;
}

// EGL Swap Buffers Hook (Render Loop)
EGLBoolean (*old_eglSwapBuffers)(EGLDisplay, EGLSurface);
EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    if (!setupimg) {
        eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
        eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);
        InitMenu();
        setupimg = true;
    }

    ImGuiIO &io = ImGui::GetIO();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    // Draw Menu
    if (menuVisible) RenderMenu();

    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return old_eglSwapBuffers(dpy, surface);
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
            LOGW(OBFUSCATE("can't parse %s"), app_data_dir);
            env->ReleaseStringUTFChars(appDataDir, app_data_dir);
            return 0;
        }
    }
    if (strcmp(package_name, GamePackageName) == 0) {
        LOGI(OBFUSCATE("detect game: %s"), package_name);
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
    // Wait for libil2cpp.so
    do {
        sleep(1);
        // ✅ Use actual API from KittyMemory.h
        KittyMemory::ProcMap il2cppMap = KittyMemory::getLibraryBaseMap("libil2cpp.so");
        
        if (il2cppMap.isValid()) {
            uintptr_t il2cppBase = static_cast<uintptr_t>(il2cppMap.startAddress);
            g_il2cppBaseMap = il2cppMap; // Global ProcMap assign
            KITTY_LOGI("il2cpp base: %p", (void*)il2cppBase);
            break;
        }
    } while (true);

    Pointers();
    Hooks();

    // Hook eglSwapBuffers (Unity Render Loop)
    auto eglhandle = dlopen("libunity.so", RTLD_LAZY);
    if (eglhandle) {
        auto eglSwapBuffers = dlsym(eglhandle, "eglSwapBuffers");
        if (eglSwapBuffers) {
            DobbyHook((void*)eglSwapBuffers, (void*)hook_eglSwapBuffers, (void**)&old_eglSwapBuffers);
            LOGI("eglSwapBuffers hooked!");
        }
    }

    // Hook Android InputSystem (64-bit fallback to 32-bit)
    void *sym_input = DobbySymbolResolver("/system/lib64/libinput.so", "_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE");
    if (!sym_input) sym_input = DobbySymbolResolver("/system/lib64/libinput.so", "_ZN7android13InputConsumer7consumeEPNS_26InputEventFactoryInterfaceEblPjPPNS_10InputEventE");
    if (!sym_input) sym_input = DobbySymbolResolver("/system/lib/libinput.so", "_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE");
    if (!sym_input) sym_input = DobbySymbolResolver("/system/lib/libinput.so", "_ZN7android13InputConsumer7consumeEPNS_26InputEventFactoryInterfaceEblPjPPNS_10InputEventE");

    if (sym_input) {
        DobbyHook(sym_input, (void*)myConsume, (void**)&origConsume);
        LOGI("InputConsumer hooked!");
    } else {
        LOGE("Failed to resolve InputConsumer symbol!");
    }

    LOGI("Hook thread completed successfully!");
    return nullptr;
}