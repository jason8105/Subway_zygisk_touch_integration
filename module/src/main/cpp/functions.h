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

// ✅ FIX: Sirf 'extern' aur semicolon (;) yahan hona chahiye
extern bool stopZ; 

void Pointers();
void Patches();
void InitWorker();
void Hooks();

#endif //ZYCHEATS_SGUYS_FUNCTIONS_H