#ifndef ZYCHEATS_SGUYS_FUNCTIONS_H
#define ZYCHEATS_SGUYS_FUNCTIONS_H

#include <jni.h>
#include <dobby.h>
#include "il2cpp.h"
#include "xdl.h"

// The variable is defined in hook.cpp – only declare it here.
extern bool stopZ;

// Forward declarations (implemented in hook.cpp)
void Pointers();
void Patches();
void InitWorker();
void Hooks();

#endif // ZYCHEATS_SGUYS_FUNCTIONS_H