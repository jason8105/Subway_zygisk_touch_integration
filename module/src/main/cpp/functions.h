#ifndef ZYCHEATS_SGUYS_FUNCTIONS_H
#define ZYCHEATS_SGUYS_FUNCTIONS_H

#include <jni.h>
#include <android/log.h>
#include <dobby.h>
#include <thread>
#include <chrono>
#include "hook.h"
#include "il2cpp.h"
#include "il2cpp_hook.h"
#include "xdl.h"

bool stopZ = false; 

void Pointers() {}
void Patches() {}

// Background worker thread - only resolve function pointers, DO NOT get domain
void InitWorker() {
    // 1. Wait safely for libil2cpp.so to load
    while (!IL2CPP::il2cpp_base) {
        if (IL2CPP::Init()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // DO NOT call il2cpp_domain_get() here - it will crash
    // Domain is obtained later when actually needed
    
    LOGI("InitWorker() done - IL2CPP functions resolved");
}

void Hooks() {
    // Spawn detached thread so game startup is not blocked
    std::thread(InitWorker).detach();
}

#endif //ZYCHEATS_SGUYS_FUNCTIONS_H