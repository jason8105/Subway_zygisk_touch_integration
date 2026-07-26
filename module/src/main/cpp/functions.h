#ifndef ZYCHEATS_SGUYS_FUNCTIONS_H
#define ZYCHEATS_SGUYS_FUNCTIONS_H

#include <jni.h>
#include <android/log.h>
#include <dobby.h>
#include "il2cpp.h"
#include "il2cpp_hook.h"
#include "xdl.h"

// Restored variables so menu.h compiles without errors
bool addCurrency = false, freeItems = false, everythingUnlocked = false, showAllItems = false, addSkins = false;
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
    // FIX: Using the correct namespace 'IL2CPP' and standard method 'GetMethodOffset'
    auto get_isIAP = IL2CPP::GetMethodOffset("SYBO.Subway.Core.CommonData.dll", "SYBO.Subway.Core.CommonData", "Currency", "get_IsIAP", 0);
    
    if (get_isIAP != 0) {
        DobbyHook((void *)get_isIAP, (void *)StopZombie, (void **)&_stopZombie);
    } else {
        LOGW("Failed to find get_IsIAP offset!");
    }
}
#endif //ZYCHEATS_SGUYS_FUNCTIONS_H
