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
    do {
        sleep(1);
        g_il2cppBaseMap = KittyMemory::getLibraryBaseMap("libil2cpp.so");
    } while (!g_il2cppBaseMap.isValid());
    LOGI("il2cpp base: %p", (void*)(g_il2cppBaseMap.startAddress));

    std::thread([]() {
        sleep(5);
        IL2CPP::Init();
        for (int i = 0; i < 30; i++) {
            if (IL2CPP::API::il2cpp_domain_get != nullptr) {
                IL2CPP::domain = IL2CPP::API::il2cpp_domain_get();
                if (IL2CPP::domain != nullptr) {
                    LOGI("IL2CPP Domain obtained: %p", IL2CPP::domain);
                    IL2CPP::Attach();
                    return;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        LOGW("Failed to get IL2CPP domain");
    }).detach();

    auto eglHandle = dlopen("libEGL.so", RTLD_LAZY);
    if (eglHandle) {
        auto eglSwapBuffers = dlsym(eglHandle, "eglSwapBuffers");
        if (eglSwapBuffers) {
            DobbyHook((void*)eglSwapBuffers, (void*)hook_eglSwapBuffers,
                      (void**)&old_eglSwapBuffers);
            LOGI("eglSwapBuffers hooked");
        }
        dlclose(eglHandle);
    }

    auto eglMakeCurrent = dlsym(RTLD_DEFAULT, "eglMakeCurrent");
    if (eglMakeCurrent) {
        DobbyHook((void*)eglMakeCurrent, (void*)hook_eglMakeCurrent,
                  (void**)&old_eglMakeCurrent);
        LOGI("eglMakeCurrent hooked");
    }

    auto eglSwapBuffersWithDamageKHR = dlsym(RTLD_DEFAULT, "eglSwapBuffersWithDamageKHR");
    if (eglSwapBuffersWithDamageKHR) {
        DobbyHook((void*)eglSwapBuffersWithDamageKHR, (void*)hook_eglSwapBuffersWithDamageKHR,
                  (void**)&old_eglSwapBuffersWithDamageKHR);
        LOGI("eglSwapBuffersWithDamageKHR hooked");
    }

    LOGI("All hooks set up");
    return nullptr;
}

// ============================================================
// HOOK FUNCTIONS
// ============================================================

EGLBoolean (*old_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

    if (!setupimg) {
        SetupImgui();
        setupimg = true;
    }

    ImGuiIO &io = ImGui::GetIO();

    if (IL2CPP::domain != nullptr && IL2CPP::API::il2cpp_class_from_name != nullptr) {
        auto corlib = IL2CPP::API::il2cpp_get_corlib();
        if (corlib) {
            auto inputClass = IL2CPP::API::il2cpp_class_from_name(corlib, "UnityEngine", "Input");
            if (inputClass) {
                auto touchCountMethod = IL2CPP::API::il2cpp_class_get_method_from_name(inputClass, "get_touchCount", 0);
                auto getTouchMethod = IL2CPP::API::il2cpp_class_get_method_from_name(inputClass, "GetTouch", 1);
                
                if (touchCountMethod && touchCountMethod->methodPointer &&
                    getTouchMethod && getTouchMethod->methodPointer) {
                    
                    typedef int (*get_touchCount_t)();
                    typedef UnityEngine_Touch_Fields (*get_touch_t)(int);
                    
                    auto TouchCount = (get_touchCount_t)touchCountMethod->methodPointer;
                    auto GetTouch = (get_touch_t)getTouchMethod->methodPointer;
                    
                    int touchCount = TouchCount();
                    if (touchCount > 0) {
                        UnityEngine_Touch_Fields touch = GetTouch(0);
                        float reverseY = io.DisplaySize.y - touch.m_Position.fields.y;

                        switch (touch.m_Phase) {
                            case TouchPhase::Began:
                            case TouchPhase::Stationary:
                                io.MousePos = ImVec2(touch.m_Position.fields.x, reverseY);
                                io.MouseDown[0] = true;
                                break;
                            case TouchPhase::Ended:
                            case TouchPhase::Canceled:
                                io.MouseDown[0] = false;
                                break;
                            case TouchPhase::Moved:
                                io.MousePos = ImVec2(touch.m_Position.fields.x, reverseY);
                                break;
                            default:
                                break;
                        }
                    } else {
                        io.MouseDown[0] = false;
                    }
                }
            }
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
    DrawKenzGUIMenu();
    Patches();
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    return old_eglSwapBuffers(dpy, surface);
}

void (*old_eglMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
void hook_eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) {
    old_eglMakeCurrent(dpy, draw, read, ctx);
    
    if (ctx != EGL_NO_CONTEXT && draw != EGL_NO_SURFACE) {
        if (!setupimg) {
            SetupImgui();
            setupimg = true;
        }
        
        if (setupimg) {
            ImGuiIO &io = ImGui::GetIO();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplAndroid_NewFrame();
            ImGui::NewFrame();
            DrawKenzGUIMenu();
            Patches();
            ImGui::Render();
            glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
    }
}

// ============================================================
// eglSwapBuffersWithDamageKHR hook (was missing)
// ============================================================
EGLBoolean (*old_eglSwapBuffersWithDamageKHR)(EGLDisplay dpy, EGLSurface surface, EGLint* rects, EGLint n_rects);
EGLBoolean hook_eglSwapBuffersWithDamageKHR(EGLDisplay dpy, EGLSurface surface, EGLint* rects, EGLint n_rects) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

    if (!setupimg) {
        SetupImgui();
        setupimg = true;
    }

    ImGuiIO &io = ImGui::GetIO();

    if (IL2CPP::domain != nullptr && IL2CPP::API::il2cpp_class_from_name != nullptr) {
        auto corlib = IL2CPP::API::il2cpp_get_corlib();
        if (corlib) {
            auto inputClass = IL2CPP::API::il2cpp_class_from_name(corlib, "UnityEngine", "Input");
            if (inputClass) {
                auto touchCountMethod = IL2CPP::API::il2cpp_class_get_method_from_name(inputClass, "get_touchCount", 0);
                auto getTouchMethod = IL2CPP::API::il2cpp_class_get_method_from_name(inputClass, "GetTouch", 1);
                
                if (touchCountMethod && touchCountMethod->methodPointer &&
                    getTouchMethod && getTouchMethod->methodPointer) {
                    
                    typedef int (*get_touchCount_t)();
                    typedef UnityEngine_Touch_Fields (*get_touch_t)(int);
                    
                    auto TouchCount = (get_touchCount_t)touchCountMethod->methodPointer;
                    auto GetTouch = (get_touch_t)getTouchMethod->methodPointer;
                    
                    int touchCount = TouchCount();
                    if (touchCount > 0) {
                        UnityEngine_Touch_Fields touch = GetTouch(0);
                        float reverseY = io.DisplaySize.y - touch.m_Position.fields.y;

                        switch (touch.m_Phase) {
                            case TouchPhase::Began:
                            case TouchPhase::Stationary:
                                io.MousePos = ImVec2(touch.m_Position.fields.x, reverseY);
                                io.MouseDown[0] = true;
                                break;
                            case TouchPhase::Ended:
                            case TouchPhase::Canceled:
                                io.MouseDown[0] = false;
                                break;
                            case TouchPhase::Moved:
                                io.MousePos = ImVec2(touch.m_Position.fields.x, reverseY);
                                break;
                            default:
                                break;
                        }
                    } else {
                        io.MouseDown[0] = false;
                    }
                }
            }
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
    DrawKenzGUIMenu();
    Patches();
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    return old_eglSwapBuffersWithDamageKHR(dpy, surface, rects, n_rects);
}