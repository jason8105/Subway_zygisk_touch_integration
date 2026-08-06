#ifndef ZYGISK_MENU_TEMPLATE_MENU_H
#define ZYGISK_MENU_TEMPLATE_MENU_H

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "il2cpp.h"

extern int glWidth, glHeight;
extern bool setupimg;

// Helper: get screen size from Unity safely (KenzGUI method)
bool GetScreenSizeFromUnity(float &width, float &height) {
    // Default fallback
    width = 1080.0f;
    height = 1920.0f;
    
    // Only try IL2CPP if domain is ready
    if (IL2CPP::domain == nullptr) return false;
    if (IL2CPP::API::il2cpp_domain_get == nullptr) return false;
    
    // Try to find Screen class and get_width/get_height methods
    // This uses the Il2Cpp API from your il2cpp.h
    auto corlib = IL2CPP::API::il2cpp_get_corlib();
    if (!corlib) return false;
    
    auto screenClass = IL2CPP::API::il2cpp_class_from_name(corlib, "UnityEngine", "Screen");
    if (!screenClass) return false;
    
    auto getWidthMethod = IL2CPP::API::il2cpp_class_get_method_from_name(screenClass, "get_width", 0);
    auto getHeightMethod = IL2CPP::API::il2cpp_class_get_method_from_name(screenClass, "get_height", 0);
    
    if (!getWidthMethod || !getHeightMethod) return false;
    if (!getWidthMethod->methodPointer || !getHeightMethod->methodPointer) return false;
    
    typedef int (*get_int_fn)();
    auto getWidth = (get_int_fn)getWidthMethod->methodPointer;
    auto getHeight = (get_int_fn)getHeightMethod->methodPointer;
    
    width = (float)getWidth();
    height = (float)getHeight();
    return true;
}

void DrawMenu()
{
    {
        ImGui::Begin("ZyCheats");
        ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_FittingPolicyResizeDown;
        if (ImGui::BeginTabBar("Menu", tab_bar_flags)) {
            if (ImGui::BeginTabItem("Cheats")) {
                
                ImGui::Checkbox("Free Shopping", &stopZ);
                
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        Patches();
        ImGui::End();
    }
}

void SetupImgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    
    // KenzGUI method: try Unity Screen API first, fallback to EGL
    float unityW, unityH;
    if (GetScreenSizeFromUnity(unityW, unityH)) {
        io.DisplaySize = ImVec2(unityW, unityH);
        LOGI("KenzGUI: Screen size from Unity: %.0fx%.0f", unityW, unityH);
    } else {
        // Fallback to EGL
        io.DisplaySize = ImVec2((float) glWidth, (float) glHeight);
        LOGI("KenzGUI: Screen size from EGL: %dx%d", glWidth, glHeight);
    }
    
    ImGui_ImplOpenGL3_Init("#version 100");
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(7.0f);
    io.Fonts->AddFontDefault();
    
    ImGui_ImplAndroid_Init(nullptr);
}

// Update screen size from Unity on each frame (if domain becomes ready later)
void UpdateScreenSizeIfNeeded() {
    ImGuiIO &io = ImGui::GetIO();
    
    // If we already have valid size from Unity, skip
    if (io.DisplaySize.x > 100 && io.DisplaySize.y > 100) return;
    
    float unityW, unityH;
    if (GetScreenSizeFromUnity(unityW, unityH)) {
        io.DisplaySize = ImVec2(unityW, unityH);
        LOGI("KenzGUI: Updated screen size from Unity: %.0fx%.0f", unityW, unityH);
    }
}

EGLBoolean (*old_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

    if (!setupimg) {
        SetupImgui();
        setupimg = true;
    }

    // Try to update screen size from Unity if domain is now ready
    UpdateScreenSizeIfNeeded();

    ImGuiIO &io = ImGui::GetIO();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    DrawMenu();

    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return old_eglSwapBuffers(dpy, surface);
}

#endif //ZYGISK_MENU_TEMPLATE_MENU_H