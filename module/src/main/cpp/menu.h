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
inline bool GetScreenSizeFromUnity(float &width, float &height) {   // <-- inline
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

inline void SetupImgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1080, 1920);
    LOGI("SetupImgui: Using 1080x1920 fallback");
    ImGui_ImplOpenGL3_Init("#version 100");
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(7.0f);
    io.Fonts->AddFontDefault();
    ImGui_ImplAndroid_Init(nullptr);
}

inline void UpdateScreenSizeIfNeeded() {   // <-- inline
    ImGuiIO &io = ImGui::GetIO();
    if (io.DisplaySize.x > 100 && io.DisplaySize.y > 100) return;
    
    float unityW, unityH;
    if (GetScreenSizeFromUnity(unityW, unityH)) {
        io.DisplaySize = ImVec2(unityW, unityH);
        LOGI("Updated screen size from Unity: %.0fx%.0f", unityW, unityH);
    }
}

// Add at the end of menu.h, before #endif
struct UnityEngine_Vector2_Fields {
    float x;
    float y;
};

struct UnityEngine_Vector2_o {
    UnityEngine_Vector2_Fields fields;
};

enum TouchPhase {
    Began = 0,
    Moved = 1,
    Stationary = 2,
    Ended = 3,
    Canceled = 4
};

struct UnityEngine_Touch_Fields {
    int32_t m_FingerId;
    struct UnityEngine_Vector2_o m_Position;
    struct UnityEngine_Vector2_o m_RawPosition;
    struct UnityEngine_Vector2_o m_PositionDelta;
    float m_TimeDelta;
    int32_t m_TapCount;
    int32_t m_Phase;
    int32_t m_Type;
    float m_Pressure;
    float m_maximumPossiblePressure;
    float m_Radius;
    float m_fRadiusVariance;
    float m_AltitudeAngle;
    float m_AzimuthAngle;
};

void DrawKenzGUIMenu();  // Declaration only

#endif //ZYGISK_MENU_TEMPLATE_MENU_H