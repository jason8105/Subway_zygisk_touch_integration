#include "hook.h"
#include "menu.h"
#include "il2cpp.h"
#include "Includes/Dobby/dobby.h"
#include "Include/obfuscate.h"
#include "KittyMemory/KittyMemory.h"
#include "ImGui/imgui.h"
#include "ImGui/backends/imgui_impl_android.h"
#include "ImGui/backends/imgui_impl_opengl3.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <thread>
#include <chrono>

extern int glWidth, glHeight;
extern bool setupimg;

static int currentTab = 0;

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