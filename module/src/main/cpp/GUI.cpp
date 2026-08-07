#include "hook.h"
#include "menu.h"
#include "il2cpp.h"
#include "Includes/Dobby/dobby.h"
#include "Includes/obfuscate.h"
#include "KittyMemory/KittyMemory.h"
#include "ImGui/imgui.h"
#include "ImGui/backends/imgui_impl_android.h"
#include "ImGui/backends/imgui_impl_opengl3.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <thread>
#include <chrono>

// Unity structs for touch handling
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

static int currentTab = 0;

// ---------------------------------------------------------------------------
// KenzGUI style tabbed menu
// ---------------------------------------------------------------------------
void DrawKenzGUIMenu() {
    const ImVec2 window_size = ImVec2(700, 555);
    const char* window_title = "ZyCheats";
    ImGui::SetNextWindowSize(window_size, ImGuiCond_Once);

    if (ImGui::Begin(window_title, nullptr)) {
        ImGui::BeginChild("##LeftMenu", ImVec2(200, 0), true);
        {
            float menu_height = ImGui::GetContentRegionAvail().y;
            const int button_count = 3;
            float button_height = (menu_height - (button_count - 1) * ImGui::GetStyle().ItemSpacing.y) / button_count;

            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

            if (ImGui::Button("Home", ImVec2(-1, button_height))) 
                currentTab = 0;
            if (ImGui::Button("Visuals", ImVec2(-1, button_height))) 
                currentTab = 1;
            if (ImGui::Button("Settings", ImVec2(-1, button_height))) 
                currentTab = 2;

            ImGui::PopStyleVar(2);
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##Content", ImVec2(0, 0), true);
        {
            switch (currentTab) {
                case 0: {
                    extern bool stopZ;
                    ImGui::Checkbox("Free Shopping", &stopZ);
                    break;
                }
                case 1: {
                    ImGui::Text("ESP / Visuals");
                    break;
                }
                case 2: {
                    ImGui::Text("Memory Menu");
                    break;
                }
            }
        }
        ImGui::EndChild();
        ImGui::End();
    }
}

// ---------------------------------------------------------------------------
// hook_eglSwapBuffers - with touch handling from Unity Input
// ---------------------------------------------------------------------------
EGLBoolean (*old_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

    if (!setupimg) {
        SetupImgui();
        setupimg = true;
    }

    UpdateScreenSizeIfNeeded();

    ImGuiIO &io = ImGui::GetIO();

    // --- Touch handling via Unity Input class ---
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

    // Render ImGui
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