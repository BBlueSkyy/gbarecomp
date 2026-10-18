#include "function_finder.h"
#include "emit_function.h"
#include <algorithm>

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr std::uint32_t kBase = 0x08000000u;

void put16(std::vector<std::uint8_t>& rom, std::size_t off,
           std::uint16_t value) {
    rom[off] = static_cast<std::uint8_t>(value);
    rom[off + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put32(std::vector<std::uint8_t>& rom, std::size_t off,
           std::uint32_t value) {
    put16(rom, off, static_cast<std::uint16_t>(value));
    put16(rom, off + 2, static_cast<std::uint16_t>(value >> 16));
}

bool has_function(const gbarecomp::FunctionFinder& finder,
                  std::uint32_t addr, gbarecomp::CpuMode mode) {
    for (const auto& fn : finder.functions()) {
        if (fn.addr == addr && fn.mode == mode) return true;
    }
    return false;
}

const gbarecomp::Function* find_function(
    const gbarecomp::FunctionFinder& finder,
    std::uint32_t addr,
    gbarecomp::CpuMode mode) {
    for (const auto& fn : finder.functions()) {
        if (fn.addr == addr && fn.mode == mode) return &fn;
    }
    return nullptr;
}

}  // namespace

// Actual GCC Thumb switch encodings, including the PC+4 value at an ADD pc
// whose address is 2 mod 4. Cases remain labels in their dispatcher host.
static bool thumb_switch(bool halfword, bool literal, bool manual,
                         unsigned fault = 0, bool bhi = false,
                         bool late_guard = false) {
    std::vector<std::uint8_t> rom(0x400, 0);
    put16(rom, 0, 0x2803);      // cmp r0,#3
    put16(rom, 2, 0xD21D);     // bcs default at +0x40
    if (bhi) {
        put16(rom, 0, 0x2802); // cmp r0,#2 (inclusive bound)
        put16(rom, 2, 0xD81D); // bhi default
    }
    put16(rom, 4, halfword ? 0x0041 : 0x46C0); // lsls r1,r0,#1 / nop
    put16(rom, 6, literal ? 0x4B16 : 0xA316);  // table at +0x60
    put16(rom, 8, 0x46C0);     // nop (ADD pc is 2 mod 4)
    put16(rom, 10, halfword ? 0x5A5B : 0x5C1B); // ldrh r3,[r3,r1] / ldrb
    put16(rom, 12, fault == 2 ? 0x009B : 0x005B); // lsl #2 / #1
    put16(rom, 14, fault == 3 ? 0x469F : 0x449F); // mov / add pc,r3
    if (literal) put32(rom, 0x60, kBase + 0x80);
    const unsigned table = literal ? 0x80 : 0x60;
    // Values exceed 255 in the halfword variant, proving full-width loads.
    const unsigned first = halfword ? 0x220 : 0xA0;
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned target = first + 4 * i;
        const unsigned value = (target - (fault == 10 ? 20 : 18)) / 2;
        if (halfword) put16(rom, table + 2 * i, value);
        else rom[table + i] = static_cast<std::uint8_t>(value);
        put16(rom, target, 0x2201 + i);  // movs r2,#case
        put16(rom, target + 2, 0x4770);  // bx lr
    }
    put16(rom, 0x40, 0x4770);
    if (fault == 1 || manual) put16(rom, 2, 0x46C0); // no guard
    if (fault == 4) { // invalid target in the table: reject atomically
        if (halfword) put16(rom, table, 0xFFFF);
        else rom[table] = static_cast<std::uint8_t>((table - 18) / 2);
    }
    if (fault == 5) put16(rom, 0, 0x2903); // guard compares wrong index
    if (fault == 6) put16(rom, 8, halfword ? 0x2100 : 0x2000); // clobber index
    if (fault == 7) { // a call between guard and table base invalidates the bound
        put16(rom, 4, 0xF000); // bl default
        put16(rom, 6, 0xF81C);
        put16(rom, 8, literal ? 0x4B15 : 0xA315); // reload table base after call
    }
    if (late_guard || fault == 8 || fault == 9) {
        put16(rom, 0, 0x0041); // lsls r1,r0,#1 before the guard
        put16(rom, 2, fault == 8 ? 0x2000 : fault == 9 ? 0xBC01 : 0x46C0);
        // A bound on a replacement r0 must not constrain the old r1.
        put16(rom, 4, 0x2803); // cmp r0,#3
        put16(rom, 6, 0xD21B); // bcs default
        put16(rom, 8, literal ? 0x4B15 : 0xA315);
    }
    if (fault == 10) {
        put16(rom, 0, 0x0041); // lsls r1,r0,#1
        put16(rom, 2, 0xF000); // a call can replace the unbounded source
        put16(rom, 4, 0xF81D);
        put16(rom, 6, 0x2803); // cmp r0,#3 after the call
        put16(rom, 8, 0xD21A); // bcs default
        put16(rom, 10, literal ? 0x4B15 : 0xA315);
        put16(rom, 12, 0x5A5B);
        put16(rom, 14, 0x005B);
        put16(rom, 16, 0x449F);
    }
    gbarecomp::FunctionFinder finder(rom.data(), rom.size(), kBase);
    finder.set_speculative_literal_harvest(false);
    finder.set_static_resume_all(true);
    if (manual) finder.add_thumb_relative_table(kBase + table,
        halfword ? 2 : 1, 3, kBase + 18);
    finder.add_seed({kBase, gbarecomp::CpuMode::Thumb, "switch_host"});
    finder.run(64);
    const auto* host = find_function(finder, kBase, gbarecomp::CpuMode::Thumb);
    if (!host || !finder.collisions().empty()) return false;
    if (fault) return host->switch_blocks.empty() && finder.auto_jump_tables().empty();
    if (finder.auto_jump_tables().size() != (manual ? 0u : 1u) ||
        host->switch_blocks.size() != 3) return false;
    for (unsigned i = 0; i < 3; ++i) {
        const auto target = kBase + first + 4 * i;
        if (has_function(finder, target, gbarecomp::CpuMode::Thumb) ||
            std::find(host->alias_entries.begin(), host->alias_entries.end(), target) ==
                host->alias_entries.end()) return false;
    }
    const auto body = gbarecomp::emit_function_body_str(
        *host, rom.data(), rom.size(), kBase, {});
    if (body.find("/* 08000060 ") != std::string::npos ||
        body.find("/* 08000080 ") != std::string::npos) return false;
    return true;
}

int main() {
    for (bool halfword : {false, true}) {
        for (bool literal : {false, true}) {
            for (unsigned fault = 0; fault <= 7; ++fault) {
                if (!thumb_switch(halfword, literal, false, fault)) {
                    std::fprintf(stderr, "Thumb switch failed: half=%d literal=%d fault=%u\n",
                                 halfword, literal, fault);
                    return 1;
                }
            }
            if (!thumb_switch(halfword, literal, true)) return 1;
            if (!thumb_switch(halfword, literal, false, 0, true)) return 1;
            if (halfword) {
                for (unsigned fault : {0u, 8u, 9u, 10u}) {
                    if (!thumb_switch(true, literal, false, fault, false, true)) {
                        std::fprintf(stderr, "Thumb halfword late guard failed: literal=%d fault=%u\n",
                                     literal, fault);
                        return 1;
                    }
                }
            }
        }
    }
    std::vector<std::uint8_t> rom(0x80, 0);

    // ldr r1,[pc,#8]  -> tracker sees the even ROM data pointer below
    // pop {r1}        -> r1 is overwritten from the runtime stack
    // bx r1           -> dynamic return; must not reuse the stale literal
    put16(rom, 0x00, 0x4902u);
    put16(rom, 0x02, 0xBC02u);
    put16(rom, 0x04, 0x4708u);
    put32(rom, 0x0C, kBase + 0x20u);

    // Make the false target valid-looking ARM code. Before the regression
    // fix, the stale constant caused this address to be emitted.
    put32(rom, 0x20, 0xE12FFF1Eu);  // bx lr

    gbarecomp::FunctionFinder finder(rom.data(), rom.size(), kBase);
    finder.set_speculative_literal_harvest(false);
    finder.add_seed({kBase, gbarecomp::CpuMode::Thumb, "pop_bx_return"});
    finder.run(32);

    if (!has_function(finder, kBase, gbarecomp::CpuMode::Thumb)) {
        std::fprintf(stderr, "entry function was not discovered\n");
        return 1;
    }
    if (has_function(finder, kBase + 0x20u, gbarecomp::CpuMode::Arm)) {
        std::fprintf(stderr,
                     "POP destination retained a stale constant and seeded "
                     "0x%08X as ARM code\n",
                     kBase + 0x20u);
        return 1;
    }

    // A stack-local helper may be copied again at an overlapping address.
    // The explicit source address must win over the first fixed mapping, and
    // CFG roots from the older placement must not truncate the newer body.
    std::vector<std::uint8_t> relocated_rom(0x80, 0);
    constexpr std::size_t kTemplate = 0x40;
    put16(relocated_rom, kTemplate + 0x00, 0x2800u); // cmp r0,#0
    put16(relocated_rom, kTemplate + 0x02, 0xD001u); // beq +8
    put16(relocated_rom, kTemplate + 0x04, 0x4770u); // bx lr
    put16(relocated_rom, kTemplate + 0x06, 0x46C0u); // nop
    put16(relocated_rom, kTemplate + 0x08, 0x4770u); // bx lr

    constexpr std::uint32_t kRam = 0x03000000u;
    gbarecomp::FunctionFinder relocated(
        relocated_rom.data(), relocated_rom.size(), kBase);
    relocated.set_speculative_literal_harvest(false);
    relocated.add_code_copy(kRam, kBase + kTemplate, 0x20, "first copy");
    relocated.add_seed({kRam, gbarecomp::CpuMode::Thumb, "first"});
    gbarecomp::FunctionSeed overlapping{
        kRam + 4, gbarecomp::CpuMode::Thumb, "overlapping"};
    overlapping.source_addr = kBase + kTemplate;
    relocated.add_seed(overlapping);
    relocated.run(32);

    const auto* overlap_fn = find_function(
        relocated, kRam + 4, gbarecomp::CpuMode::Thumb);
    if (!overlap_fn || overlap_fn->source_addr != kBase + kTemplate) {
        std::fprintf(stderr,
                     "overlapping relocation did not retain explicit ROM backing\n");
        return 1;
    }
    if (overlap_fn->end_addr <= kRam + 8) {
        std::fprintf(stderr,
                     "older placement incorrectly truncated overlapping CFG\n");
        return 1;
    }
    const auto* overlap_target = find_function(
        relocated, kRam + 12, gbarecomp::CpuMode::Thumb);
    if (!overlap_target ||
        overlap_target->source_addr != kBase + kTemplate + 8) {
        std::fprintf(stderr,
                     "relocation bias did not propagate to direct branch target\n");
        return 1;
    }

    // A bounded AOT sweep finds compiler-shaped prologues without runtime
    // evidence, then uses two strong entries in an even-address THUMB pointer
    // table to infer the table mode and recover a leaf function with no PUSH.
    std::vector<std::uint8_t> aot_rom(0x100, 0);
    put16(aot_rom, 0x00, 0x4802u);  // ldr r0,[pc,#8] -> table base
    put16(aot_rom, 0x02, 0x4770u);  // bx lr
    put32(aot_rom, 0x0C, kBase + 0xC0u);
    put16(aot_rom, 0x40, 0xB500u);  // push {lr}
    put16(aot_rom, 0x42, 0x46C0u);
    put16(aot_rom, 0x44, 0x46C0u);
    put16(aot_rom, 0x46, 0x46C0u);
    put16(aot_rom, 0x48, 0x46C0u);
    put16(aot_rom, 0x4A, 0xBD00u);  // pop {pc}
    put16(aot_rom, 0x50, 0xB500u);  // second strong table-mode vote
    put16(aot_rom, 0x52, 0x46C0u);
    put16(aot_rom, 0x54, 0x46C0u);
    put16(aot_rom, 0x56, 0x46C0u);
    put16(aot_rom, 0x58, 0x46C0u);
    put16(aot_rom, 0x5A, 0xBD00u);
    put16(aot_rom, 0x60, 0x2001u);  // movs r0,#1 (leaf, no prologue)
    put16(aot_rom, 0x62, 0x46C0u);
    put16(aot_rom, 0x64, 0x46C0u);
    put16(aot_rom, 0x66, 0x46C0u);
    put16(aot_rom, 0x68, 0x46C0u);
    put16(aot_rom, 0x6A, 0x4770u);  // bx lr
    put32(aot_rom, 0xC0, kBase + 0x40u);
    put32(aot_rom, 0xC4, kBase + 0x50u);
    put32(aot_rom, 0xC8, kBase + 0x60u);

    gbarecomp::FunctionFinder aot(aot_rom.data(), aot_rom.size(), kBase);
    aot.set_speculative_literal_harvest(false);
    aot.set_aot_scan_range(kBase + 0x40u, kBase + 0x80u);
    aot.add_seed({kBase, gbarecomp::CpuMode::Thumb, "entry"});
    aot.run(64);
    if (!has_function(aot, kBase + 0x40u, gbarecomp::CpuMode::Thumb) ||
        !has_function(aot, kBase + 0x50u, gbarecomp::CpuMode::Thumb) ||
        !has_function(aot, kBase + 0x60u, gbarecomp::CpuMode::Thumb)) {
        std::fprintf(stderr,
                     "bounded AOT scan missed a prologue or address-taken "
                     "THUMB leaf\n");
        return 1;
    }
    if (aot.stats().aot_prologue_seeds != 2 ||
        aot.stats().aot_pointer_tables != 1 ||
        aot.stats().aot_pointer_seeds != 1) {
        std::fprintf(stderr,
                     "bounded AOT scan stats disagree: prologues=%zu "
                     "tables=%zu leaves=%zu\n",
                     aot.stats().aot_prologue_seeds,
                     aot.stats().aot_pointer_tables,
                     aot.stats().aot_pointer_seeds);
        return 1;
    }

    // A copied ARM blob can contain internal callable helpers that are not
    // reachable from its declared entry. Scan the ROM backing of a known
    // code-copy range for stack-frame prologues, including leaf helpers that
    // save callee-saved registers without saving LR.
    std::vector<std::uint8_t> copied_rom(0x100, 0);
    constexpr std::size_t kCopiedTemplate = 0x40;
    put32(copied_rom, kCopiedTemplate + 0x00, 0xE12FFF1Eu); // bx lr
    put32(copied_rom, kCopiedTemplate + 0x1C, 0xE1A0C00Du); // mov ip,sp
    put32(copied_rom, kCopiedTemplate + 0x20, 0xE92D0030u); // push {r4,r5}
    put32(copied_rom, kCopiedTemplate + 0x24, 0xE1A00000u); // nop
    put32(copied_rom, kCopiedTemplate + 0x28, 0xE1A00000u);
    put32(copied_rom, kCopiedTemplate + 0x2C, 0xE1A00000u);
    put32(copied_rom, kCopiedTemplate + 0x30, 0xE1A00000u);
    put32(copied_rom, kCopiedTemplate + 0x34, 0xE8BD0030u); // pop {r4,r5}
    put32(copied_rom, kCopiedTemplate + 0x38, 0xE12FFF1Eu); // bx lr

    constexpr std::uint32_t kCopiedRam = 0x03001000u;
    gbarecomp::FunctionFinder copied(
        copied_rom.data(), copied_rom.size(), kBase);
    copied.set_speculative_literal_harvest(false);
    copied.set_static_resume_all(true);
    copied.add_code_copy(
        kCopiedRam, kBase + kCopiedTemplate, 0x40, "ARM helper blob");
    copied.add_seed(
        {kCopiedRam, gbarecomp::CpuMode::Arm, "copied_entry"});
    copied.run(32);
    const auto* copied_helper = find_function(
        copied, kCopiedRam + 0x1Cu, gbarecomp::CpuMode::Arm);
    bool copied_push_resume = false;
    if (copied_helper) {
        for (std::uint32_t pc : copied_helper->alias_entries) {
            if (pc == kCopiedRam + 0x20u) copied_push_resume = true;
        }
    }
    if (!copied_helper ||
        copied_helper->source_addr != kBase + kCopiedTemplate + 0x1Cu ||
        copied.stats().aot_code_copy_seeds != 1 ||
        !copied_push_resume) {
        std::fprintf(stderr,
                     "code-copy scan missed internal ARM helper: fn=%s "
                     "source=0x%08X seeds=%zu push_resume=%s\n",
                     copied_helper ? "yes" : "no",
                     copied_helper ? copied_helper->source_addr : 0u,
                     copied.stats().aot_code_copy_seeds,
                     copied_push_resume ? "yes" : "no");
        return 1;
    }

    std::printf("function_finder_tests: PASS\n");
    return 0;
}
