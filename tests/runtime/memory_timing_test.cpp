#include "gba_bus.h"
#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

// Exercise the exported ABI, active-bus rebinding and live wait-table updates.
// Independent expected costs use the public table, not the candidate helper.
class CustomBus : public gba::GbaBus {
  public:
    uint32_t access_cycles(uint32_t addr, uint8_t width, bool sequential) const override {
        return (addr & 0xffu) + width + (sequential ? 1000u : 2000u);
    }
};

int main() {
    gbarecomp::set_active_bus(nullptr);
    if (runtime_mem_cycles(0x02000000, 4, 0) != 1)
        return 1;
    auto first = std::make_unique<gba::GbaBus>();
    auto second = std::make_unique<gba::GbaBus>();
    const std::array<uint32_t, 9> widths{0, 1, 2, 3, 4, 8, 255, 256, 260};
    unsigned checks = 0;
    for (auto *bus : {first.get(), second.get(), first.get()}) {
        gbarecomp::set_active_bus(bus);
        for (uint32_t config = 0; config < 0x8000; ++config) {
            bus->write16(0x04000204, uint16_t(config));
            // Include accepted/rejected memory-control values and prefetch on/off.
            bus->write32(0x04000800, (config & 15u) << 24);
            bus->wait_table()->last_prefetched_pc = 0x08000124;
            const auto before = *bus->wait_table();
            for (uint32_t region = 0; region < 18; ++region) {
                const uint32_t addr = region == 17 ? 0xffffffffu : region << 24;
                const uint32_t r = addr >> 24;
                for (auto width : widths)
                    for (uint32_t seq : {0u, 1u, 2u}) {
                        const uint32_t expected =
                            r > 15 ? 1u
                                   : 1u + (r >= 14 ? before.n16[r]
                                           : uint8_t(width) == 4
                                               ? (seq ? before.s32[r] : before.n32[r])
                                               : (seq ? before.s16[r] : before.n16[r]));
                        const auto actual = runtime_mem_cycles(addr, width, seq);
                        if (actual != expected) {
                            std::fprintf(stderr,
                                         "config=%x addr=%x width=%u seq=%u got=%u expected=%u\n",
                                         config, addr, width, seq, actual, expected);
                            return 1;
                        }
                        ++checks;
                    }
            }
            if (std::memcmp(&before, bus->wait_table(), sizeof(before)))
                return 2;
        }
    }
    gbarecomp::set_active_bus(nullptr);
    if (runtime_mem_cycles(0x08000000, 4, 1) != 1)
        return 3;
    auto custom = std::make_unique<CustomBus>();
    gbarecomp::set_active_bus(custom.get());
    if (runtime_mem_cycles(0x02000023, 260, 2) != 1039u) {
        std::fputs("FAIL: runtime_mem_cycles bypassed virtual bus timing\n", stderr);
        return 4;
    }
    gbarecomp::set_active_bus(nullptr);
    std::printf("memory_timing_tests: %u comparisons PASS\n", checks);
}
