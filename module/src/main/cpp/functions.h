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

inline bool stopZ = false;   // <-- add inline

inline void Pointers() {}    // <-- add inline
inline void Patches() {}     // <-- add inline

// Background worker thread - only resolve function pointers, DO NOT get domain
inline void InitWorker() {   // <-- add inline
    while (!IL2CPP::il2cpp_base) {
        if (IL2CPP::Init()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    LOGI("InitWorker() done - IL2CPP functions resolved");
}

inline void Hooks() {        // <-- add inline
    std::thread(InitWorker).detach();
}

#endif //ZYCHEATS_SGUYS_FUNCTIONS_H