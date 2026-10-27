// Authored state only. Build using engine3886062 headers/libraries, not v5.
#include "gba_bus.h"
#include "gba_ppu.h"
#include "runtime_arm.h"
#include "runtime_context.h"
#include "snapshot.h"
#include <fstream>
#include <memory>
struct DispatchEntry { uint32_t addr; uint8_t thumb, resume; void (*fn)(); };
extern "C" const DispatchEntry kDispatchTable[]={{0,0,0,nullptr}};
extern "C" const unsigned kDispatchTableLen=0;
int main(int argc, char** argv){
 if(argc!=2)return 2;
 static_assert(gbarecomp::debug::kSnapshotVersion==4, "Compile against the base v4 engine");
 auto b=std::make_unique<gba::GbaBus>(); auto p=std::make_unique<gba::GbaPpu>();
 b->set_rom(nullptr,0); runtime_init(b.get()); g_cpu={}; g_cpu.cpsr=0x93; g_cpu.R[0]=0x12345678;
 g_cpu.R[13]=0x03007d00;g_cpu.R[14]=0x03001000;g_cpu.R[15]=0x100;
 g_cpu.banked_spsr[ARM_BANK_SUPERVISOR]=0x3f;
 g_cpu.banked_sp[ARM_BANK_USER]=0x03007e00;g_cpu.banked_lr[ARM_BANK_USER]=0x03002001;
 runtime_call_push_return(0x03002000);runtime_call_push_return(0x03003000);
 gbarecomp::runtime_restore_swi_context({{0x03001000,0x03000ffe}});
 b->rtc().set_diagnostic_host_seconds(0);b->write32(0x03000010,0xaabbccdd);
 uint64_t old_clock=98765;gbarecomp::debug::SnapshotContext c;
 c.bus=b.get();c.ppu=p.get();c.rom_sha1="0000000000000000000000000000000000000000";c.cycles_elapsed=&old_clock;
 std::vector<uint8_t> bytes;std::string e;
 if(!gbarecomp::debug::save_state_bytes(&bytes,c,&e))return 1;
 std::ofstream out(argv[1],std::ios::binary);
 out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());return !out;
}
