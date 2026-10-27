// Continuation regression, adapted from PR #8 halt_continuation_probe.
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
#include <cstring>
#include <memory>
#include <string>
#include <vector>

extern "C" uint32_t g_irq_nest_depth, g_irq_iret_depth;
extern "C" unsigned long long g_runtime_irq_entries;
struct DispatchEntry { uint32_t addr; uint8_t thumb, resume; void (*fn)(); };
namespace {
unsigned failures = 0;
void check(bool ok, const char* message) {
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}
class AuthoredBus : public gba::GbaBus {
public:
    uint32_t read32(uint32_t pc) override {
        if (pc == 0x100 || pc == 0x10c) return 0xe1b0f00e; // MOVS PC,LR
        if (pc == 0x108) return 0xe321f093; // MSR CPSR_c,#SVC|I
        return gba::GbaBus::read32(pc);
    }
};
struct Fixture {
    std::unique_ptr<AuthoredBus> bus = std::make_unique<AuthoredBus>();
    std::unique_ptr<gba::GbaPpu> ppu = std::make_unique<gba::GbaPpu>();
    gbarecomp::RuntimeArmContext owner, replacement;
    Fixture() {
        bus->set_rom(nullptr, 0); // same mapper initialization as real bring-up
        bus->io().set_bus(bus.get()); bus->io().set_ppu(ppu.get());
        bus->rtc().set_diagnostic_host_seconds(0);
        gbarecomp::set_active_bus(bus.get()); gbarecomp::set_active_ppu(ppu.get());
        runtime_init(bus.get()); gbarecomp::runtime_restore_timing_context({});
        g_irq_nest_depth = g_irq_iret_depth = 0;
        gbarecomp::runtime_set_resumable_context(nullptr);
        gbarecomp::runtime_set_owned_context(&owner);
        g_runtime_irq_entries = 0;
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
    check(!omitted, "different IRQ preserved registers produce distinct snapshots");
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
    check(!missing && fresh.interrupts.size() == 1 && g_irq_nest_depth == 1 &&
          runtime_call_stack_floor() == 1 && fresh.interrupts.front().preserved[0] == preserved,
          "fresh-owner load restores the full IRQ continuation");
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
constexpr uint32_t main_pc = 0x03001000, main_return = 0x03002000, helper_return = 0x03004000;
enum class Scenario { Irq, Swi, NestedIrq, NestedIrqSwi, MainSwi, NestedSwi, SystemSwi };
Scenario scenario;
bool pause_enabled, paused;
unsigned native_calls;
Fixture* live_save_probe = nullptr;
bool entry_save_probe = false;
int inspect_entry_read(uint32_t, uint32_t address, uint32_t, uint32_t, uint32_t*) {
    if (address == 0x04000200 && entry_save_probe) {
        std::vector<uint8_t> out{7}; std::string error;
        check(!gbarecomp::debug::save_state_bytes(&out, live_save_probe->snapshot(), &error) &&
              error.find("active IRQ dispatch") != std::string::npos && out==std::vector<uint8_t>{7},
              "atomic IRQ entry is not accepted as a save boundary");
        entry_save_probe=false;
    }
    return 0;
}
void trusted_hook_a(uint32_t) {}
void trusted_hook_b(uint32_t) {}
bool same_bytes(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a == b) return true;
    std::size_t i = 0;
    while (i < a.size() && i < b.size() && a[i] == b[i]) ++i;
    std::fprintf(stderr, "snapshot mismatch scenario=%u sizes=%zu/%zu offset=%zu value=%u/%u\n",
                 unsigned(scenario), a.size(), b.size(), i,
                 i<a.size()?a[i]:0, i<b.size()?b[i]:0);
    return false;
}
std::vector<uint8_t> save(Fixture& f) {
    std::vector<uint8_t> bytes; std::string error;
    check(gbarecomp::debug::save_state_bytes(&bytes, f.snapshot(), &error), error.c_str());
    return bytes;
}
void maybe_pause() {
    if (pause_enabled && !paused) { paused = true; gbarecomp::runtime_suspend_dispatch(); }
}
void irq_finish() {
    if (scenario == Scenario::Swi) runtime_call_cancel_return(helper_return);
    g_cpu.R[0] = 0xfedcba98; g_cpu.R[1] = 99; g_cpu.R[12] = 17;
    const auto pc = g_cpu.R[14] - 4;
    g_cpu.R[15] = pc;
    runtime_tick(3 + runtime_exception_return_refill(pc));
    runtime_exception_return(pc);
    gbarecomp::active_bus()->set_bios_access_enabled(g_cpu.R[15] < 0x4000);
}
void outer_irq_finish() {
    g_cpu.R[14] = bus_read_u32(0x03007000);
    runtime_msr_spsr(bus_read_u32(0x03007004), 0xffffffffu);
    runtime_call_cancel_return(helper_return);
    irq_finish();
}
void outer_swi_finish() {
    g_cpu.R[14] = bus_read_u32(0x03007008);
    runtime_msr_spsr(bus_read_u32(0x0300700c), 0xffffffffu);
    runtime_bridge_interpret(0x100, false, 0, 0);
}
void swi_entry() {
    if (scenario == Scenario::NestedSwi && gbarecomp::runtime_swi_depth() == 1) {
        bus_write_u32(0x03007008, g_cpu.R[14]);
        bus_write_u32(0x0300700c, runtime_mrs_spsr());
        g_cpu.R[15] = 0x104; runtime_swi(0x060000); return;
    }
    g_cpu.R[0] ^= 0x01020304;
    if (scenario == Scenario::SystemSwi) runtime_msr_cpsr(0x1f, 0xff);
    g_cpu.R[15] = scenario == Scenario::SystemSwi ? 0x108 : 0x100;
    runtime_tick(2); maybe_pause();
    runtime_bridge_interpret(g_cpu.R[15], false, 0, 0);
}
void irq_entry() {
    if (live_save_probe) {
        std::vector<uint8_t> out{7}; std::string error;
        check(!gbarecomp::debug::save_state_bytes(&out, live_save_probe->snapshot(), &error) &&
              error.find("active IRQ dispatch") != std::string::npos && out==std::vector<uint8_t>{7},
              "live IRQ driver is not accepted as a save boundary");
        live_save_probe=nullptr;
    }
    bus_write_u16(0x04000202, gba::GbaIo::IrqVBlank);
    if (g_irq_nest_depth == 1 &&
        (scenario == Scenario::NestedIrq || scenario == Scenario::NestedIrqSwi)) {
        // Guest memory owns bank values overwritten by nested hardware IRQs.
        bus_write_u32(0x03007000, g_cpu.R[14]);
        bus_write_u32(0x03007004, runtime_mrs_spsr());
        runtime_call_push_return(helper_return);
        g_cpu.R[15] = 0x90; runtime_irq(0x90); return;
    }
    if (scenario == Scenario::Swi || scenario == Scenario::NestedIrqSwi) {
        if (scenario == Scenario::Swi) runtime_call_push_return(helper_return);
        g_cpu.R[15] = 0x84; runtime_swi(0x060000); return;
    }
    g_cpu.R[0] = 0xdead0000 | g_irq_nest_depth;
    g_cpu.R[15] = 0x80; runtime_tick(2); maybe_pause(); irq_finish();
}
void main_continuation() {
    ++native_calls; ++g_cpu.R[4]; g_cpu.R[15] = g_cpu.R[14] & ~1u;
    runtime_tick(4); runtime_call_should_return(g_cpu.R[15]);
    gbarecomp::active_bus()->set_bios_access_enabled(false);
}
std::vector<uint8_t> run_resume(Scenario which, bool reload, bool thumb) {
    Fixture f; scenario = which; pause_enabled = reload; paused = false; native_calls = 0;
    g_cpu.cpsr = 0x6000001f | (thumb ? CPSR_T_BIT : 0u);
    for (unsigned i = 0; i < 13; ++i) g_cpu.R[i] = 0x11223300 + i;
    g_cpu.R[14] = main_return | unsigned(thumb);
    g_cpu.banked_sp[ARM_BANK_SUPERVISOR] = 0x03007d00;
    g_cpu.banked_lr[ARM_BANK_SUPERVISOR] = 0x11110000;
    g_cpu.banked_spsr[ARM_BANK_SUPERVISOR] = 0x5f;
    runtime_call_push_return(main_return); g_runtime_fn_entry_hook = trusted_hook_a;
    runtime_tick(13);
    const bool main_swi = which == Scenario::MainSwi || which == Scenario::NestedSwi || which == Scenario::SystemSwi;
    try {
        if (main_swi) runtime_swi(0x060000); else runtime_irq(main_pc);
    } catch (const gbarecomp::RuntimeDispatchYield&) {
        check(reload && paused, "authored dispatch yields only at requested boundary");
    }
    if (reload) {
        check(paused && !gbarecomp::runtime_irq_driver_active(), "IRQ host frames have unwound");
        const auto before = save(f);
        gbarecomp::RuntimeArmContext expected;
        gbarecomp::runtime_capture_arm_context(expected);
        gbarecomp::RuntimeTimingContext expected_time;
        gbarecomp::runtime_capture_timing_context(expected_time);
        gbarecomp::runtime_set_owned_context(&f.replacement);
        gbarecomp::RuntimeArmContext empty; gbarecomp::runtime_restore_arm_context(empty);
        gbarecomp::runtime_restore_timing_context({}); g_runtime_fn_entry_hook = trusted_hook_b;
        f.owner.interrupts.clear(); f.owner.swi_returns.clear(); // poison previous owner
        std::string error;
        check(gbarecomp::debug::load_state_bytes(before.data(), before.size(), f.snapshot(), &error),
              "load into a different owner");
        check(g_runtime_fn_entry_hook == trusted_hook_b && gbarecomp::runtime_owned_context() == &f.replacement,
              "restoration retains current trusted wiring, never saved host pointers");
        gbarecomp::RuntimeArmContext restored; gbarecomp::runtime_capture_arm_context(restored);
        check(std::memcmp(&expected.cpu, &restored.cpu, sizeof(g_cpu)) == 0 &&
              expected.returns == restored.returns && expected.swi_returns.size() == restored.swi_returns.size() &&
              expected.interrupts.size() == restored.interrupts.size() &&
              expected.return_floor == restored.return_floor && expected.irq_depth == restored.irq_depth &&
              expected.iret_depth == restored.iret_depth, "CPU, banks and continuation limits restored");
        check(same_bytes(save(f), before), "complete snapshot bytes match immediately after load");
        check(g_runtime_cycles == expected_time.cycles, "load restores master cycle clock");
    }
    for (unsigned guard = 0; guard < 8 && (g_irq_nest_depth || gbarecomp::runtime_swi_depth()); ++guard) {
        if (g_irq_nest_depth) gbarecomp::runtime_resume_irq(); else runtime_dispatch(g_cpu.R[15]);
    }
    check(!g_irq_nest_depth && !gbarecomp::runtime_swi_depth() && runtime_call_stack_floor() == 0,
          "all nested IRQ/SWI continuations complete");
    check(g_cpu.R[15] == main_pc && runtime_call_stack_depth() == 1 && native_calls == 0,
          "IRQ/SVC completion preserves caller BL and leaves mainline native work unexecuted");
    runtime_dispatch(g_cpu.R[15]);
    check(native_calls == 1 && g_cpu.R[15] == main_return && runtime_call_stack_depth() == 0,
          "native caller executes once and consumes its own BL");
    return save(f);
}
void differential_execution() {
    for (auto which : {Scenario::Irq, Scenario::Swi, Scenario::NestedIrq, Scenario::NestedIrqSwi,
                       Scenario::MainSwi, Scenario::NestedSwi, Scenario::SystemSwi})
        for (bool thumb : {false, true}) {
            const auto reference = run_resume(which, false, thumb);
            const auto restored = run_resume(which, true, thumb);
            check(same_bytes(reference, restored), "synchronous and save/load execution produce identical final machine snapshots");
        }
}
#include "fixtures/snapshot_v4_rle.h"
void legacy_snapshot() {
    Fixture f; std::vector<uint8_t> old;
    for (const auto& run : snapshot_v4_runs) old.insert(old.end(), run.count, run.value);
    check(old.size() == 553456, "independent v4 baseline fixture expands correctly");
    gbarecomp::runtime_restore_timing_context({555,3,2,4,88});
    std::string error;
    check(gbarecomp::debug::load_state_bytes(old.data(), old.size(), f.snapshot(), &error), "load actual v4 writer fixture");
    check(g_cpu.R[0] == 0x12345678 && g_cpu.cpsr == 0x93 && f.bus->read32(0x03000010) == 0xaabbccdd &&
          runtime_call_stack_depth() == 2 && gbarecomp::runtime_swi_depth() == 1 && f.owner.interrupts.empty() &&
          g_irq_nest_depth == 0 && runtime_call_stack_floor() == 0, "v4 migrates CPU/BL/SWI with empty IRQ ownership");
    gbarecomp::RuntimeTimingContext t; gbarecomp::runtime_capture_timing_context(t);
    check(t.cycles == 98765 && t.pending_cycles == 0 && t.event_budget == 0,
          "v4 retains META clock and clears unavailable lazy timing debt");
    runtime_dispatch(g_cpu.R[15]);
    check(g_cpu.R[15] == main_pc && (g_cpu.cpsr & CPSR_T_BIT) &&
          runtime_call_stack_depth() == 2 && !gbarecomp::runtime_swi_depth(),
          "migrated SVC resumes at internal BIOS PC without consuming BL");
}
uint32_t word(const std::vector<uint8_t>& b, std::size_t i) {
    return uint32_t(b[i]) | uint32_t(b[i+1])<<8 | uint32_t(b[i+2])<<16 | uint32_t(b[i+3])<<24;
}
void patch_word(std::vector<uint8_t>& b, std::size_t i, uint32_t value) {
    for (unsigned j=0;j<4;++j) b[i+j] = uint8_t(value >> (j*8));
}
std::size_t section(const std::vector<uint8_t>& b, const char* tag) {
    std::size_t cursor = 52;
    for (unsigned i=0;i<word(b,48);++i) {
        if (!std::memcmp(b.data()+cursor,tag,4)) return cursor+8;
        cursor += 8+word(b,cursor+4);
    }
    return 0;
}
void validation_and_clock() {
    Fixture f; runtime_call_push_return(main_return);
    gbarecomp::runtime_set_resumable_context(&f.owner);
    gbarecomp::runtime_set_return_yield(true); runtime_irq(main_pc);
    gbarecomp::runtime_set_owned_context(&f.owner);
    const auto bytes = save(f); const auto exec = section(bytes,"EXEC");
    check(exec != 0, "v5 contains explicit EXEC");
    if (!exec) return;
    // EXEC: schema, floor, depth, iret, resume, from_halt, entries64,
    // count, then five registers/floor/depth/iret for each 32-byte IRQ record.
    const std::pair<std::size_t,uint32_t> invalid[] = {
        {0,2}, {4,2}, {8,2}, {12,2}, {20,2}, {32,1025},
        {36+20,1}, {36+24,2}, {36+28,1}, {36+32+28,1}, {36+32+36,0xffffffffu}};
    for (auto [off,value] : invalid) {
        auto broken = bytes; patch_word(broken,exec+off,value);
        std::string error;
        check(!gbarecomp::debug::load_state_bytes(broken.data(),broken.size(),f.snapshot(),&error),
              "malformed continuation/clock metadata rejected");
        check(save(f) == bytes, "rejection occurs before any guest/context mutation");
    }
    for (unsigned version : {3u,4u}) {
        auto broken=bytes;patch_word(broken,4,version);std::string error;
        check(!gbarecomp::debug::load_state_bytes(broken.data(),broken.size(),f.snapshot(),&error),
              "unsupported version or v4 carrying v5 EXEC rejected");
        check(save(f)==bytes,"version rejection preserves all state");
    }
    auto missing=bytes;patch_word(missing,exec-8,0x54504f30u);std::string error;
    check(!gbarecomp::debug::load_state_bytes(missing.data(),missing.size(),f.snapshot(),&error),
          "v5 rejects missing EXEC");
    check(save(f)==bytes,"missing EXEC cannot partially load");
    auto advisory=bytes;patch_word(advisory,section(bytes,"META")+8,999);
    auto context=f.snapshot();context.cycles_elapsed=reinterpret_cast<uint64_t*>(&g_runtime_cycles);
    check(gbarecomp::debug::load_state_bytes(advisory.data(),advisory.size(),context,&error) && g_runtime_cycles==0,
          "v5 authoritative clock wins over aliased advisory META");
    auto no_meta=bytes;patch_word(no_meta,section(bytes,"META")-8,0x54504f30u);
    check(gbarecomp::debug::load_state_bytes(no_meta.data(),no_meta.size(),context,&error) && g_runtime_cycles==0,
          "v5 restores its clock without META");
}
void completed_iret() {
    for (unsigned depth : {1u,2u}) {
        Fixture f;gbarecomp::RuntimeArmContext state;
        state.cpu=g_cpu;state.cpu.R[15]=main_pc;state.returns={main_return};
        state.irq_depth=depth;state.iret_depth=depth;state.return_floor=1;
        for (unsigned i=0;i<depth;++i) {
            gbarecomp::RuntimeIrqContinuation frame;
            frame.preserved={100+i,200+i,300+i,400+i,500+i};
            frame.return_depth=1;frame.return_floor=i?1:0;frame.iret_depth=i;
            state.interrupts.push_back(frame);
        }
        gbarecomp::runtime_restore_arm_context(state);
        runtime_tick(9); // completed instruction charged BEFORE this clean boundary
        auto bytes=save(f);std::string error;
        check(gbarecomp::debug::load_state_bytes(bytes.data(),bytes.size(),f.snapshot(),&error),"load completed iret state");
        native_calls=0;
        for (unsigned i=0;i<depth;++i) gbarecomp::runtime_resume_irq();
        check(native_calls==0 && g_runtime_cycles==9 && g_cpu.R[15]==main_pc && g_cpu.R[0]==100 &&
              g_cpu.R[12]==500 && !g_irq_nest_depth && !g_irq_iret_depth && runtime_call_stack_depth()==1 &&
              runtime_call_stack_floor()==0,"completed nested iretdrivers restore metadata without executing caller or charging cycles");
    }
}
void prefetch_roundtrip() {
    Fixture f;f.bus->write16(0x04000204,0x4000);
    runtime_tick(1); // initialize the event horizon before accruing lazy debt
    runtime_tick(7);
    g_runtime_cycles=0; // established common-runner clock re-origin after load
    f.bus->wait_table()->last_prefetched_pc=0x08000100;
    const auto bytes=save(f);f.bus->wait_table()->last_prefetched_pc=0;
    std::string error;
    check(gbarecomp::debug::load_state_bytes(bytes.data(),bytes.size(),f.snapshot(),&error),"load prefetched state");
    check(f.bus->wait_table()->last_prefetched_pc==0x08000100 && save(f)==bytes,
          "rebuilding derived wait states preserves the serialized prefetch cursor");
    gbarecomp::RuntimeTimingContext t; gbarecomp::runtime_capture_timing_context(t);
    check(t.cycles==0 && t.pending_cycles==7, "clock re-origin does not discard prior lazy cycle debt");
}
void live_save_boundary() {
    Fixture f; scenario=Scenario::Irq;pause_enabled=false;
    live_save_probe=&f;entry_save_probe=true;g_runtime_bus_read_override=inspect_entry_read;
    runtime_irq(main_pc);g_runtime_bus_read_override=nullptr;
    check(!live_save_probe && !entry_save_probe,"live entry/driver save probes executed");
}
std::vector<uint8_t> paused_or_finished_irq(bool pause) {
    Fixture f;scenario=Scenario::Irq;pause_enabled=pause;paused=false;
    runtime_call_push_return(main_return);g_cpu.R[14]=main_return;runtime_tick(13);
    try { runtime_irq(main_pc); }
    catch (const gbarecomp::RuntimeDispatchYield&) { check(pause,"requested IRQ pause"); }
    if (!pause) runtime_dispatch(g_cpu.R[15]);
    return save(f);
}
void fresh_machine_restore() {
    const auto expected=paused_or_finished_irq(false);
    const auto paused_state=paused_or_finished_irq(true);
    Fixture fresh; std::string error;
    g_runtime_fn_entry_hook=trusted_hook_b;
    check(gbarecomp::debug::load_state_bytes(paused_state.data(),paused_state.size(),fresh.snapshot(),&error),
          "load after original bus, PPU and owner were destroyed");
    check(gbarecomp::runtime_owned_context()==&fresh.owner && gbarecomp::active_bus()==fresh.bus.get() &&
          gbarecomp::active_ppu()==fresh.ppu.get() && g_runtime_fn_entry_hook==trusted_hook_b,
          "loaded context uses current machine and trusted host callbacks");
    gbarecomp::runtime_resume_irq();
    runtime_dispatch(g_cpu.R[15]);
    check(same_bytes(save(fresh),expected),"fresh-machine continuation reaches identical full final state");
}
} // namespace
extern "C" const DispatchEntry kDispatchTable[] = {
    {main_pc,0,1,main_continuation}, {main_pc,1,1,main_continuation}};
extern "C" const unsigned kDispatchTableLen = 2;
extern "C" const DispatchEntry kBiosDispatchTable[] = {
    {8,0,0,swi_entry}, {0x18,0,0,irq_entry}, {0x80,0,1,irq_finish},
    {0x84,0,1,irq_finish}, {0x90,0,1,outer_irq_finish}, {0x104,0,1,outer_swi_finish}};
extern "C" const unsigned kBiosDispatchTableLen = 6;
int main() {
#if !defined(_WIN32)
    setenv("GBARECOMP_SELFHEAL_RECOMPILE", "0", 1);
    setenv("GBARECOMP_YIELD_ON_VBLANK", "0", 1);
#endif
    inspect_irq_snapshot(); inspect_masked_wake(); differential_execution(); legacy_snapshot();
    validation_and_clock(); completed_iret(); prefetch_roundtrip(); live_save_boundary(); fresh_machine_restore();
    std::printf("continuation_tests: %u failures\n", failures);
    return failures ? 1 : 0;
}
