#ifndef ZYCHEATS_SGUYS_FUNCTIONS_H
#define ZYCHEATS_SGUYS_FUNCTIONS_H

// Required Headers
#include "il2cpp.h"
#include "il2cpp_hook.h"
#include "xdl.h"

// Variables for the cheats
bool stopZ = false; // Free Shopping toggle

void Pointers() {
    // Left empty: add future pointers here
}

void Patches() {
    // Left empty: add future patches here
}

// Free Shopping Hook (Stop Zombie)
bool (*_stopZombie)(void *thisObj);
bool StopZombie(void *thisObj) {
    if (stopZ) {
        return false; // Returns false when the Free Shopping cheat is enabled
    }
    return _stopZombie(thisObj);
}

void Hooks() {
    // Dynamically fetch and hook get_IsIAP using il2cpp features
    auto get_isIAP = IL2Cpp::Il2CppGetMethodOffset(
        "SYBO.Subway.Core.CommonData.dll", 
        "SYBO.Subway.Core.CommonData", 
        "Currency", 
        "get_IsIAP", 
        0
    );
    
    if (get_isIAP != 0) { // Safety check to ensure the method was found
        DobbyHook((void *)get_isIAP, (void *)StopZombie, (void **)&_stopZombie);
    } else {
        LOGW("Failed to find get_IsIAP offset!");
    }
}

#endif //ZYCHEATS_SGUYS_FUNCTIONS_H
