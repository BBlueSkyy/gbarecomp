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
    // Code used only after copying to RAM is absent from the ROM call graph.
    // A reachable PUSH-LR source, tagged or untagged, can enter the guarded
    // catalog without enabling speculative ROM dispatch discovery.
    std::vector<uint8_t> copy_image(0x200,0);
    copy_image[0]=0; copy_image[1]=0x48; // ldr r0,[pc,#0]
    copy_image[2]=0x70; copy_image[3]=0x47;
    for (unsigned b=0;b<4;++b) copy_image[4+b]=uint8_t((base+0x101u)>>(8u*b));
    copy_image[0x100]=0; copy_image[0x101]=0xB5;
    for (unsigned i=2;i<10;i+=2) copy_image[0x101+i]=0x20;
    copy_image[0x10A]=0; copy_image[0x10B]=0xBD;
    loader.addr=base; loader.end_addr=base+4;
    templates=gbarecomp::collect_ram_code_templates({loader},copy_image.data(),copy_image.size(),base,"");
    ok &= templates.size()==1 && templates[0].function.source_addr==base+0x100u;
    copy_image[4]=0; // byte-copy source pointers need not carry a Thumb tag
    templates=gbarecomp::collect_ram_code_templates({loader},copy_image.data(),copy_image.size(),base,"");
    ok &= templates.size()==1 && templates[0].function.source_addr==base+0x100u;
    const std::vector<gbarecomp::DataRange> excluded={{base+0x100u,base+0x10Cu,"data"}};
    templates=gbarecomp::collect_ram_code_templates({loader},copy_image.data(),copy_image.size(),base,"",&excluded);
    ok &= templates.empty();
    copy_image[0x101]=0x20; // no distinctive code entry
    templates=gbarecomp::collect_ram_code_templates({loader},copy_image.data(),copy_image.size(),base,"");
    ok &= templates.empty();
    // Matrix ROMs can contain differently linked siblings of a known copied
    // startup module. Their PC-relative immediates differ after the shared
    // mode/MSR prefix; each body needs its own immutable source mapping.
    std::vector<uint8_t> banked(0x02001000u,0xFF);
    banked[0xAC]='M';
    auto put_word=[&](unsigned at,uint32_t word) {
        for (unsigned b=0;b<4;++b) banked[at+b]=uint8_t(word>>(8u*b));
    };
    for (unsigned at : {0x100u,0x200u}) {
        put_word(at,0xE3A00012u); put_word(at+4,0xE129F000u);
        put_word(at+8,at==0x100u ? 0xE3A02001u : 0xE3A02007u);
        put_word(at+12,0xE12FFF1Eu);
    }
    fn={}; fn.addr=0x02000000u; fn.end_addr=fn.addr+16u;
    fn.source_addr=base+0x100u; fn.mode=gbarecomp::CpuMode::Arm;
    const std::vector<gbarecomp::CodeCopyRange> copies={{fn.addr,fn.source_addr,16u,"module"}};
    templates=gbarecomp::collect_ram_code_templates({fn},banked.data(),banked.size(),base,"",nullptr,&copies);
    auto has_sibling=[&]() {
        for (const auto& item : templates) {
            const auto source=item.function.source_addr;
            if (source<=base+0x208u && uint64_t(source)+item.bytes.size()>=base+0x20Cu) {
                const auto n=base+0x208u-source;
                if (item.bytes[n]==7u && item.bytes[n+1]==0x20u &&
                    item.bytes[n+2]==0xA0u && item.bytes[n+3]==0xE3u &&
                    !item.cart_function.name.empty()) return true;
            }
        }
        return false;
    };
    ok &= has_sibling();
    put_word(0x204u,0xE1A00000u); // unrelated prefix cannot qualify a sibling
    templates=gbarecomp::collect_ram_code_templates({fn},banked.data(),banked.size(),base,"",nullptr,&copies);
    ok &= !has_sibling();
    if (!ok) std::fprintf(stderr,"RAM AOT fragment bounds/source/deduplication failed\n");
    return ok ? 0 : 1;
}
