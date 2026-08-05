#include "menu.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>

// Definition for the extern declared in menu.h
bool menuVisible = true;

// ---------------------------------------------------------------------------
void InitMenu() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard |
                      ImGuiConfigFlags_NavEnableGamepad;
    io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(2.0f);          // make it readable on phones
}

// ---------------------------------------------------------------------------
void RenderMenu() {
    if (!menuVisible) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Subway Menu", &menuVisible,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    ImGui::Text("Overlay Active");
    ImGui::Separator();

    static bool toggle = false;
    ImGui::Checkbox("Example Toggle", &toggle);
    ImGui::Text(toggle ? "State: ON" : "State: OFF");

    ImGui::End();
}