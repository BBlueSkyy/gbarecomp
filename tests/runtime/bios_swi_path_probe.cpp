// ROM/BIOS-free investigation probe, NOT an implementation of any BIOS service.
// The authored vector either returns immediately or writes HALTCNT. SWI service
// names below identify routing inputs only; they do not claim service parity.
#include "bios_hle.h"
#include "gba_bus.h"
#include "gba_ppu.h"
#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include "runtime_context.h"
#include "self_heal.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

extern "C" unsigned long long g_runtime_irq_entries;
struct DispatchEntry { uint32_t addr; uint8_t thumb, resume; void (*fn)(); };
extern "C" const DispatchEntry kDispatchTable[] = {{0, 0, 0, nullptr}};
extern "C" const unsigned kDispatchTableLen = 0;
namespace {
constexpr uint32_t continuation = 0x03001000u;
unsigned failures = 0, decoded_number = ~0u, hook_calls = 0;
int (*real_hle)(uint32_t) = nullptr;
int observe_hle(uint32_t number) {
    decoded_number = number;
    ++hook_calls;
    return real_hle(number);
}
void check(bool ok, const char* label) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", label); ++failures; }
}
class AuthoredBus : public gba::GbaBus {
public:
    std::array<uint32_t, 32> code{};
    unsigned vector_fetches = 0;
    uint32_t read32(uint32_t address) override {
        if (address == 8) ++vector_fetches;
        if (address < code.size() * 4u) return code[address / 4u];
        return gba::GbaBus::read32(address);
    }
};
struct Fixture {
    std::unique_ptr<AuthoredBus> bus = std::make_unique<AuthoredBus>();
    std::unique_ptr<gba::GbaPpu> ppu = std::make_unique<gba::GbaPpu>();
    Fixture() {
        bus->code[2] = 0xE1B0F00Eu; // 08 MOVS PC,LR -- authored return only
        bus->code[6] = 0xE3A00001u; // 18 MOV R0,#VBlank
        bus->code[7] = 0xE59F1004u; // 1C LDR R1,[PC,#4]
        bus->code[8] = 0xE1C100B0u; // 20 STRH R0,[R1] -- acknowledge IF
        bus->code[9] = 0xE25EF004u; // 24 SUBS PC,LR,#4
        bus->code[10] = 0x04000202u;
        bus->io().set_bus(bus.get()); bus->io().set_ppu(ppu.get());
        gbarecomp::set_active_bus(bus.get()); gbarecomp::set_active_ppu(ppu.get());
        runtime_init(bus.get()); gbarecomp::runtime_restore_timing_context({});
        g_cpu = {}; g_cpu.cpsr = 0x1fu;
        g_cpu.R[13] = 0x03007e00u;
        g_cpu.banked_sp[ARM_BANK_SUPERVISOR] = 0x03007d00u;
        g_cpu.banked_sp[ARM_BANK_IRQ] = 0x03007c00u;
        gba::bios_hle_set_mode(gba::BiosHleMode::On);
        real_hle = g_bios_hle_hook; g_bios_hle_hook = observe_hle;
        hook_calls = 0;
    }
    ~Fixture() {
        gba::bios_hle_set_mode(gba::BiosHleMode::Off);
        gbarecomp::set_active_bus(nullptr); gbarecomp::set_active_ppu(nullptr);
        runtime_shutdown();
    }
};
void routing() {
    for (bool thumb : {false, true}) {
        for (uint32_t number : {2u, 4u, 5u, 0x1au, 0x1cu, 0xffu}) {
            Fixture f;
            const auto before = gbarecomp::self_heal_interpreted_insns();
            for (unsigned repeat = 0; repeat < 32; ++repeat) {
                const uint32_t cpsr = 0x1fu | (thumb ? CPSR_T_BIT : 0u);
                g_cpu.cpsr = cpsr; g_cpu.R[15] = continuation;
                runtime_call_push_return(0x03002000u);
                // ARM's low 16 comment bits are NOT the BIOS service number.
                const uint32_t immediate = thumb ? number : (number << 16) | 0x1234u;
                runtime_swi(immediate);
                check(decoded_number == number, "HLE receives correctly decoded SWI number");
                check(g_cpu.R[15] == continuation && g_cpu.cpsr == cpsr,
                      "authored SVC return restores continuation/mode/T/I");
                check(runtime_call_stack_depth() == 1, "fallback preserves caller frame");
                runtime_call_cancel_return(0x03002000u);
                std::vector<gbarecomp::RuntimeSwiContinuation> ledger;
                gbarecomp::runtime_capture_swi_context(ledger);
                check(ledger.empty(), "SVC return consumes its continuation once");
            }
            const auto interpreted = gbarecomp::self_heal_interpreted_insns() - before;
            check(hook_calls == 32 && f.bus->vector_fetches == 32 && interpreted == 32,
                  "distinct unsupported SWIs all enter vector 8, once per call");
            std::printf("route mode=%s swi=%02x calls=%u vector8=%u interpreted=%llu\n",
                        thumb ? "Thumb" : "ARM", number, hook_calls, f.bus->vector_fetches,
                        static_cast<unsigned long long>(interpreted));
        }
        Fixture f;
        const auto before = gbarecomp::self_heal_interpreted_insns();
        g_cpu.cpsr = 0x1fu | (thumb ? CPSR_T_BIT : 0u);
        g_cpu.R[15] = continuation; g_cpu.R[0] = 10; g_cpu.R[1] = 3;
        runtime_swi(thumb ? 6u : 0x060000u);
        check(decoded_number == 6 && g_cpu.R[0] == 3 && g_cpu.R[1] == 1 &&
              !f.bus->vector_fetches && gbarecomp::self_heal_interpreted_insns() == before,
              "implemented Div is serviced by actual HLE without vector fallback");
    }
}
void pending_vblank() {
    for (bool thumb : {false, true}) for (bool masked : {false, true})
        for (bool ime : {false, true}) for (bool enabled : {false, true}) {
            Fixture f;
            f.bus->write16(0x04000004u, 8); // DISPSTAT VBlank IRQ enable
            f.bus->write16(0x04000200u, enabled ? gba::GbaIo::IrqVBlank : 0);
            f.bus->write16(0x04000208u, ime ? 1 : 0);
            g_cpu.cpsr |= CPSR_I_BIT;
            runtime_tick(gba::GbaPpu::kCyclesPerScanline * gba::GbaPpu::kLinesVisible);
            check((f.bus->io().if_reg() & gba::GbaIo::IrqVBlank) != 0,
                  "real PPU progression produces pending VBlank");
            const auto before = g_runtime_irq_entries;
            const auto insns = gbarecomp::self_heal_interpreted_insns();
            for (unsigned repeat = 0; repeat < 8; ++repeat) {
                g_cpu.cpsr = 0x1fu | (thumb ? CPSR_T_BIT : 0u) | (masked ? CPSR_I_BIT : 0u);
                g_cpu.R[15] = continuation;
                runtime_swi(thumb ? 5u : 0x050000u);
                check(g_cpu.R[15] == continuation, "pending/masked IRQ returns to SWI continuation");
            }
            const bool delivered = !masked && ime && enabled;
            check(g_runtime_irq_entries - before == unsigned(delivered),
                  "IRQ delivery obeys IE, IME and restored CPSR.I");
            check((f.bus->io().if_reg() & gba::GbaIo::IrqVBlank) ==
                  (delivered ? 0u : uint32_t(gba::GbaIo::IrqVBlank)), "only delivered IRQ acknowledges IF");
            check(gbarecomp::self_heal_interpreted_insns() - insns == (delivered ? 12u : 8u),
                  "instruction attribution includes four IRQ wrapper instructions when delivered");
        }
    std::puts("VBlank: 16 combinations of ARM/Thumb, CPSR.I, IME and IE, 8 calls each");
}
// Compare the real bridge to the existing forced-interpreter prologue at a
// HALT boundary. Both execute the same authored STRB and same tick. Expected
// behavior is no following instruction until a wake; --check-halt reports the
// pre-existing mismatch as exit 1. This is NOT registered as a passing CTest.
int halt_boundary() {
    auto run = [](bool bridge) {
        Fixture f;
        f.bus->code[2] = 0xE3A00000u; // 08 MOV R0,#0
        f.bus->code[3] = 0xE59F1008u; // 0C LDR R1,[PC,#8] -> 1C
        f.bus->code[4] = 0xE5C10000u; // 10 STRB R0,[R1] -- HALTCNT
        f.bus->code[5] = 0xE2822001u; // 14 ADD R2,R2,#1 -- must not run halted
        f.bus->code[6] = 0xE1B0F00Eu; // 18 MOVS PC,LR
        f.bus->code[7] = 0x04000301u;
        if (bridge) {
            g_cpu.R[15] = continuation;
            runtime_swi(0x020000u);
        } else {
            // Same SVC entry state and entry tick, without dispatching vector 8
            // into the bridge. Step through the existing reference frontend.
            g_cpu.banked_spsr[ARM_BANK_SUPERVISOR] = 0x1fu;
            g_cpu.banked_sp[ARM_BANK_USER] = g_cpu.R[13];
            g_cpu.R[13] = g_cpu.banked_sp[ARM_BANK_SUPERVISOR];
            g_cpu.cpsr = 0x93u; g_cpu.R[14] = continuation; g_cpu.R[15] = 8;
            runtime_tick(3);
            for (unsigned i = 0; i < 5; ++i) runtime_force_interp_step();
        }
        const std::array<uint64_t, 4> result{g_cpu.R[15], g_cpu.R[2],
                                            f.bus->io().halted(), g_runtime_cycles};
        std::printf("HALT path=%s pc=%08x r2=%u halted=%u cycles=%llu\n",
                    bridge ? "bridge" : "forced", unsigned(result[0]), unsigned(result[1]),
                    unsigned(result[2]), static_cast<unsigned long long>(result[3]));
        return result;
    };
    const auto forced = run(false), bridge = run(true);
    check(forced[0] == 0x14 && forced[1] == 0 && forced[2] == 1,
          "reference frontend stops at HALT before ADD");
    if (forced != bridge) {
        std::puts("KNOWN DIVERGENCE: bridge executes the ADD and SVC return while HALT remains set");
        return 1;
    }
    return failures ? 2 : 0;
}
} // namespace
int main(int argc, char** argv) {
#if defined(_WIN32)
    _putenv_s("GBARECOMP_SELFHEAL_RECOMPILE", "0");
    _putenv_s("GBARECOMP_YIELD_ON_VBLANK", "0");
#else
    setenv("GBARECOMP_SELFHEAL_RECOMPILE", "0", 1);
    setenv("GBARECOMP_YIELD_ON_VBLANK", "0", 1);
#endif
    if (argc == 2 && std::strcmp(argv[1], "--check-halt") == 0) return halt_boundary();
    if (argc != 1) return 2;
    routing(); pending_vblank();
    std::printf("bios_swi_path_probe: %u failures\n", failures);
    return failures ? 1 : 0;
}
