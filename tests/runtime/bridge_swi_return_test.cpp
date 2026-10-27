// Authored instructions only: exercise the real miss/bridge path without a BIOS/ROM.
#include "gba_bus.h"
#include "gba_ppu.h"
#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include "runtime_context.h"
#include "self_heal.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

extern "C" uint32_t g_irq_nest_depth;
extern "C" uint32_t g_irq_iret_depth;
extern "C" unsigned long long g_runtime_irq_entries;
namespace {
constexpr uint32_t continuation = 0x03001000, exit_pc = 0x03002000;
bool bridge_bios = false;
unsigned native_calls = 0, failures = 0;
void check(bool ok, const char* label) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", label); ++failures; }
}
void bios_entry() {
    if (bridge_bios) { runtime_dispatch_miss(8); return; }
    // Native translation of the one authored MOVS PC,LR at synthetic vector 8.
    const auto target = g_cpu.R[14] & ~1u;
    const auto cycles = 3u + runtime_exception_return_refill(target);
    g_cpu.R[15] = target;
    runtime_tick(cycles);
    runtime_exception_return(target);
}
void caller_continuation() {
    ++native_calls;
    if (runtime_should_yield()) return;
    const bool thumb = (g_cpu.cpsr & CPSR_T_BIT) != 0;
    const auto old = g_cpu.R[0];
    g_cpu.R[0] = old + 1;
    if (thumb) arm_set_nzcv_add(old, 1, g_cpu.R[0]);
    g_cpu.R[15] = continuation + (thumb ? 2u : 4u);
    runtime_tick(1);
    const auto target = g_cpu.R[14];
    g_cpu.R[15] = target & ~1u;
    if (target & 1u) g_cpu.cpsr |= CPSR_T_BIT;
    else g_cpu.cpsr &= ~CPSR_T_BIT;
    runtime_tick(3u + runtime_refill_cycles(target, target & 1u));
    runtime_call_should_return(target);
}
class AuthoredBus : public gba::GbaBus {
public:
    std::array<uint32_t, 16> bios{};
    unsigned observed_irq_floor = 0;
    uint32_t read32(uint32_t addr) override {
        if (addr == 0x18) observed_irq_floor = runtime_call_stack_floor();
        if (addr < bios.size() * 4u) return bios[addr / 4u];
        return gba::GbaBus::read32(addr);
    }
};
struct Fixture {
    std::unique_ptr<AuthoredBus> bus = std::make_unique<AuthoredBus>();
    std::unique_ptr<gba::GbaPpu> ppu = std::make_unique<gba::GbaPpu>();
    Fixture(bool thumb, unsigned depth, unsigned irq_depth = 0) {
        bus->bios[2] = 0xE1B0F00Eu; // MOVS PC, LR
        if (thumb) {
            bus->write16(continuation, 0x3001u); // ADDS R0,#1
            bus->write16(continuation + 2, 0x4770u); // BX LR
        } else {
            bus->write32(continuation, 0xE2800001u); // ADD R0,R0,#1
            bus->write32(continuation + 4, 0xE12FFF1Eu); // BX LR
        }
        bus->io().set_bus(bus.get()); bus->io().set_ppu(ppu.get());
        gbarecomp::set_active_bus(bus.get()); gbarecomp::set_active_ppu(ppu.get());
        runtime_init(bus.get()); gbarecomp::runtime_restore_timing_context({});
        g_irq_nest_depth = irq_depth; g_irq_iret_depth = 0;
        g_cpu = {}; g_cpu.cpsr = 0x1fu | (thumb ? CPSR_T_BIT : 0u);
        g_cpu.R[0] = 10; g_cpu.R[14] = exit_pc | unsigned(thumb);
        g_cpu.R[15] = continuation; g_cpu.R[13] = 0x03007e00;
        g_cpu.banked_sp[ARM_BANK_SUPERVISOR] = 0x03007d00;
        for (unsigned i = 0; i < depth; ++i) runtime_call_push_return(exit_pc);
        native_calls = 0;
    }
    ~Fixture() {
        g_irq_nest_depth = g_irq_iret_depth = 0;
        gbarecomp::set_active_bus(nullptr); gbarecomp::set_active_ppu(nullptr);
        runtime_shutdown();
    }
};
struct Outcome {
    ArmCpuState cpu; unsigned depth; unsigned long long cycles;
    uint64_t io_hash, audio_samples, frames, pending_cycles;
    uint32_t vcount, next_ppu_event;
    int64_t event_budget;
};
Outcome run(bool bridge, bool thumb, unsigned depth, unsigned irq_depth, bool finish, bool deliver_irq = false) {
    Fixture fixture(thumb, depth, irq_depth);
    bridge_bios = bridge;
    const auto irq_entries_before = g_runtime_irq_entries;
    if (deliver_irq) {
        fixture.bus->bios[6] = 0xE3A00008u; // 18 MOV R0,#Timer0
        fixture.bus->bios[7] = 0xE59F1004u; // 1C LDR R1,[PC,#4]
        fixture.bus->bios[8] = 0xE1C100B0u; // 20 STRH R0,[R1] (ack IF)
        fixture.bus->bios[9] = 0xE25EF004u; // 24 SUBS PC,LR,#4
        fixture.bus->bios[10] = 0x04000202u;
        fixture.bus->write16(0x04000200u, gba::GbaIo::IrqTimer0);
        fixture.bus->write16(0x04000208u, 1);
        fixture.bus->io().request_irq(gba::GbaIo::IrqTimer0);
    }
    const auto before = gbarecomp::self_heal_interpreted_insns();
    runtime_swi(thumb ? 6u : 0x060000u);
    check(g_cpu.R[15] == continuation && g_cpu.R[0] == 10,
          "SWI bridge leaves caller instructions for native execution");
    check(runtime_call_stack_depth() == depth, "SWI does not consume its caller's BL frame");
    check(native_calls == 0, "SWI itself does not call the native continuation");
    if (bridge) check(gbarecomp::self_heal_interpreted_insns() - before == (deliver_irq ? 5u : 1u),
                      "only BIOS return plus any delivered IRQ wrapper is interpreted");
    std::vector<gbarecomp::RuntimeSwiContinuation> ledger;
    gbarecomp::runtime_capture_swi_context(ledger);
    check(ledger.empty(), "SWI ledger consumed exactly once");
    if (finish && g_cpu.R[15] == continuation) {
        // Generated callers cancel their outstanding BL frames when a tail SWI
        // unwinds at an interior continuation; the outer loop redispatches PC.
        for (unsigned i = 0; i < depth; ++i) runtime_call_cancel_return(exit_pc);
        if (deliver_irq) runtime_call_push_return(exit_pc);
        runtime_dispatch(g_cpu.R[15]);
        check(native_calls == 1 && g_cpu.R[0] == 11 && g_cpu.R[15] == exit_pc,
              "generated unwind/redispatch executes continuation exactly once");
    }
    if (deliver_irq) {
        check(g_runtime_irq_entries - irq_entries_before == 1 && g_irq_nest_depth == 0,
              "pending IRQ delivered and completed once after SWI return");
        check(fixture.bus->io().if_reg() == 0 && fixture.bus->observed_irq_floor == 1,
              "IRQ bridge acknowledges IF and protects the open native call frame");
    }
    gbarecomp::RuntimeTimingContext timing;
    gbarecomp::runtime_capture_timing_context(timing);
    return {g_cpu, runtime_call_stack_depth(), g_runtime_cycles,
            fixture.bus->io().cosim_hash(), fixture.bus->audio().samples_generated(),
            fixture.ppu->frame_count(), timing.pending_cycles, fixture.ppu->vcount(),
            fixture.ppu->cycles_until_next_event(), timing.event_budget};
}
void differential() {
    for (bool thumb : {false, true}) for (unsigned depth : {0u, 1u, 3u})
        for (unsigned irq_depth : {0u, 1u}) for (bool finish : {false, true}) {
            const auto native = run(false, thumb, depth, irq_depth, finish);
            const auto bridged = run(true, thumb, depth, irq_depth, finish);
            check(std::memcmp(&native.cpu, &bridged.cpu, sizeof(native.cpu)) == 0,
                  "bridged/native SWI CPU and banked registers match");
            check(native.depth == bridged.depth && native.cycles == bridged.cycles,
                  "bridged/native SWI call stack and guest cycles match");
            check(native.io_hash == bridged.io_hash && native.audio_samples == bridged.audio_samples &&
                  native.frames == bridged.frames && native.pending_cycles == bridged.pending_cycles &&
                  native.vcount == bridged.vcount && native.next_ppu_event == bridged.next_ppu_event &&
                  native.event_budget == bridged.event_budget,
                  "bridged/native device state and scheduler horizon match");
        }
}
void actual_irq_delivery() {
    for (bool thumb : {false, true}) {
        const auto bridged = run(true, thumb, 1, 0, true, true);
        // Preserve the bridge's existing instruction-boundary IRQ delivery.
        // Its interpreter restores SPSR before ticking the SVC return, so the
        // pending IRQ arrives at the post-SWI PC, before the caller's ADD.
        // Native BIOS code ticks before SPSR restore; parity of that separate
        // preexisting timing difference is outside this boundary-only fix.
        check(bridged.cpu.banked_lr[ARM_BANK_IRQ] == continuation + 4u &&
              bridged.cpu.banked_spsr[ARM_BANK_IRQ] == (0x1fu | (thumb ? CPSR_T_BIT : 0u)),
              "pending IRQ retains the existing bridge's return PC and saved CPSR");
    }
}
void forced_stop_and_ordinary_subtree() {
    for (bool forced : {false, true}) {
        Fixture fixture(false, 1);
        uint32_t entry = continuation;
        if (forced) {
            // Explicit validation mode must still interpret through its stop.
            g_cpu.cpsr = 0x93; g_cpu.R[14] = continuation;
            g_cpu.banked_lr[ARM_BANK_USER] = exit_pc;
            g_cpu.banked_spsr[ARM_BANK_SUPERVISOR] = 0x1f;
            entry = 8;
        }
        const int result = runtime_bridge_interpret(entry, false, forced ? exit_pc : 0, 16);
        check(result == 1 && g_cpu.R[15] == exit_pc && g_cpu.R[0] == 11,
              "ordinary and forced-stop bridges keep their whole-subtree contract");
        check(runtime_call_stack_depth() == 0, "ordinary subtree consumes its own BL frame");
    }
}
void nested_swi() {
    Fixture fixture(false, 1);
    fixture.bus->bios = {};
    const uint32_t code[] = {
        0xE3500000u, // 08 CMP R0,#0
        0x1A000006u, // 0C BNE 2C (nested return)
        0xE14F2000u, // 10 MRS R2,SPSR
        0xE92D4004u, // 14 STMDB SP!,{R2,LR}
        0xE3A00001u, // 18 MOV R0,#1
        0xEF000006u, // 1C SWI 6 (ARM immediate form is immaterial to synthetic code)
        0xE8BD4004u, // 20 LDMIA SP!,{R2,LR}
        0xE16FF002u, // 24 MSR SPSR_fsxc,R2
        0xE1B0F00Eu, // 28 MOVS PC,LR (outer return)
        0xE1B0F00Eu  // 2C MOVS PC,LR (nested return)
    };
    std::copy(std::begin(code), std::end(code), fixture.bus->bios.begin() + 2);
    bridge_bios = true; g_cpu.R[0] = 0;
    runtime_swi(0x060000u);
    check(g_cpu.R[15] == continuation && g_cpu.R[0] == 1 && g_cpu.cpsr == 0x1f,
          "nested SVC returns stop at each SWI's own boundary");
    check(runtime_call_stack_depth() == 1 && g_cpu.R[13] == 0x03007e00 &&
              g_cpu.banked_sp[ARM_BANK_SUPERVISOR] == 0x03007d00,
          "nested SWIs preserve caller and both stack banks");
    std::vector<gbarecomp::RuntimeSwiContinuation> ledger;
    gbarecomp::runtime_capture_swi_context(ledger);
    check(ledger.empty(), "nested SWI ledgers balanced");
}
} // namespace
struct DispatchEntry { uint32_t addr; uint8_t thumb, resume; void (*fn)(); };
extern "C" const DispatchEntry kDispatchTable[] = {
    {continuation, 0, 1, caller_continuation}, {continuation, 1, 1, caller_continuation}};
extern "C" const unsigned kDispatchTableLen = 2;
// Router chooses a native synthetic BIOS or deliberately enters the real miss
// path at the same vector. No real BIOS bytes or service implementation used.
extern "C" const DispatchEntry kBiosDispatchTable[] = {{8, 0, 0, bios_entry}};
extern "C" const unsigned kBiosDispatchTableLen = 1;
int main() {
#if defined(_WIN32)
    _putenv_s("GBARECOMP_SELFHEAL_RECOMPILE", "0");
    _putenv_s("GBARECOMP_YIELD_ON_VBLANK", "0");
#else
    setenv("GBARECOMP_SELFHEAL_RECOMPILE", "0", 1);
    setenv("GBARECOMP_YIELD_ON_VBLANK", "0", 1);
#endif
    differential(); actual_irq_delivery(); forced_stop_and_ordinary_subtree(); nested_swi();
    std::printf("bridge_swi_return_tests: %u failures\n", failures);
    return failures ? 1 : 0;
}
