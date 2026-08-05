#ifndef ZYGISKPG_MISC_H
#define ZYGISKPG_MISC_H

#include "Includes/Dobby/dobby.h"
#include "Include/Unity.h"
#include "KittyMemory/KittyMemory.h"
#include "KittyMemory/KittyScanner.h"
#include "KittyMemory/MemoryPatch.h"
#include "Include/obfuscate.h"

using KittyMemory::ProcMap;
using KittyScanner::RegisterNativeFn;

// Filled by hack_thread once libil2cpp.so is located
extern ProcMap g_il2cppBaseMap;

// ---------------------------------------------------------------------------
//  Tiny wrapper around DobbyHook
// ---------------------------------------------------------------------------
inline void hook(void* offset, void* detour, void** original) {
    DobbyHook(offset, detour, original);
}

// ---------------------------------------------------------------------------
//  Runtime patch helper (used by PATCH / PATCH_SWITCH macros)
// ---------------------------------------------------------------------------
std::vector<MemoryPatch> memoryPatches;
std::vector<uint64_t>    offsetVector;

inline void patchOffset(uint64_t offset, const std::string& hexBytes, bool enable) {
    MemoryPatch patch = MemoryPatch::createWithHex(g_il2cppBaseMap, offset, hexBytes);
    if (!patch.isValid()) return;

    auto it = std::find(offsetVector.begin(), offsetVector.end(), offset);
    if (it != offsetVector.end()) {
        patch = memoryPatches[std::distance(offsetVector.begin(), it)];
    } else {
        memoryPatches.push_back(patch);
        offsetVector.push_back(offset);
    }

    if (enable && patch.get_CurrBytes() == patch.get_OrigBytes())
        patch.Modify();
    else if (!enable && patch.get_CurrBytes() != patch.get_OrigBytes())
        patch.Restore();
}

// ---------------------------------------------------------------------------
//  Convert a hex string (e.g. "0x1234") to uintptr_t
// ---------------------------------------------------------------------------
inline uintptr_t string2Offset(const char* c) {
    constexpr int base = 16;
    static_assert(sizeof(uintptr_t) == sizeof(unsigned long) ||
                  sizeof(uintptr_t) == sizeof(unsigned long long),
                  "Add conversion for this architecture");

    if (sizeof(uintptr_t) == sizeof(unsigned long))
        return strtoul(c, nullptr, base);
    return strtoull(c, nullptr, base);
}

// ---------------------------------------------------------------------------
//  Convenience macros – they obfuscate the literal strings at compile time
// ---------------------------------------------------------------------------
#define HOOK(off, detour, orig) \
    hook(reinterpret_cast<void*>(g_il2cppBaseMap.startAddress + \
                string2Offset(OBFUSCATE(off))), \
         reinterpret_cast<void*>(detour), \
         reinterpret_cast<void**>(&orig))

#define PATCH(off, hex) \
    patchOffset(string2Offset(OBFUSCATE(off)), OBFUSCATE(hex), true)

#define PATCH_SWITCH(off, hex, enable) \
    patchOffset(string2Offset(OBFUSCATE(off)), OBFUSCATE(hex), enable)

#define RESTORE(off) \
    patchOffset(string2Offset(OBFUSCATE(off)), "", false)

#endif // ZYGISKPG_MISC_H