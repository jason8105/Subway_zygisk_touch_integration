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

bool feature1 = false;

void Pointers() {}
void Patches() {}

void InitWorker() {
    while (!IL2CPP::il2cpp_base) {
        if (IL2CPP::Init()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    while (!IL2CPP::domain) {
        if (IL2CPP::API::il2cpp_domain_get != nullptr) {
            IL2CPP::domain = IL2CPP::API::il2cpp_domain_get();
        }
        if (IL2CPP::domain != nullptr) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void Hooks() {
    std::thread(InitWorker).detach();
}

#endif //ZYCHEATS_SGUYS_FUNCTIONS_H
