#include "menu.h"
#include "imgui.h"

// Definition for the extern declared in menu.h
bool menuVisible = true;

// ---------------------------------------------------------------------------
void InitMenu() {
    // ImGui context is created in hook.cpp for Vulkan
    // This function is kept for compatibility but does nothing now
}

// ---------------------------------------------------------------------------
void RenderMenu() {
    if (!menuVisible) return;

    ImGui::Begin("Subway Menu", &menuVisible,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    ImGui::Text("Overlay Active");
    ImGui::Separator();

    static bool toggle = false;
    ImGui::Checkbox("Example Toggle", &toggle);
    ImGui::Text(toggle ? "State: ON" : "State: OFF");

    ImGui::End();
}