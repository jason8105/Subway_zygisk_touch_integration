#ifndef ZYCHEATS_SGUYS_FUNCTIONS_H
#define ZYCHEATS_SGUYS_FUNCTIONS_H

#include <jni.h>
#include <android/log.h>
#include <dobby.h>
#include "il2cpp.h"
#include "il2cpp_hook.h"
#include "xdl.h"

bool stopZ = false; // Free Shopping toggle

void Pointers() {}
void Patches() {}

// Free Shopping Hook (Stop Zombie)
bool (*_stopZombie)(void *thisObj);
bool StopZombie(void *thisObj) {
    if (stopZ) return false;
    return _stopZombie(thisObj);
}

void Hooks() {
    // 1. Get the DLL Image
    Il2CppImage* image = IL2CPP::GetImage("SYBO.Subway.Core.CommonData.dll");
    if (image != nullptr) {
        // 2. Get the Class
        Il2CppClass* klass = IL2CPP::API::il2cpp_class_from_name(image, "SYBO.Subway.Core.CommonData", "Currency");
        if (klass != nullptr) {
            // 3. Get the Method
            const MethodInfo* method = IL2CPP::GetMethod(klass, "get_IsIAP", 0);
            if (method != nullptr && method->methodPointer != nullptr) {
                // 4. Hook the method pointer directly
                DobbyHook((void *)method->methodPointer, (void *)StopZombie, (void **)&_stopZombie);
                LOGI("Successfully hooked get_IsIAP!");
            } else {
                LOGW("Failed to find get_IsIAP method!");
            }
        } else {
            LOGW("Failed to find Currency class!");
        }
    } else {
        LOGW("Failed to find SYBO.Subway.Core.CommonData.dll!");
    }
}

#endif //ZYCHEATS_SGUYS_FUNCTIONS_H
