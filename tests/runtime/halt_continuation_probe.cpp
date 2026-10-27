// Investigation only. No BIOS/ROM bytes and no production HALT implementation.
// Tests whether the existing single-machine snapshot can transport the explicit
// IRQ continuation that a resumable HALT-in-IRQ implementation would require.
#include "gba_bus.h"
#include "gba_ppu.h"
#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include "runtime_context.h"
#include "snapshot.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

extern "C" uint32_t g_irq_nest_depth, g_irq_iret_depth;
struct DispatchEntry { uint32_t addr; uint8_t thumb, resume; void (*fn)(); };
extern "C" const DispatchEntry kDispatchTable[] = {{0, 0, 0, nullptr}};
extern "C" const unsigned kDispatchTableLen = 0;
namespace {
unsigned failures = 0;
void check(bool ok, const char* message) {
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}
struct Fixture {
    std::unique_ptr<gba::GbaBus> bus = std::make_unique<gba::GbaBus>();
    std::unique_ptr<gba::GbaPpu> ppu = std::make_unique<gba::GbaPpu>();
    Fixture() {
        bus->io().set_bus(bus.get()); bus->io().set_ppu(ppu.get());
        bus->rtc().set_diagnostic_host_seconds(0);
        gbarecomp::set_active_bus(bus.get()); gbarecomp::set_active_ppu(ppu.get());
        runtime_init(bus.get()); gbarecomp::runtime_restore_timing_context({});
        g_irq_nest_depth = g_irq_iret_depth = 0;
        gbarecomp::runtime_set_resumable_context(nullptr);
        g_cpu = {}; g_cpu.cpsr = 0x1f; g_cpu.R[15] = 0x03001000;
        g_cpu.R[13] = 0x03007e00; g_cpu.R[0] = 0x12345678;
        g_cpu.banked_sp[ARM_BANK_IRQ] = 0x03007c00;
    }
    ~Fixture() {
        gbarecomp::runtime_set_resumable_context(nullptr);
        g_irq_nest_depth = g_irq_iret_depth = 0;
        gbarecomp::set_active_bus(nullptr); gbarecomp::set_active_ppu(nullptr);
        runtime_shutdown();
    }
    gbarecomp::debug::SnapshotContext snapshot() {
        gbarecomp::debug::SnapshotContext c;
        c.bus = bus.get(); c.ppu = ppu.get();
        c.rom_sha1 = "0000000000000000000000000000000000000000";
        return c;
    }
};

void inspect_irq_snapshot() {
    Fixture f;
    gbarecomp::RuntimeArmContext execution;
    runtime_call_push_return(0x03002000);
    gbarecomp::runtime_set_resumable_context(&execution);
    gbarecomp::runtime_set_return_yield(true);
    // Exercise the real asynchronous IRQ-entry machinery, which owns explicit
    // preserved registers/floor/return depth instead of a synchronous C++ frame.
    runtime_irq(g_cpu.R[15]);
    check(execution.interrupts.size() == 1 && g_irq_nest_depth == 1 &&
          runtime_call_stack_floor() == 1, "actual IRQ entry creates an explicit continuation");
    if (execution.interrupts.empty()) return;
    gbarecomp::runtime_set_return_yield(false);
    // Model a clean paused IRQ dispatch boundary. These are authored state
    // inputs, NOT evidence that the production bridge already suspends at HALT.
    g_cpu.R[15] = 0x0000001c;
    f.bus->write8(0x04000301, 0);
    std::vector<uint8_t> first, second;
    std::string error;
    const auto context = f.snapshot();
    check(gbarecomp::debug::save_state_bytes(&first, context, &error), "save paused IRQ state");
    const auto preserved = execution.interrupts.front().preserved[0];
    execution.interrupts.front().preserved[0] ^= 0xffffffffu;
    check(gbarecomp::debug::save_state_bytes(&second, context, &error), "save changed IRQ continuation");
    const bool omitted = first == second;
    check(omitted, "probe observes identical snapshots despite different IRQ preserved R0");
    std::printf("snapshot_irq_continuation_omitted=%u bytes=%zu\n", omitted, first.size());

    // A fresh owner has no C++ frame or explicit continuation to reconstruct.
    // CPU, HALT, BL and SWI payloads can load while IRQ ownership is missing.
    gbarecomp::runtime_set_resumable_context(nullptr);
    gbarecomp::RuntimeArmContext fresh;
    fresh.cpu.cpsr = 0x1f;
    gbarecomp::runtime_restore_arm_context(fresh);
    gbarecomp::runtime_set_resumable_context(&fresh);
    check(gbarecomp::debug::load_state_bytes(first.data(), first.size(), context, &error),
          "existing loader accepts the snapshot");
    check(f.bus->io().halted() && (g_cpu.cpsr & 0x1f) == 0x12 &&
          runtime_call_stack_depth() == 1, "CPU/HALT/BL payloads survive snapshot load");
    const bool missing = fresh.interrupts.empty() && g_irq_nest_depth == 0 &&
                         runtime_call_stack_floor() == 0;
    check(missing, "IRQ continuation/depth/floor are absent after fresh-owner load");
    std::printf("load cpu_mode=%02x halted=%u BL=%u IRQ_depth=%u floor=%u continuations=%zu original_R0=%08x\n",
                g_cpu.cpsr & 0x1f, f.bus->io().halted(), runtime_call_stack_depth(),
                g_irq_nest_depth, runtime_call_stack_floor(), fresh.interrupts.size(), preserved);
}

void inspect_masked_wake() {
    for (bool ime : {false, true}) for (bool masked : {false, true}) {
        Fixture f;
        // Keep all cases masked at the CPU or IME, so no BIOS IRQ is dispatched.
        // The fully enabled case is covered by existing IRQ-delivery tests.
        if (ime && !masked) continue;
        g_cpu.cpsr = 0x1f | (masked ? CPSR_I_BIT : 0u);
        f.bus->write16(0x04000200, gba::GbaIo::IrqVBlank);
        f.bus->write16(0x04000208, ime ? 1 : 0);
        f.bus->io().request_irq(gba::GbaIo::IrqVBlank);
        f.bus->write8(0x04000301, 0);
        const auto pc = g_cpu.R[15];
        runtime_tick(1);
        const bool remains_halted = f.bus->io().halted();
        check(remains_halted && g_cpu.R[15] == pc && g_runtime_cycles == 1,
              "probe observes existing legacy masked-wake behavior without CPU execution");
        std::printf("masked_wake IE&IF=%04x IME=%u CPSR.I=%u halted=%u cycles=%llu\n",
                    f.bus->io().ie() & f.bus->io().if_reg(), ime, masked,
                    remains_halted, g_runtime_cycles);
    }
}
} // namespace
int main() {
    inspect_irq_snapshot(); inspect_masked_wake();
    std::printf("halt_continuation_probe: %u unexpected results\n", failures);
    return failures ? 1 : 0;
}
