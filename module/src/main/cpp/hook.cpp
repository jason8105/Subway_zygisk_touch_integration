#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <dlfcn.h>
#include <cstdlib>
#include <cinttypes>
#include <string>
#include <thread>
#include <chrono>

#include <android/log.h>
#include <pthread.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "dobby.h"

// ImGui
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_android.h"

#include "KittyMemory/KittyMemory.h"
#include "KittyMemory/KittyScanner.h"

// Project headers
#include "hook.h"
#include "menu.h"
#include "functions.h"
#include "Misc.h"
#include "zygisk.hpp"

#define GamePackageName "com.innersloth.spacemafia"

// ---------------------------------------------------------------------------
//  Globals
// ---------------------------------------------------------------------------
int enable_hack = 0;
char* game_data_dir = nullptr;
KittyMemory::ProcMap g_il2cppBaseMap;
bool setupimg = false;

// Vulkan state (simplified – we only need queue and device for ImGui)
static VkQueue g_vkQueue = VK_NULL_HANDLE;
static VkDevice g_vkDevice = VK_NULL_HANDLE;
static VkPhysicalDevice g_vkPhysicalDevice = VK_NULL_HANDLE;
static VkInstance g_vkInstance = VK_NULL_HANDLE;

// ---------------------------------------------------------------------------
//  Function bodies
// ---------------------------------------------------------------------------
bool stopZ = false;

void Pointers() { LOGI("Pointers() called"); }
void Patches() { LOGI("Patches() called"); }

void InitWorker() {
    LOGI("InitWorker() called");
    while (!IL2CPP::il2cpp_base) {
        if (IL2CPP::Init()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    while (!IL2CPP::domain) {
        if (IL2CPP::API::il2cpp_domain_get) {
            IL2CPP::domain = IL2CPP::API::il2cpp_domain_get();
        }
        if (IL2CPP::domain) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    if (IL2CPP::domain) IL2CPP::Attach();
}

void Hooks() {
    LOGI("Hooks() called");
    if (IL2CPP::domain) IL2CPP::Attach();
}

// ---------------------------------------------------------------------------
//  Vulkan hook: vkQueuePresentKHR
// ---------------------------------------------------------------------------
static VkResult (*old_vkQueuePresentKHR)(VkQueue, const VkPresentInfoKHR*) = nullptr;

VkResult hook_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    LOGI("hook_vkQueuePresentKHR called, setupimg=%d", setupimg);

    // Capture queue and device (we'll get device from queue later)
    if (g_vkQueue == VK_NULL_HANDLE) {
        g_vkQueue = queue;
        // Get device from queue (requires vkGetDeviceQueue, but we can store it from elsewhere)
        // For simplicity, we'll set up ImGui when we have device info
    }

    // Initialize ImGui on first call
    if (!setupimg && g_vkDevice != VK_NULL_HANDLE) {
        LOGI("Initializing ImGui for Vulkan");
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
        io.Fonts->AddFontDefault();
        ImGui::StyleColorsDark();

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = g_vkInstance;
        init_info.PhysicalDevice = g_vkPhysicalDevice;
        init_info.Device = g_vkDevice;
        init_info.QueueFamily = 0; // You need the correct queue family index
        init_info.Queue = g_vkQueue;
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = VK_NULL_HANDLE; // Create a descriptor pool
        init_info.Allocator = nullptr;
        init_info.MinImageCount = 2;
        init_info.ImageCount = 2;
        init_info.CheckVkResultFn = nullptr;
        init_info.RenderPass = VK_NULL_HANDLE; // Need to get from game

        if (ImGui_ImplVulkan_Init(&init_info)) {
            ImGui_ImplAndroid_Init(nullptr);
            setupimg = true;
            LOGI("ImGui Vulkan init OK");
        } else {
            LOGE("ImGui_ImplVulkan_Init failed");
        }
    }

    if (!setupimg) {
        return old_vkQueuePresentKHR ? old_vkQueuePresentKHR(queue, pPresentInfo) : VK_SUCCESS;
    }

    // ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    if (menuVisible) {
        LOGI("hook_vkQueuePresentKHR: rendering menu");
        RenderMenu();
    }

    ImGui::Render();
    // Submit draw data – you need a command buffer
    // ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

    return old_vkQueuePresentKHR ? old_vkQueuePresentKHR(queue, pPresentInfo) : VK_SUCCESS;
}

// ---------------------------------------------------------------------------
//  Additional Vulkan hooks to capture device and instance
// ---------------------------------------------------------------------------
static VkResult (*old_vkCreateDevice)(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*) = nullptr;
VkResult hook_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
    VkResult result = old_vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (result == VK_SUCCESS) {
        g_vkPhysicalDevice = physicalDevice;
        g_vkDevice = *pDevice;
        LOGI("Captured VkDevice: %p", (void*)g_vkDevice);
    }
    return result;
}

static VkResult (*old_vkCreateInstance)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*) = nullptr;
VkResult hook_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
    VkResult result = old_vkCreateInstance(pCreateInfo, pAllocator, pInstance);
    if (result == VK_SUCCESS) {
        g_vkInstance = *pInstance;
        LOGI("Captured VkInstance: %p", (void*)g_vkInstance);
    }
    return result;
}

// ---------------------------------------------------------------------------
//  Helper used by Zygisk entry point
// ---------------------------------------------------------------------------
int isGame(JNIEnv* env, jstring appDataDir) {
    if (!appDataDir) {
        LOGW("isGame: appDataDir is null");
        return 0;
    }
    const char* dir = env->GetStringUTFChars(appDataDir, nullptr);
    LOGI("isGame: full dir=%s", dir);

    int user = 0;
    char pkg[256] = {0};

    if (sscanf(dir, "/data/%*[^/]/%d/%255s", &user, pkg) != 2) {
        if (sscanf(dir, "/data/%*[^/]/%255s", pkg) != 1) {
            LOGW("isGame: failed to parse package from path: %s", dir);
            env->ReleaseStringUTFChars(appDataDir, dir);
            return 0;
        }
    }
    env->ReleaseStringUTFChars(appDataDir, dir);

    LOGI("isGame: parsed pkg=%s", pkg);
    if (strcmp(pkg, GamePackageName) == 0) {
        LOGI("isGame: match! package=%s", pkg);
        delete[] game_data_dir;
        game_data_dir = new char[strlen(dir) + 1];
        strcpy(game_data_dir, dir);
        return 1;
    }
    LOGI("isGame: no match, pkg=%s", pkg);
    return 0;
}

// ---------------------------------------------------------------------------
//  Thread started from postAppSpecialize
// ---------------------------------------------------------------------------
void* hack_thread(void* /*arg*/) {
    LOGI("hack_thread started");

    // Wait for libvulkan.so and hook Vulkan functions
    while (true) {
        sleep(1);
        void* vkCreateInstanceAddr = dlsym(RTLD_DEFAULT, "vkCreateInstance");
        void* vkCreateDeviceAddr = dlsym(RTLD_DEFAULT, "vkCreateDevice");
        void* vkQueuePresentKHRAddr = dlsym(RTLD_DEFAULT, "vkQueuePresentKHR");
        if (vkCreateInstanceAddr && vkCreateDeviceAddr && vkQueuePresentKHRAddr) {
            LOGI("Vulkan functions found, hooking...");
            DobbyHook(vkCreateInstanceAddr, (void*)hook_vkCreateInstance, (void**)&old_vkCreateInstance);
            DobbyHook(vkCreateDeviceAddr, (void*)hook_vkCreateDevice, (void**)&old_vkCreateDevice);
            DobbyHook(vkQueuePresentKHRAddr, (void*)hook_vkQueuePresentKHR, (void**)&old_vkQueuePresentKHR);
            LOGI("Vulkan hooks installed");
            break;
        }
    }

    // Wait for libil2cpp.so (if needed for IL2CPP hooks)
    while (true) {
        sleep(1);
        KittyMemory::ProcMap map = KittyMemory::getLibraryBaseMap("libil2cpp.so");
        if (map.isValid()) {
            g_il2cppBaseMap = map;
            LOGI("libil2cpp.so @ %p", (void*)map.startAddress);
            break;
        }
    }

    Pointers();
    Hooks();

    LOGI("hack_thread finished");
    return nullptr;
}