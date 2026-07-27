#ifndef ZYCHEATS_SGUYS_FUNCTIONS_H
#define ZYCHEATS_SGUYS_FUNCTIONS_H

#include <jni.h>
#include <android/log.h>
#include <dobby.h>
#include <thread>
#include <chrono>
#include "il2cpp.h"
#include "il2cpp_hook.h"
#include "xdl.h"

// Restored stopZ for menu.h compatibility
bool stopZ = false; 

void Pointers() {}
void Patches() {}

// Background worker thread to prevent early boot crashes (SIGSEGV)
void InitWorker() {
    // 1. Wait safely for libil2cpp.so to load
    while (!IL2CPP::il2cpp_base) {
        if (IL2CPP::Init()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // 2. Wait safely for Unity to create the IL2CPP Domain
    while (!IL2CPP::domain) {
        if (IL2CPP::API::il2cpp_domain_get != nullptr) {
            IL2CPP::domain = IL2CPP::API::il2cpp_domain_get();
        }
        if (IL2CPP::domain != nullptr) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // ====================================================================
    // GAME HOOKS GO HERE (Safe to add hooks here once domain is ready)
    // ====================================================================
}

void Hooks() {
    // Spawn detached thread so game startup is not blocked
    std::thread(InitWorker).detach();
}

#endif //ZYCHEATS_SGUYS_FUNCTIONS_H
