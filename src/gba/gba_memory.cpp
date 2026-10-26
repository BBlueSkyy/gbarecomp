// gba_memory.cpp — memory map classification + mirror resolution.
//
// This file is intentionally small and reference-implementation style.
// The hot bus path (gba_bus.cpp) consults `classify` and `resolve_offset`
// once and caches per-region pointers; codegen-emitted loads / stores
// go through the cached pointer except for IO and ROM-with-waitstate
// regions.

#include "gba_memory.h"

namespace gba {

const char* region_name(Region r) noexcept {
    switch (r) {
        case Region::Bios:    return "BIOS";
        case Region::Ewram:   return "EWRAM";
        case Region::Iwram:   return "IWRAM";
        case Region::Io:      return "IO";
        case Region::Pal:     return "PAL";
        case Region::Vram:    return "VRAM";
        case Region::Oam:     return "OAM";
        case Region::Rom:     return "ROM";
        case Region::Save:    return "SAVE";
        case Region::OpenBus: return "OPENBUS";
        case Region::Unknown: return "UNKNOWN";
    }
    return "??";
}


}  // namespace gba
