#include "ram_templates.h"
#include "emit_function.h"
#include <cstdio>
#include <vector>

int main() {
    constexpr uint32_t base = 0x08000000u;
    std::vector<uint8_t> rom(192, 0xFF);
    // Two equal instruction bodies separated by data must produce one body.
    rom[0]=rom[32]=0x01; rom[1]=rom[33]=0x20;
    rom[2]=rom[34]=0x70; rom[3]=rom[35]=0x47;
    gbarecomp::Function fn{};
    fn.addr=base; fn.end_addr=base+4; fn.mode=gbarecomp::CpuMode::Thumb;
    fn.switch_blocks.push_back({base+32,base+36});
    auto unreferenced = gbarecomp::collect_ram_code_templates({fn},rom.data(),rom.size(),base,"");
    bool ok = unreferenced.empty();
    gbarecomp::Function loader{};
    loader.addr=base+0x80; loader.end_addr=loader.addr+4;
    loader.mode=gbarecomp::CpuMode::Thumb;
    rom[0x80]=5; rom[0x81]=0x48; rom[0x82]=0x70; rom[0x83]=0x47;
    for (unsigned b=0;b<4;++b) rom[0x98+b]=uint8_t(base>>(8u*b));
    auto templates = gbarecomp::collect_ram_code_templates({fn,loader},rom.data(),rom.size(),base,"");
    ok &= templates.size()==1 && templates[0].bytes.size()==4 &&
              templates[0].function.alias_entries == std::vector<uint32_t>{0x03000002u};
    // RAM-address metadata still points to the immutable ROM source.
    fn.addr=0x03001200u; fn.source_addr=base; fn.end_addr=fn.addr+4;
    fn.switch_blocks.clear();
    templates = gbarecomp::collect_ram_code_templates({fn},rom.data(),rom.size(),base,"test_");
    ok &= templates.size()==1 && templates[0].function.source_addr==base;
    if (templates.size()==1) {
        const auto body=gbarecomp::emit_function_body_str(templates[0].function,
            rom.data(),rom.size(),base,{},nullptr,nullptr,true);
        ok &= body.find("g_runtime_native_ram_base")!=std::string::npos &&
              body.find("runtime_native_ram_instruction_matches")!=std::string::npos &&
              body.find("GBARECOMP_TAIL_DISPATCH(_ram_base + 4u)")!=std::string::npos;
    }
    // Large discovered spans have bounded fragments and preserve every byte.
    fn.addr=0x03001200u; fn.source_addr=base; fn.end_addr=fn.addr+130;
    for (unsigned i=0;i<130;++i) rom[i]=uint8_t(i);
    templates=gbarecomp::collect_ram_code_templates({fn},rom.data(),rom.size(),base,"");
    ok &= templates.size()==3 && templates[0].bytes.size()==64 &&
          templates[1].bytes.size()==64 && templates[2].bytes.size()==2;
    if (!ok) std::fprintf(stderr,"RAM AOT fragment bounds/source/deduplication failed\n");
    return ok ? 0 : 1;
}
