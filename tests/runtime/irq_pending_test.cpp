#include "gba_io.h"
#include <cstdio>

extern "C" {
unsigned long long g_runtime_cycles = 0;
}
extern "C" uint32_t runtime_current_pc() { return 0x08000100; }

int main() {
    gba::GbaIo io;
    for (unsigned requested = 0; requested < 14; ++requested) {
        io.write16(gba::IoReg::IF, 0x3fff);
        io.request_irq(uint16_t(1u << requested));
        for (unsigned enabled = 0; enabled < 14; ++enabled) {
            io.write16(gba::IoReg::IE, uint16_t(1u << enabled));
            for (uint16_t ime : {uint16_t(0), uint16_t(1), uint16_t(2), uint16_t(0xffff)}) {
                io.write16(gba::IoReg::IME, ime);
                const auto before = io.if_reg();
                for (unsigned repeat = 0; repeat < 3; ++repeat) {
                    if (io.irq_pending() != (requested == enabled && (ime & 1u)))
                        return 1;
                    if (io.if_reg() != before)
                        return 2;
                }
            }
        }
        io.write16(gba::IoReg::IE, uint16_t(1u << requested));
        io.write16(gba::IoReg::IME, 1);
        if (!io.irq_pending())
            return 3;
        io.write16(gba::IoReg::IF, uint16_t(1u << requested));
        if (io.irq_pending())
            return 4;
    }
    std::puts("irq_pending_tests: all 14 sources, masks, IME and W1C PASS");
}
