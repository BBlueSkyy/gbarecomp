#include "gba_bus.h"
#include "gba_ppu.h"
#include "mobile_platform.h"
#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include "snapshot.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
struct DispatchEntry {
    uint32_t addr;
    uint8_t thumb, resume;
    void (*fn)();
};
static void entry() { ++g_cpu.R[0]; }
extern "C" const DispatchEntry kDispatchTable[] = {{0x08000100, 1, 0, entry}};
extern "C" const unsigned kDispatchTableLen = 1;
static void disable_frame_yield_for_fixture() {
#if defined(_WIN32)
    _putenv_s("GBARECOMP_YIELD_ON_VBLANK", "0");
#else
    setenv("GBARECOMP_YIELD_ON_VBLANK", "0", 1);
#endif
}
static double bench(const char *label, void (*fn)(), unsigned n) {
    const auto t = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < n; ++i)
        fn();
    const double ns =
        std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t).count() / n;
    std::printf("%s,%.3f\n", label, ns);
    return ns;
}
static void tick() { runtime_tick(1); }
static void yield() { g_cpu.R[0] += runtime_should_yield(); }
static void read() { g_cpu.R[0] = bus_read_u32(0x02000100); }
static void mem() { g_cpu.R[0] = runtime_mem_cycles(0x02000100, 4, 0); }
static void dispatch() { runtime_dispatch(0x08000100); }
static void alu() {
    g_cpu.R[15] = 0x08000100;
    if (runtime_should_yield())
        std::abort();
    g_cpu.R[0]++;
    arm_set_nz(g_cpu.R[0]);
    runtime_tick(1);
}
static void store() {
    g_cpu.R[15] = 0x08000100;
    if (runtime_should_yield())
        std::abort();
    bus_write_u32(0x02000104, ++g_cpu.R[1]);
    runtime_tick(1 + runtime_mem_cycles(0x02000104, 4, 0));
}
static void load() {
    g_cpu.R[15] = 0x08000100;
    if (runtime_should_yield())
        std::abort();
    g_cpu.R[0] = bus_read_u32(0x02000100);
    runtime_tick(1 + runtime_mem_cycles(0x02000100, 4, 0));
}
static void mixed() {
    alu();
    alu();
    alu();
    load();
    store();
}
int main() {
    disable_frame_yield_for_fixture();
    auto bus = std::make_unique<gba::GbaBus>();
    auto ppu = std::make_unique<gba::GbaPpu>();
    bus->rtc().set_diagnostic_host_seconds(0); // freeze host RTC only in this ROM-free fixture
    gbarecomp::set_active_bus(bus.get());
    gbarecomp::set_active_ppu(ppu.get());
    g_cpu.cpsr = 0x1f | CPSR_T_BIT | CPSR_I_BIT;
    g_cpu.R[15] = 0x08000100;
    gbarecomp::host_stack_guard_arm_current(1024 * 1024);
    for (int round = 0; round < 7; ++round) {
        bench("yield", yield, 1000000);
        bench("tick", tick, 1000000);
        bench("read-ewram", read, 1000000);
        bench("mem-cycles", mem, 1000000);
        bench("dispatch-hit", dispatch, 1000000);
        bench("alu-boundary", alu, 1000000);
        bench("load-boundary", load, 1000000);
        bench("mixed-5-boundaries", mixed, 200000);
    }
    std::printf("cycles=%llu frames=%llu pc=%x reg=%u\n", g_runtime_cycles,
                (unsigned long long)ppu->frame_count(), g_cpu.R[15], g_cpu.R[0]);
    gbarecomp::debug::SnapshotWriter state;
    bus->serialize(state);
    bus->io().serialize(state);
    bus->audio().serialize(state);
    ppu->serialize(state);
    state.bytes(&g_cpu, sizeof(g_cpu));
    uint64_t hash = 14695981039346656037ull;
    for (auto byte : state.buffer()) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    std::printf("state_fnv1a=%016llx bytes=%zu\n", (unsigned long long)hash, state.size());
    gbarecomp::set_active_bus(nullptr);
    gbarecomp::set_active_ppu(nullptr);
}
