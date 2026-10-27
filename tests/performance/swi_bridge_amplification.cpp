// ROM/BIOS-free reproduction of native caller work absorbed by a SWI miss.
#include "gba_bus.h"
#include "gba_ppu.h"
#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include "self_heal.h"
#include "snapshot.h"
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <memory>
struct DispatchEntry { uint32_t addr; uint8_t thumb, resume; void (*fn)(); };
static unsigned native_calls=0;
static void native_continuation() {
 ++native_calls;
 for (;;) {
  g_cpu.R[15]=0x03001000; if(runtime_should_yield()) std::abort();
  ++g_cpu.R[0]; g_cpu.R[15]=0x03001004; runtime_tick(1);
  if(runtime_should_yield()) std::abort();
  const auto before=g_cpu.R[1]; --g_cpu.R[1]; arm_set_nzcv_sub(before,1,g_cpu.R[1]);
  g_cpu.R[15]=0x03001008; runtime_tick(1);
  if(runtime_should_yield()) std::abort();
  if(!cpsr_z()) {g_cpu.R[15]=0x03001000;runtime_tick(3);}
  else {g_cpu.R[15]=0x0300100c;runtime_tick(1);break;}
 }
 if(runtime_should_yield()) std::abort();
 g_cpu.R[15]=g_cpu.R[14]&~1u; runtime_tick(3); runtime_call_should_return(g_cpu.R[15]);
}
extern "C" const DispatchEntry kDispatchTable[]={{0x03001000u,0,0,native_continuation}};
extern "C" const unsigned kDispatchTableLen=1;
class SyntheticBiosBus:public gba::GbaBus {
 public: bool pending_irq=false;
 public: uint32_t read32(uint32_t addr) override {
    if(pending_irq) {
      if(addr==0x18) return 0xE3A00008u;
      if(addr==0x1c) return 0xE59F1004u;
      if(addr==0x20) return 0xE1C100B0u;
      if(addr==0x24) return 0xE25EF004u;
      if(addr==0x28) return 0x04000202u;
    }
    if(addr==8) return 0xE1B0F00Eu; // authored MOVS PC,LR: return from synthetic SWI
    return gba::GbaBus::read32(addr);
 }
};
int main(int argc, char** argv) {
 const bool pending_irq=argc==2 && std::strcmp(argv[1],"--pending-irq")==0;
#if defined(_WIN32)
 _putenv_s("GBARECOMP_SELFHEAL_RECOMPILE", "0");
 _putenv_s("GBARECOMP_YIELD_ON_VBLANK", "0");
#else
 setenv("GBARECOMP_SELFHEAL_RECOMPILE","0",1);
 setenv("GBARECOMP_YIELD_ON_VBLANK","0",1);
#endif
 auto bus=std::make_unique<SyntheticBiosBus>();auto ppu=std::make_unique<gba::GbaPpu>();
 bus->write32(0x03001000,0xE2800001); // add r0,r0,#1 -- native caller work
 bus->write32(0x03001004,0xE2511001);
 bus->write32(0x03001008,0x1AFFFFFC);
 bus->write32(0x0300100c,0xE12FFF1E); // bx lr -- caller returns to its caller
 bus->rtc().set_diagnostic_host_seconds(0);
 gbarecomp::set_active_bus(bus.get());gbarecomp::set_active_ppu(ppu.get());runtime_init(bus.get());
 g_cpu={}; g_cpu.cpsr=0x1f;g_cpu.R[0]=0;g_cpu.R[1]=10000;g_cpu.R[14]=0x03002000;g_cpu.R[15]=0x03001000;
 g_cpu.R[13]=0x03007e00;g_cpu.banked_sp[ARM_BANK_SUPERVISOR]=0x03007d00;
 bus->pending_irq=pending_irq;
 if(pending_irq) {
  bus->io().set_bus(bus.get());bus->io().set_ppu(ppu.get());
  bus->write16(0x04000200,gba::GbaIo::IrqTimer0);bus->write16(0x04000208,1);
  bus->io().request_irq(gba::GbaIo::IrqTimer0);
 }
 runtime_call_push_return(0x03002000);
 const auto before=gbarecomp::self_heal_interpreted_insns();
 const auto start=std::chrono::steady_clock::now();
 runtime_swi(0x060000);
 if(g_cpu.R[15]!=0x03002000) {
  runtime_call_cancel_return(0x03002000);
  runtime_dispatch(g_cpu.R[15]);
 }
 const auto ns=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count();
 std::printf("pc=%08x r0=%u call_depth=%u native_calls=%u interpreted=%llu\n",g_cpu.R[15],g_cpu.R[0],runtime_call_stack_depth(),native_calls,(unsigned long long)(gbarecomp::self_heal_interpreted_insns()-before));
 std::printf("elapsed_ns=%.0f cycles=%llu final=%08x r0=%u r1=%u depth=%u cpsr=%08x\n",ns,g_runtime_cycles,g_cpu.R[15],g_cpu.R[0],g_cpu.R[1],runtime_call_stack_depth(),g_cpu.cpsr);
 gbarecomp::debug::SnapshotWriter state;
 bus->serialize(state);bus->io().serialize(state);bus->audio().serialize(state);ppu->serialize(state);
 state.bytes(&g_cpu,sizeof(g_cpu));
 gbarecomp::RuntimeTimingContext timing;gbarecomp::runtime_capture_timing_context(timing);
 state.u64(timing.cycles);state.u64(timing.pending_cycles);state.u64(static_cast<uint64_t>(timing.event_budget));
 uint64_t hash=14695981039346656037ull;
 for(auto byte:state.buffer()){hash^=byte;hash*=1099511628211ull;}
 std::printf("state_fnv1a=%016llx bytes=%zu\n",(unsigned long long)hash,state.size());
 const bool correct=g_cpu.R[15]==0x03002000 && g_cpu.R[0]==10000 && g_cpu.R[1]==0 && runtime_call_stack_depth()==0 && (pending_irq ? g_runtime_cycles>50007 : g_runtime_cycles==50007);
 gbarecomp::set_active_bus(nullptr);gbarecomp::set_active_ppu(nullptr);runtime_shutdown();
 return correct?0:1;
}
