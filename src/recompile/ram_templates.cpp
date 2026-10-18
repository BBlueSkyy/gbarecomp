#include "ram_templates.h"

#include <algorithm>
#include <cstdio>
#include <set>
#include <unordered_set>
#include "arm_decode.h"
#include "thumb_decode.h"

namespace gbarecomp {

std::vector<RamCodeTemplate> collect_ram_code_templates(
    const std::vector<Function>& functions, const uint8_t* image,
    std::size_t image_size, uint32_t image_base, const std::string& prefix) {
    std::vector<RamCodeTemplate> result;
    std::set<std::pair<CpuMode, std::vector<uint8_t>>> seen;
    std::set<uint32_t> references;
    std::vector<bool> selected(functions.size(),false);
    auto source_of=[](const Function& fn) { return fn.source_addr ? fn.source_addr : fn.addr; };
    auto spans_of=[](const Function& fn) {
        auto spans=fn.switch_blocks;
        spans.push_back({fn.addr,fn.end_addr});
        return spans;
    };
    auto read_word=[&](int64_t address, uint32_t width, uint32_t* value) {
        if (address < image_base || uint64_t(address-image_base)+width > image_size)
            return false;
        *value=0;
        for (uint32_t b=0;b<width;++b)
            *value |= uint32_t(image[std::size_t(address-image_base)+b]) << (8u*b);
        return true;
    };
    auto decode=[&](const Function& fn,uint32_t pc,armv4t::Instr* instruction) {
        uint32_t raw=0;
        if (!read_word(int64_t(source_of(fn))+int64_t(pc)-fn.addr,
                       fn.mode==CpuMode::Thumb ? 2u : 4u,&raw)) return false;
        *instruction=fn.mode==CpuMode::Thumb
            ? armv4t::ThumbDecoder::decode(uint16_t(raw),pc)
            : armv4t::ArmDecoder::decode(raw,pc);
        return true;
    };
    auto add_reference=[&](uint32_t value) {
        const uint32_t address=value & ~1u;
        if (address >= image_base && uint64_t(address-image_base)<image_size)
            references.insert(address);
    };
    // Restrict the catalog to address-taken code and known ROM-backed copies.
    // Full decomp symbol imports can contain tens of thousands of functions;
    // compiling every body a second time would add substantial build cost.
    for (const auto& fn : functions) {
        const uint32_t step=fn.mode==CpuMode::Thumb ? 2u : 4u;
        for (const auto& span : spans_of(fn)) {
            for (uint32_t pc=span.start;pc<span.end;pc+=step) {
                armv4t::Instr ins{};
                if (!decode(fn,pc,&ins)) break;
                if (ins.op==armv4t::IrOp::LDR && ins.mem.rn==15 &&
                    !ins.mem.by_register && !ins.mem.writeback) {
                    uint32_t pool=pc+(ins.thumb ? 4u : 8u);
                    if (ins.thumb) pool &= ~3u;
                    if (ins.mem.pre_indexed)
                        pool=ins.mem.add ? pool+ins.mem.imm_offset : pool-ins.mem.imm_offset;
                    uint32_t value=0;
                    if (read_word(int64_t(source_of(fn))+int64_t(pool)-fn.addr,4,&value))
                        add_reference(value);
                }
                if ((ins.op==armv4t::IrOp::ADD || ins.op==armv4t::IrOp::SUB) &&
                    ins.rn==15 && ins.rd!=15 && ins.op2.kind==armv4t::Op2::Kind::Imm) {
                    uint32_t value=pc+(ins.thumb ? 4u : 8u);
                    if (ins.thumb) value &= ~3u;
                    value=ins.op==armv4t::IrOp::ADD
                        ? value+ins.op2.imm_value : value-ins.op2.imm_value;
                    add_reference(value);
                }
            }
        }
    }
    bool changed=true;
    while (changed) {
        changed=false;
        for (std::size_t i=0;i<functions.size();++i) {
            if (selected[i]) continue;
            const auto& fn=functions[i];
            const auto source=source_of(fn);
            bool eligible=source!=fn.addr;
            for (const auto& span : spans_of(fn)) {
                const uint32_t start=source+(span.start-fn.addr);
                const auto it=references.lower_bound(start);
                eligible |= it!=references.end() && uint64_t(*it-start)<span.end-span.start;
            }
            if (!eligible) continue;
            selected[i]=true;
            changed=true;
            const uint32_t step=fn.mode==CpuMode::Thumb ? 2u : 4u;
            for (const auto& span : spans_of(fn)) {
                armv4t::Instr last{};
                bool have_last=false;
                for (uint32_t pc=span.start;pc<span.end;pc+=step) {
                    armv4t::Instr ins{};
                    if (!decode(fn,pc,&ins)) break;
                    if (ins.op==armv4t::IrOp::B)
                        add_reference(source+(ins.branch_target-fn.addr));
                    last=ins; have_last=true;
                }
                if (have_last && !last.is_return && !last.is_indirect &&
                    !(last.op==armv4t::IrOp::B && last.cond==armv4t::Cond::AL))
                    add_reference(source+(span.end-fn.addr));
            }
        }
    }
    // Small native fragments also cover partial copies and function suffixes.
    // Boundaries are deterministic; all transfer/resume PCs remain live RAM PCs.
    constexpr uint32_t kFragmentBytes = 64u;
    constexpr uint32_t kCanonicalBase = 0x03000000u;
    for (std::size_t index=0;index<functions.size();++index) {
        if (!selected[index]) continue;
        const auto& original=functions[index];
        std::vector<FunctionBlock> spans = original.switch_blocks;
        spans.push_back({original.addr, original.end_addr});
        std::sort(spans.begin(), spans.end(), [](const auto& a, const auto& b) {
            return a.start < b.start;
        });
        const uint32_t source = original.source_addr
            ? original.source_addr : original.addr;
        const uint32_t step = original.mode == CpuMode::Thumb ? 2u : 4u;
        for (const auto& span : spans) {
            for (uint32_t pc = span.start; pc < span.end;) {
                const uint32_t length = std::min(kFragmentBytes, span.end - pc);
                const int64_t source_pc = int64_t(source) + int64_t(pc) - original.addr;
                if (length % step != 0 || source_pc < image_base ||
                    uint64_t(source_pc - image_base) + length > image_size) break;
                const auto offset = std::size_t(source_pc - image_base);
                std::vector<uint8_t> bytes(image + offset, image + offset + length);
                if (seen.emplace(original.mode, bytes).second) {
                    Function fn{};
                    fn.addr = kCanonicalBase;
                    fn.source_addr = uint32_t(source_pc);
                    fn.mode = original.mode;
                    fn.end_addr = fn.walk_end_addr = fn.addr + length;
                    char name[96];
                    std::snprintf(name, sizeof(name), "%sram_aot_%zu",
                                  prefix.c_str(), result.size());
                    fn.name = name;
                    for (uint32_t n = step; n < length; n += step)
                        fn.alias_entries.push_back(fn.addr + n);
                    result.push_back({std::move(fn), std::move(bytes)});
                }
                pc += length;
            }
        }
    }
    return result;
}

}  // namespace gbarecomp
