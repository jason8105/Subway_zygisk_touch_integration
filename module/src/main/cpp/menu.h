#ifndef ZYGISK_MENU_TEMPLATE_MENU_H
#define ZYGISK_MENU_TEMPLATE_MENU_H

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "il2cpp.h"
#include "functions.h"
#include "hook.h"

extern int glWidth, glHeight;
extern bool setupimg;

// Helper: get screen size from Unity safely
bool GetScreenSizeFromUnity(float &width, float &height) {
    width = 1080.0f;
    height = 1920.0f;
    
    if (IL2CPP::domain == nullptr) return false;
    if (IL2CPP::API::il2cpp_domain_get == nullptr) return false;
    
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

void SetupImgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    
    float unityW, unityH;
    if (GetScreenSizeFromUnity(unityW, unityH)) {
        io.DisplaySize = ImVec2(unityW, unityH);
        LOGI("Screen size from Unity: %.0fx%.0f", unityW, unityH);
    } else {
        io.DisplaySize = ImVec2((float) glWidth, (float) glHeight);
        LOGI("Screen size from EGL: %dx%d", glWidth, glHeight);
    }
    
    ImGui_ImplOpenGL3_Init("#version 100");
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(7.0f);
    io.Fonts->AddFontDefault();
    
    ImGui_ImplAndroid_Init(nullptr);
}

void UpdateScreenSizeIfNeeded() {
    ImGuiIO &io = ImGui::GetIO();
    if (io.DisplaySize.x > 100 && io.DisplaySize.y > 100) return;
    
    float unityW, unityH;
    if (GetScreenSizeFromUnity(unityW, unityH)) {
        io.DisplaySize = ImVec2(unityW, unityH);
        LOGI("Updated screen size from Unity: %.0fx%.0f", unityW, unityH);
    }
}

#endif //ZYGISK_MENU_TEMPLATE_MENU_H