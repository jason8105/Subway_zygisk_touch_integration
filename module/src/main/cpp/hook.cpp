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

// Frame counters for debugging
static int frameCount_eglSwapBuffers = 0;
static int frameCount_glDrawElements = 0;
static int frameCount_vkQueuePresentKHR = 0;
static int frameCount_ANativeWindow_lock = 0;
static int frameCount_eglMakeCurrent = 0;

void DumpAllSymbols() {
    LOGI("====================================================");
    LOGI("=== COMPREHENSIVE SYMBOL DUMP ===");
    LOGI("====================================================");
    
    // 1. Check all rendering libraries
    LOGI("--- Checking loaded libraries ---");
    const char* libs[] = {
        "libunity.so", "libEGL.so", "libGLESv2.so", "libGLESv3.so",
        "libvulkan.so", "libnativewindow.so", "libandroid.so",
        "libc.so", "libm.so", "libOpenSLES.so"
    };
    for (auto lib : libs) {
        auto handle = dlopen(lib, RTLD_LAZY | RTLD_NOLOAD);
        LOGI("  Library %s: %s", lib, handle ? "LOADED" : "NOT LOADED");
        if (handle) dlclose(handle);
    }
    
    // 2. Dump ALL rendering-related symbols
    LOGI("--- Checking all rendering symbols ---");
    const char* symbols[] = {
        // EGL
        "eglSwapBuffers", "eglMakeCurrent", "eglSwapInterval",
        "eglGetDisplay", "eglCreateWindowSurface", "eglDestroySurface",
        "eglQuerySurface", "eglGetError", "eglInitialize",
        "eglChooseConfig", "eglCreateContext", "eglMakeCurrent",
        // OpenGL ES
        "glDrawElements", "glDrawArrays", "glClear",
        "glClearColor", "glViewport", "glScissor",
        "glUseProgram", "glBindFramebuffer", "glFinish",
        "glFlush", "eglSwapBuffersWithDamageKHR",
        // Vulkan
        "vkQueuePresentKHR", "vkQueueSubmit", "vkAcquireNextImageKHR",
        "vkCreateSwapchainKHR", "vkDestroySwapchainKHR",
        "vkGetSwapchainImagesKHR", "vkQueueWaitIdle",
        "vkDeviceWaitIdle", "vkWaitForFences",
        // Android native
        "ANativeWindow_lock", "ANativeWindow_unlockAndPost",
        "ANativeWindow_setBuffersGeometry", "eglCreateSyncKHR",
        "eglClientWaitSyncKHR", "eglDestroySyncKHR",
        // Unity specific
        "UnitySendMessage", "UnityPlayerRender",
        "_Z19UnityPlayerRenderv", "GfxDeviceGLESInit",
        "GfxDeviceVulkanInit"
    };
    
    for (auto sym : symbols) {
        void* ptr = nullptr;
        // Try RTLD_DEFAULT first
        ptr = dlsym(RTLD_DEFAULT, sym);
        if (ptr) {
            LOGI("  [RTLD_DEFAULT] %s = %p", sym, ptr);
        } else {
            // Try RTLD_NEXT
            ptr = dlsym(RTLD_NEXT, sym);
            if (ptr) {
                LOGI("  [RTLD_NEXT]    %s = %p", sym, ptr);
            } else {
                LOGI("  [NOT FOUND]    %s", sym);
            }
        }
    }
    
    // 3. Dump /proc/self/maps for all loaded libraries
    LOGI("--- Dumping loaded libraries from /proc/self/maps ---");
    FILE* maps = fopen("/proc/self/maps", "r");
    if (maps) {
        char line[512];
        while (fgets(line, sizeof(line), maps)) {
            // Only log .so files
            if (strstr(line, ".so")) {
                // Remove newline
                line[strcspn(line, "\n")] = 0;
                LOGI("  MAP: %s", line);
            }
        }
        fclose(maps);
    }
    
    // 4. Get current process info
    LOGI("--- Process info ---");
    LOGI("  PID: %d", getpid());
    LOGI("  UID: %d", getuid());
    
    // 5. Check EGL display
    LOGI("--- EGL Display check ---");
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy != EGL_NO_DISPLAY) {
        LOGI("  EGL Display: %p", dpy);
        EGLint major, minor;
        if (eglInitialize(dpy, &major, &minor)) {
            LOGI("  EGL initialized: %d.%d", major, minor);
            LOGI("  EGL Vendor: %s", eglQueryString(dpy, EGL_VENDOR));
            LOGI("  EGL Version: %s", eglQueryString(dpy, EGL_VERSION));
            LOGI("  EGL Extensions: %s", eglQueryString(dpy, EGL_EXTENSIONS));
        }
    } else {
        LOGI("  EGL Display: NULL (no display yet)");
    }
    
    // 6. Try to get OpenGL renderer info
    LOGI("--- OpenGL info ---");
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* version = (const char*)glGetString(GL_VERSION);
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    LOGI("  GL_RENDERER: %s", renderer ? renderer : "NULL");
    LOGI("  GL_VERSION: %s", version ? version : "NULL");
    LOGI("  GL_VENDOR: %s", vendor ? vendor : "NULL");
    
    LOGI("====================================================");
    LOGI("=== DUMP COMPLETE ===");
    LOGI("====================================================");
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
    LOGI("hack_thread started - PID: %d, TID: %d", getpid(), gettid());
    
    // Dump all symbols first
    DumpAllSymbols();
    
    // Wait for libil2cpp.so
    do {
        sleep(1);
        g_il2cppBaseMap = KittyMemory::getLibraryBaseMap("libil2cpp.so");
    } while (!g_il2cppBaseMap.isValid());
    LOGI("il2cpp base: %p", (void*)(g_il2cppBaseMap.startAddress));

    // Reduce IL2CPP wait to 5 seconds
    std::thread([]() {
        sleep(5);
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

    // Try EVERY possible hook in order
    LOGI("=== Attempting to hook rendering functions ===");
    
    // 1. Try eglSwapBuffers from libEGL.so directly
    auto eglHandle = dlopen("libEGL.so", RTLD_LAZY);
    if (eglHandle) {
        auto eglSwapBuffers = dlsym(eglHandle, "eglSwapBuffers");
        if (eglSwapBuffers) {
            LOGI("HOOKING: eglSwapBuffers from libEGL.so at %p", eglSwapBuffers);
            DobbyHook((void*)eglSwapBuffers, (void*)hook_eglSwapBuffers,
                      (void**)&old_eglSwapBuffers);
            LOGI("SUCCESS: eglSwapBuffers hooked");
        } else {
            LOGW("FAILED: eglSwapBuffers not found in libEGL.so");
        }
        dlclose(eglHandle);
    } else {
        LOGW("FAILED: libEGL.so not found");
    }
    
    // 2. Try glDrawElements
    auto glDrawElements = dlsym(RTLD_DEFAULT, "glDrawElements");
    if (glDrawElements) {
        LOGI("HOOKING: glDrawElements at %p", glDrawElements);
        DobbyHook((void*)glDrawElements, (void*)hook_glDrawElements,
                  (void**)&old_glDrawElements);
        LOGI("SUCCESS: glDrawElements hooked");
    } else {
        LOGW("FAILED: glDrawElements not found");
    }
    
    // 3. Try vkQueuePresentKHR
    auto vkQueuePresentKHR = dlsym(RTLD_DEFAULT, "vkQueuePresentKHR");
    if (vkQueuePresentKHR) {
        LOGI("HOOKING: vkQueuePresentKHR at %p", vkQueuePresentKHR);
        DobbyHook((void*)vkQueuePresentKHR, (void*)hook_vkQueuePresentKHR,
                  (void**)&old_vkQueuePresentKHR);
        LOGI("SUCCESS: vkQueuePresentKHR hooked");
    } else {
        LOGW("FAILED: vkQueuePresentKHR not found");
    }
    
    // 4. Try ANativeWindow_lock
    auto nativeWindowLock = dlsym(RTLD_DEFAULT, "ANativeWindow_lock");
    if (nativeWindowLock) {
        LOGI("HOOKING: ANativeWindow_lock at %p", nativeWindowLock);
        DobbyHook((void*)nativeWindowLock, (void*)hook_ANativeWindow_lock,
                  (void**)&old_ANativeWindow_lock);
        LOGI("SUCCESS: ANativeWindow_lock hooked");
    } else {
        LOGW("FAILED: ANativeWindow_lock not found");
    }
    
    // 5. Try eglMakeCurrent
    auto eglMakeCurrent = dlsym(RTLD_DEFAULT, "eglMakeCurrent");
    if (eglMakeCurrent) {
        LOGI("HOOKING: eglMakeCurrent at %p", eglMakeCurrent);
        DobbyHook((void*)eglMakeCurrent, (void*)hook_eglMakeCurrent,
                  (void**)&old_eglMakeCurrent);
        LOGI("SUCCESS: eglMakeCurrent hooked");
    } else {
        LOGW("FAILED: eglMakeCurrent not found");
    }

    LOGI("=== All hooks attempted ===");
    LOGI("Draw Done! - waiting for hook calls...");
    return nullptr;
}

// ============================================================
// HOOK FUNCTIONS WITH DETAILED LOGGING
// ============================================================

EGLBoolean (*old_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    frameCount_eglSwapBuffers++;
    
    // Log first few calls
    if (frameCount_eglSwapBuffers <= 5 || frameCount_eglSwapBuffers % 100 == 0) {
        LOGI("CALLED: eglSwapBuffers #%d (TID: %d)", frameCount_eglSwapBuffers, gettid());
    }
    
    eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);
    
    // Log screen size on first call
    if (frameCount_eglSwapBuffers == 1) {
        LOGI("Screen size from EGL: %dx%d", glWidth, glHeight);
    }

    if (!setupimg) {
        LOGI("Setting up ImGui from eglSwapBuffers");
        SetupImgui();
        setupimg = true;
        LOGI("ImGui setup complete");
    }

    UpdateScreenSizeIfNeeded();

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

void (*old_glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void* indices);
void hook_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    frameCount_glDrawElements++;
    
    if (frameCount_glDrawElements <= 5 || frameCount_glDrawElements % 100 == 0) {
        LOGI("CALLED: glDrawElements #%d (TID: %d)", frameCount_glDrawElements, gettid());
    }
    
    static bool setup = false;
    if (!setup) {
        LOGI("glDrawElements hooked - setting up ImGui");
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1080, 1920);
        ImGui_ImplOpenGL3_Init("#version 100");
        io.Fonts->AddFontDefault();
        ImGui_ImplAndroid_Init(nullptr);
        setup = true;
        LOGI("glDrawElements ImGui setup complete");
    }
    
    ImGuiIO &io = ImGui::GetIO();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
    
    DrawKenzGUIMenu();
    Patches();
    
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    old_glDrawElements(mode, count, type, indices);
}

void (*old_vkQueuePresentKHR)(void* queue, void* pPresentInfo);
void hook_vkQueuePresentKHR(void* queue, void* pPresentInfo) {
    frameCount_vkQueuePresentKHR++;
    
    if (frameCount_vkQueuePresentKHR <= 5 || frameCount_vkQueuePresentKHR % 100 == 0) {
        LOGI("CALLED: vkQueuePresentKHR #%d (TID: %d)", frameCount_vkQueuePresentKHR, gettid());
    }
    
    static bool setup = false;
    if (!setup) {
        LOGI("vkQueuePresentKHR hooked - setting up ImGui");
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1080, 1920);
        ImGui_ImplOpenGL3_Init("#version 100");
        io.Fonts->AddFontDefault();
        ImGui_ImplAndroid_Init(nullptr);
        setup = true;
        LOGI("vkQueuePresentKHR ImGui setup complete");
    }
    
    ImGuiIO &io = ImGui::GetIO();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
    
    DrawKenzGUIMenu();
    Patches();
    
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    old_vkQueuePresentKHR(queue, pPresentInfo);
}

void (*old_ANativeWindow_lock)(void* window, void* outBuffer, void* inOutDirtyBounds);
void hook_ANativeWindow_lock(void* window, void* outBuffer, void* inOutDirtyBounds) {
    frameCount_ANativeWindow_lock++;
    
    if (frameCount_ANativeWindow_lock <= 5 || frameCount_ANativeWindow_lock % 100 == 0) {
        LOGI("CALLED: ANativeWindow_lock #%d (TID: %d)", frameCount_ANativeWindow_lock, gettid());
    }
    
    static bool setup = false;
    if (!setup) {
        LOGI("ANativeWindow_lock hooked - setting up ImGui");
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1080, 1920);
        ImGui_ImplOpenGL3_Init("#version 100");
        io.Fonts->AddFontDefault();
        ImGui_ImplAndroid_Init(nullptr);
        setup = true;
        LOGI("ANativeWindow_lock ImGui setup complete");
    }
    
    ImGuiIO &io = ImGui::GetIO();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
    
    DrawKenzGUIMenu();
    Patches();
    
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    old_ANativeWindow_lock(window, outBuffer, inOutDirtyBounds);
}

void (*old_eglMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
void hook_eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) {
    frameCount_eglMakeCurrent++;
    
    if (frameCount_eglMakeCurrent <= 5 || frameCount_eglMakeCurrent % 100 == 0) {
        LOGI("CALLED: eglMakeCurrent #%d (TID: %d)", frameCount_eglMakeCurrent, gettid());
    }
    
    // Only set up ImGui once we have a valid context
    if (ctx != EGL_NO_CONTEXT && !setupimg) {
        EGLint w = 1080, h = 1920;
        if (draw != EGL_NO_SURFACE) {
            eglQuerySurface(dpy, draw, EGL_WIDTH, &w);
            eglQuerySurface(dpy, draw, EGL_HEIGHT, &h);
        }
        glWidth = w;
        glHeight = h;
        LOGI("eglMakeCurrent: screen %dx%d, context %p", w, h, ctx);
    }
    
    old_eglMakeCurrent(dpy, draw, read, ctx);
}