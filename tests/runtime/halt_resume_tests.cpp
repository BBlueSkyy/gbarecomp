// Authored HALT/IRQ/SWI fixtures. No proprietary BIOS/ROM and no service HLE.
#include "gba_bus.h"
#include "gba_ppu.h"
#include "gba_irq.h"
#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include "runtime_context.h"
#include "snapshot.h"
#include "self_heal.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#ifdef GBARECOMP_HALT_BASELINE
namespace gbarecomp {
uint32_t runtime_pump_idle(uint32_t budget) {
    auto* b=active_bus();auto* p=active_ppu();
    uint32_t c=std::min(p->cycles_until_next_event(),b->io().cycles_until_next_timer_event());
    c=std::min(c,b->audio().cycles_until_next_sample());if(!c||c==UINT32_MAX)c=1;
    c=std::min(c,budget);runtime_tick(c);return c;
}
}
#endif
extern "C" void runtime_mmio_catch_up();
extern "C" uint32_t g_irq_nest_depth, g_irq_iret_depth;
extern "C" unsigned long long g_runtime_irq_entries;
struct DispatchEntry { uint32_t addr; uint8_t thumb, resume; void (*fn)(); };
namespace {
unsigned failures;
void check(bool ok, const char* text) {
    if (!ok) { ++failures; std::fprintf(stderr,"FAIL: %s\n",text); }
}
class AuthoredBus : public gba::GbaBus {
public:
    std::array<uint32_t,1024> code{};
    unsigned halted_fetches = 0, instruction_fetches = 0;
    uint32_t read32(uint32_t address) override {
        if (address < code.size()*4) {
            if (g_cpu.R[15]==address) {
                ++instruction_fetches;
                if (io().halted()) ++halted_fetches;
            }
            return code[address/4];
        }
        return GbaBus::read32(address);
    }
};
constexpr uint32_t main_pc=0x03001000, main_return=0x03002000, flag=0x03007ff8;
struct Fixture {
    std::unique_ptr<AuthoredBus> bus=std::make_unique<AuthoredBus>();
    std::unique_ptr<gba::GbaPpu> ppu=std::make_unique<gba::GbaPpu>();
    gbarecomp::RuntimeArmContext owner,replacement;
    Fixture() {
        bus->set_rom(nullptr,0); bus->rtc().set_diagnostic_host_seconds(0);
        bus->io().set_bus(bus.get()); bus->io().set_ppu(ppu.get());
        gbarecomp::set_active_bus(bus.get()); gbarecomp::set_active_ppu(ppu.get());
        runtime_init(bus.get()); gbarecomp::runtime_restore_timing_context({});
        gbarecomp::runtime_set_owned_context(&owner);
        g_runtime_irq_entries=0;
        g_cpu={}; g_cpu.cpsr=0x6000001f; g_cpu.R[15]=main_pc;
        g_cpu.R[13]=0x03007e00; g_cpu.R[14]=main_return;
        g_cpu.banked_sp[ARM_BANK_SUPERVISOR]=0x03007d00;
        g_cpu.banked_sp[ARM_BANK_IRQ]=0x03007c00;
        // A real interpreted IRQ wrapper, acknowledges the actual IF bits and
        // ORs them into the guest-owned BIOS flag. Not a host wake shortcut.
        bus->code[0x18/4]=0xea000078; // B 200
        bus->code[0x200/4]=0xe59f0018; // LDR r0,[pc,#18] -> 220 IF
        bus->code[0x204/4]=0xe1d010b0; // LDRH r1,[r0]
        bus->code[0x208/4]=0xe1c010b0; // STRH r1,[r0]
        bus->code[0x20c/4]=0xe59f0010; // LDR r0,[pc,#10] -> 224 flag
        bus->code[0x210/4]=0xe1d020b0; // LDRH r2,[r0]
        bus->code[0x214/4]=0xe1822001; // ORR r2,r2,r1
        bus->code[0x218/4]=0xe1c020b0; // STRH r2,[r0]
        bus->code[0x21c/4]=0xe25ef004; // SUBS pc,lr,#4
        bus->code[0x220/4]=0x04000202;
        bus->code[0x224/4]=flag;
    }
    ~Fixture() {
        gbarecomp::runtime_set_owned_context(nullptr); runtime_shutdown();
        gbarecomp::set_active_bus(nullptr); gbarecomp::set_active_ppu(nullptr);
    }
    gbarecomp::debug::SnapshotContext context() {
        gbarecomp::debug::SnapshotContext c; c.bus=bus.get();c.ppu=ppu.get();
        c.rom_sha1="0000000000000000000000000000000000000000"; return c;
    }
};
std::vector<uint8_t> save(Fixture& f) {
    std::vector<uint8_t> out; std::string error;
    check(gbarecomp::debug::save_state_bytes(&out,f.context(),&error),error.c_str()); return out;
}
void boundary_code(Fixture& f) {
    f.bus->code[2]=0xe3a00000; // 08 MOV r0,#0
    f.bus->code[3]=0xe59f1008; // 0C LDR r1,[pc,#8] -> 1C
    f.bus->code[4]=0xe5c10000; // 10 STRB r0,[r1]
    f.bus->code[5]=0xe2822001; // 14 ADD r2,r2,#1
    f.bus->code[6]=0xe1b0f00e; // 18 MOVS pc,lr
    f.bus->code[7]=0x04000301;
}
void halt_boundary() {
    // Exact PR5 fixture, now a regression assertion instead of known divergence.
    for (bool reference : {true,false}) {
        Fixture f; boundary_code(f); runtime_call_push_return(main_return);
        bool yielded=false;
        try {
            if (!reference) { g_cpu.R[15]=main_pc; runtime_swi(0x020000); }
            else {
                g_cpu.banked_spsr[ARM_BANK_SUPERVISOR]=g_cpu.cpsr;
                g_cpu.banked_sp[ARM_BANK_USER]=g_cpu.R[13];
                g_cpu.R[13]=g_cpu.banked_sp[ARM_BANK_SUPERVISOR];
                g_cpu.cpsr=0x60000093;g_cpu.R[14]=main_pc;g_cpu.R[15]=8;
                runtime_tick(3);
                for(unsigned i=0;i<5;++i) runtime_force_interp_step();
            }
        } catch(const gbarecomp::RuntimeDispatchYield&) { yielded=true; }
        check(g_cpu.R[15]==0x14 && g_cpu.R[2]==0 && f.bus->io().halted() && g_runtime_cycles==9,
              "PR5 frontend/bridge stop before ADD at PC14, exactly 9 cycles");
        check(reference || yielded,"bridge suspends instead of returning the SWI");
        check(runtime_call_stack_depth()==1,"HALT does not consume native BL frame");
        if(!reference) check(gbarecomp::runtime_swi_depth()==1,"HALT keeps SWI ledger");
        const auto insns=gbarecomp::self_heal_interpreted_insns();
        const auto before=save(f);
        for(unsigned i=0;i<3;++i) {
            try { runtime_bridge_interpret(g_cpu.R[15],false,0,16); }
            catch(const gbarecomp::RuntimeDispatchYield&) {}
        }
        check(save(f)==before && gbarecomp::self_heal_interpreted_insns()==insns && !f.bus->halted_fetches,
              "re-entering a halted bridge executes/fetches nothing and mutates no state");
        std::printf("boundary %s PC=%08x R2=%u HALT=%u cycles=%llu yielded=%u\n",
                    reference?"reference":"bridge",g_cpu.R[15],g_cpu.R[2],f.bus->io().halted(),g_runtime_cycles,yielded);
    }
}
void masked_wake() {
    for(bool enabled:{false,true}) for(bool pending:{false,true})
      for(bool ime:{false,true}) for(bool masked:{false,true}) {
        Fixture f;
        g_cpu.cpsr=0x1f|(masked?CPSR_I_BIT:0);
        f.bus->write16(0x04000200,enabled?1:0); f.bus->write16(0x04000208,ime?1:0);
        if(pending) f.bus->io().request_irq(1);
        f.bus->write8(0x04000301,0);
        const auto pc=g_cpu.R[15]; runtime_tick(1);
        const bool wake=enabled&&pending, delivered=wake&&ime&&!masked;
        check(f.bus->io().halted()==!wake,"HALT wake depends on IE&IF, not IME/CPSR.I");
        check(g_runtime_irq_entries==unsigned(delivered),"delivery requires IE&IF,IME,!CPSR.I");
        if(!delivered) {
            check(g_cpu.R[15]==pc,"masked wake does not enter exception or advance PC");
            check(g_runtime_cycles==1+(wake?gba::kIrqWakeDelayCycles:0),"masked wake keeps established wake-cycle window");
            check(f.bus->io().if_reg()==unsigned(pending),"wake never acknowledges IF");
        }
    }
}

// Equivalent wait workload: guest code writes HALTCNT, then checks a guest-owned
// interrupt flag. Only guest IRQ code acknowledges IF and sets that flag.
void wait_code(Fixture& f) {
    f.bus->code[0x500/4]=0xe25ef004; // IRQ exception return
    f.bus->code[0x504/4]=0xe1b0f00e; // outer SVC exception return
    f.bus->code[2]=0xea00003c; // vector8 ->100
    auto put=[&](unsigned pc,uint32_t op){f.bus->code[pc/4]=op;};
    put(0x100,0xe92d4003); // PUSH r0,r1,lr (SVC stack)
    put(0x104,0xe321f01f); // MSR CPSR_c,#System (IRQ enabled)
    put(0x108,0xe3a00000); // MOV r0,#0
    put(0x10c,0xe59f102c); // LDR r1,[pc,#2c] ->140
    put(0x110,0xe5c10000); // STRB r0,[r1] HALT
    put(0x114,0xe59f1028); // LDR r1,[pc,#28] ->144
    put(0x118,0xe1d100b0); // LDRH r0,[r1]
    put(0x11c,0xe3100001); // TST r0,#1 (VBlank flag)
    put(0x120,0x0afffff8); // BEQ108
    put(0x124,0xe3a00000); // MOV r0,#0
    put(0x128,0xe1c100b0); // STRH r0,[r1]
    put(0x12c,0xe321f093); // MSR CPSR_c,#SVC|I
    put(0x130,0xe8bd4003); // POP r0,r1,lr
    put(0x134,0xe1b0f00e); // MOVS pc,lr
    put(0x140,0x04000301); put(0x144,flag);
}
bool reference_backend=false;
std::vector<uint8_t> captured_halt,captured_raw_final;
enum class Scenario { Plain, IrqSwi, NestedIrqSwi, NestedSwi };
Scenario scenario=Scenario::Plain;
constexpr uint32_t irq_outer_return=0x300, irq_swi_return=0x304, outer_swi_return=0x400;
uint32_t irq_slot() {return 0x03007000+g_irq_nest_depth*8;}
void interpreted_at(uint32_t pc) {
    g_cpu.R[15]=pc;
    if(reference_backend) runtime_force_interp_step();
    else runtime_bridge_interpret(pc,false,0,0);
}
void irq_finish() {
    const auto pc=g_cpu.R[15];
    check(runtime_call_should_return(pc),"IRQ helper consumes only its own BL frame");
    const auto slot=irq_slot();
    runtime_msr_cpsr(0x92,0xff);
    g_cpu.R[14]=bus_read_u32(slot);
    runtime_msr_spsr(bus_read_u32(slot+4),0xffffffff);
    interpreted_at(0x500);
}
void outer_swi_finish() {
    runtime_msr_cpsr(0x93,0xff);
    g_cpu.R[14]=bus_read_u32(0x03007080);
    runtime_msr_spsr(bus_read_u32(0x03007084),0xffffffff);
    interpreted_at(0x504);
}
void irq_entry() {
    const auto work_depth=scenario==Scenario::NestedIrqSwi?2u:1u;
    if((scenario==Scenario::IrqSwi || scenario==Scenario::NestedIrqSwi) && g_irq_nest_depth<=work_depth) {
        const auto slot=irq_slot();
        bus_write_u32(slot,g_cpu.R[14]);bus_write_u32(slot+4,runtime_mrs_spsr());
        runtime_msr_cpsr(0x1f,0xff);runtime_tick(1);
        const auto ret=g_irq_nest_depth==work_depth?irq_swi_return:irq_outer_return;
        runtime_call_push_return(ret);g_cpu.R[15]=ret;
        if(g_irq_nest_depth<work_depth) runtime_irq(ret);
        else runtime_swi(0x050000);
        return;
    }
    // A real device IRQ during the wait uses only the authored IRQ wrapper.
    if(!reference_backend) {runtime_bridge_interpret(0x18,false,0,0);return;}
    const auto depth=g_irq_nest_depth;
    while(g_irq_iret_depth!=depth) runtime_force_interp_step();
}
int reference_pc(uint32_t pc,int) {
    return pc<0x4000 && pc!=8 && pc!=0x18 && pc!=irq_outer_return && pc!=irq_swi_return && pc!=outer_swi_return;
}
unsigned caller_runs=0;
void swi_entry() {
    if(scenario==Scenario::NestedSwi && gbarecomp::runtime_swi_depth()==1) {
        bus_write_u32(0x03007080,g_cpu.R[14]);bus_write_u32(0x03007084,runtime_mrs_spsr());
        runtime_msr_cpsr(0x1f,0xff);g_cpu.R[15]=outer_swi_return;
        runtime_tick(1);runtime_swi(0x050000);return;
    }
    if(!reference_backend) {runtime_bridge_interpret(8,false,0,0);return;}
    const auto level=gbarecomp::runtime_swi_depth();
    while(gbarecomp::runtime_swi_depth()>=level) runtime_force_interp_step();
}
void caller() {
    ++caller_runs; ++g_cpu.R[4];
    g_cpu.R[15]=main_return; runtime_tick(1);runtime_call_should_return(main_return);
}
void boundary_unwound() {
    runtime_call_stack_restore(runtime_call_stack_data(),runtime_call_stack_depth());
    gbarecomp::runtime_set_return_yield(false);
}
// Exercise the shared common-runner idle pump, not a test-only timing model.
uint32_t idle_step(Fixture& f,uint32_t budget) {
    (void)f; return gbarecomp::runtime_pump_idle(budget);
}
void idle_latency() {
    for(bool pending:{false,true}) {
        Fixture f;g_cpu.cpsr|=CPSR_I_BIT;
        bus_write_u16(0x04000200,gba::GbaIo::IrqTimer0);
        bus_write_u16(0x04000208,0); // wake without IRQ delivery
        bus_write_u16(0x04000100,0xfff0);
        bus_write_u16(0x04000102,0xc0); // overflow after 16 cycles
        runtime_tick(3); // lazy device debt from the last guest instruction
        if(pending) f.bus->io().request_irq(gba::GbaIo::IrqTimer0);
        f.bus->write8(0x04000301,0);
        const auto before=g_runtime_cycles;
        idle_step(f,100);
        check(!f.bus->io().halted() && !g_runtime_irq_entries,"idle pump wakes on timer source without delivering masked IRQ");
        check(g_runtime_cycles==before+(pending?0:13)+gba::kIrqWakeDelayCycles,
              "idle pump pays no fictitious poll cycle or lazy-debt event overshoot");
    }
}

void wake_dma() {
    for(bool ime:{false,true}) {
        Fixture f;g_cpu.cpsr|=CPSR_I_BIT;
        bus_write_u16(0x04000200,gba::GbaIo::IrqTimer0);
        bus_write_u16(0x04000208,ime?1:0);
        bus_write_u32(0x02000000,0x1234abcd);
        bus_write_u32(0x040000b0,0x02000000);
        bus_write_u32(0x040000b4,0x03000000);
        bus_write_u32(0x040000b8,0xa4000001); // one HBlank word DMA
        const auto transfer=f.bus->access_cycles(0x02000000,4,false)+
                            f.bus->access_cycles(0x03000000,4,false)+2;
        runtime_tick(gba::GbaPpu::kDotsVisible*gba::GbaPpu::kCyclesPerDot-3);
        const auto before=g_runtime_cycles;
        f.bus->io().request_irq(gba::GbaIo::IrqTimer0);f.bus->write8(0x04000301,0);
        idle_step(f,100);
        check(!f.bus->io().halted() && !g_runtime_irq_entries &&
              f.bus->read32(0x03000000)==0x1234abcd && f.bus->io().dma_runs(0)==1,
              "HBlank DMA executes during masked wake without guest CPU instruction");
        check(g_runtime_cycles==before+gba::kIrqWakeDelayCycles+transfer &&
              f.bus->io().take_dma_steal_cycles()==0,
              "wake-window DMA stolen cycles are paid before CPU resumes or snapshots");
    }
}
void drive(Fixture& f) {
    unsigned guard=0;
    while((gbarecomp::runtime_swi_depth()||g_irq_nest_depth||g_cpu.R[15]!=main_return) && ++guard<20000) {
        try {
            if(f.bus->io().halted()) idle_step(f,4096);
            else if(g_irq_nest_depth) gbarecomp::runtime_resume_irq();
            else if(reference_backend && reference_pc(g_cpu.R[15],0)) runtime_force_interp_step();
            else runtime_dispatch(g_cpu.R[15]);
        } catch(const gbarecomp::RuntimeDispatchYield&) { boundary_unwound(); }
    }
    check(guard<20000,"resumption reaches actual SWI/native caller return");
}
std::vector<uint8_t> wait_run(bool reference,bool reload,bool thumb,unsigned repeats,Scenario which=Scenario::Plain) {
    Fixture f; wait_code(f);reference_backend=reference;g_force_interp=0;scenario=which;
    g_runtime_force_interp_hook=reference?reference_pc:nullptr;
    caller_runs=0;
    g_cpu.cpsr|=thumb?CPSR_T_BIT:0;
    g_cpu.R[0]=0x12345678;g_cpu.R[1]=0x87654321;
    f.bus->write16(0x04000004,8); // enable PPU VBlank IRQ
    f.bus->write16(0x04000200,1);f.bus->write16(0x04000208,1);
    for(unsigned n=0;n<repeats;++n) {
        g_cpu.R[15]=main_pc;runtime_call_push_return(main_return);
        bool suspended=false;
        try {
            if(which==Scenario::IrqSwi || which==Scenario::NestedIrqSwi) runtime_irq(main_pc);
            else runtime_swi(thumb?5:0x050000);
        } catch(const gbarecomp::RuntimeDispatchYield&){boundary_unwound();suspended=true;}
        check(suspended && f.bus->io().halted() && g_cpu.R[15]==0x114,
              "SWI05 authored wait suspends at post-HALTCNT internal System PC");
        const unsigned irqs=which==Scenario::NestedIrqSwi?2:which==Scenario::IrqSwi?1:0;
        const unsigned swis=which==Scenario::NestedSwi?2:1;
        check(g_irq_nest_depth==irqs && runtime_call_stack_floor()==irqs && runtime_call_stack_depth()==irqs+1 &&
              gbarecomp::runtime_swi_depth()==swis && !caller_runs,
              "waiting SWI retains BL/ledger and cannot execute native continuation early");
        const auto at_halt=save(f);if(n==0) captured_halt=at_halt;
        const auto old_insns=gbarecomp::self_heal_interpreted_insns();
        // Device events before VBlank advance only devices, not CPU/BL/SWI.
        idle_step(f,10);
        check(g_cpu.R[15]==0x114 && gbarecomp::self_heal_interpreted_insns()==old_insns && !f.bus->halted_fetches,
              "idle event pump executes no halted guest instruction");
        if(reload) {
            std::string error;
            for(auto& irq:f.owner.interrupts) irq.preserved.fill(0xdeadbeef);
            gbarecomp::RuntimeArmContext clean;clean.cpu.cpsr=0x1f;clean.force_interp=reference?reference_pc:nullptr;
            gbarecomp::runtime_set_owned_context(&f.replacement);
            gbarecomp::runtime_restore_arm_context(clean);
            check(gbarecomp::debug::load_state_bytes(at_halt.data(),at_halt.size(),f.context(),&error),error.c_str());
            check(save(f)==at_halt,"full HALT snapshot restored to different continuation owner");
            idle_step(f,10);
        }
        drive(f);
        check(runtime_call_stack_depth()==0 && !gbarecomp::runtime_swi_depth() && !g_irq_nest_depth,
              "wake and true SVC/native return consume exactly their own frames");
        check(g_cpu.R[0]==0x12345678 && g_cpu.R[1]==0x87654321 && f.bus->read16(flag)==0,
              "wait preserves caller registers and clears flag in authored guest code");
        check(caller_runs==1,"native continuation executes exactly once per completed SWI");
        caller_runs=0; // counter is per call for the waiting assertion
    }
    captured_raw_final=save(f);
    runtime_mmio_catch_up(); // compare materialized device boundaries, without charging cycles
    gbarecomp::RuntimeTimingContext timing;gbarecomp::runtime_capture_timing_context(timing);
    std::printf("wait scenario=%u ref=%u reload=%u thumb=%u cycles=%llu debt=%llu horizon=%lld IRQ=%llu PC=%08x\n",unsigned(which),reference,reload,thumb,g_runtime_cycles,(unsigned long long)timing.pending_cycles,(long long)timing.event_budget,g_runtime_irq_entries,g_cpu.R[15]);
    const auto result=save(f);g_force_interp=0;g_runtime_force_interp_hook=nullptr;return result;
}
bool same(const std::vector<uint8_t>& a,const std::vector<uint8_t>& b) {
    if(a==b)return true;
    size_t i=0;while(i<a.size()&&i<b.size()&&a[i]==b[i])++i;
    std::fprintf(stderr,"snapshot difference offset=%zu sizes=%zu/%zu values=%u/%u\n",i,a.size(),b.size(),i<a.size()?a[i]:0,i<b.size()?b[i]:0);return false;
}
void repeated_wait() {
    for(auto which:{Scenario::Plain,Scenario::IrqSwi,Scenario::NestedIrqSwi,Scenario::NestedSwi})
      for(bool thumb:{false,true}) {
        auto actual=wait_run(false,false,thumb,3,which);
        const auto raw_actual=captured_raw_final;
        const auto reloaded=wait_run(false,true,thumb,3,which);
        check(same(raw_actual,captured_raw_final),"resumed final snapshot also matches before materializing lazy cycle debt");
        check(same(actual,reloaded),"SWI05 full final state equal with HALT save/load");
        check(same(actual,wait_run(true,false,thumb,3,which)),"SWI05 bridge matches reference CPU, cycles and all serialized devices");
    }
}


void fresh_machine() {
    const auto expected=wait_run(false,false,true,1,Scenario::NestedIrqSwi);
    const auto paused=captured_halt, raw_expected=captured_raw_final;
    // wait_run's bus/PPU/owner are destroyed before this machine exists.
    Fixture fresh;wait_code(fresh);scenario=Scenario::NestedIrqSwi;reference_backend=false;
    caller_runs=0;std::string error;
    check(gbarecomp::debug::load_state_bytes(paused.data(),paused.size(),fresh.context(),&error),error.c_str());
    check(save(fresh)==paused && g_irq_nest_depth==2 && gbarecomp::runtime_swi_depth()==1 && fresh.bus->io().halted(),
          "new machine restores complete HALT and nested IRQ/SWI state without previous host addresses");
    idle_step(fresh,10);drive(fresh);
    check(same(save(fresh),raw_expected),"new-machine final lazy timing debt and device bytes match before flush");
    runtime_mmio_catch_up();
    check(same(save(fresh),expected),"fresh machine resumes halted nested service to identical full final state");
}
void native_halt() {
    bus_write_u8(0x04000301,0);g_cpu.R[15]=0x03003004;runtime_tick(1);
    if(runtime_should_yield()) return;
    ++g_cpu.R[7]; // must not execute under HALT
}
void native_boundary() {
    Fixture f;runtime_call_push_return(main_return);g_cpu.R[15]=0x03003000;
    bool paused=false;try {runtime_dispatch(g_cpu.R[15]);}
    catch(const gbarecomp::RuntimeDispatchYield&) {paused=true;boundary_unwound();}
    check(paused && g_cpu.R[15]==0x03003004 && !g_cpu.R[7] && runtime_call_stack_depth()==1,
          "native HALT prologue uses the same owned unwind and preserves its BL");
}
int benchmark_wait() {
    Fixture f;wait_code(f);scenario=Scenario::Plain;reference_backend=false;g_force_interp=0;
    g_runtime_force_interp_hook=nullptr;
    bus_write_u16(0x04000004,8);bus_write_u16(0x04000200,1);bus_write_u16(0x04000208,1);
    constexpr unsigned repeats=120;caller_runs=0;
    const auto start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<repeats;++i) {
        runtime_call_push_return(main_return);g_cpu.R[15]=main_pc;
        try {runtime_swi(0x050000);} catch(const gbarecomp::RuntimeDispatchYield&) {boundary_unwound();}
        drive(f);
    }
    const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    check(caller_runs==repeats && !g_irq_nest_depth && !gbarecomp::runtime_swi_depth() && !runtime_call_stack_depth(),
          "full-path benchmark completes every service and native caller");
    std::printf("authored_wait calls=%u host_ns=%lld guest_cycles=%llu instructions=%u halted_instructions=%u IRQ=%llu\n",
                repeats,(long long)ns,g_runtime_cycles,f.bus->instruction_fetches,f.bus->halted_fetches,g_runtime_irq_entries);
    return failures?1:0;
}
void boundary_stop() {
    Fixture f;boundary_code(f);g_cpu.R[15]=8;g_cpu.cpsr=0x93;g_cpu.R[14]=0x14;
    runtime_call_push_return(0x14);
    bool paused=false;try {runtime_bridge_interpret(8,false,0,0);}
    catch(const gbarecomp::RuntimeDispatchYield&) {paused=true;boundary_unwound();}
    check(paused && g_cpu.R[15]==0x14 && runtime_call_stack_depth()==1,
          "HALT at a stop address suspends without returned-frame cleanup");
    const auto bytes=save(f);
    check(runtime_bridge_interpret(0x14,false,0x18,16)==0 && save(f)==bytes,
          "bounded HALT pass fails without execution or returned-frame cleanup");
}
} // namespace
extern "C" const DispatchEntry kDispatchTable[]={{main_pc,0,1,caller},{main_pc,1,1,caller},{0x03003000,0,0,native_halt}};
extern "C" const unsigned kDispatchTableLen=3;
extern "C" const DispatchEntry kBiosDispatchTable[]={{8,0,0,swi_entry},{0x18,0,0,irq_entry},
    {irq_outer_return,0,1,irq_finish},{irq_swi_return,0,1,irq_finish},
    {outer_swi_return,0,1,outer_swi_finish}};
extern "C" const unsigned kBiosDispatchTableLen=5;
int main(int argc,char** argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
#if defined(_WIN32)
    _putenv_s("GBARECOMP_SELFHEAL_RECOMPILE","0");_putenv_s("GBARECOMP_YIELD_ON_VBLANK","0");
#else
    setenv("GBARECOMP_SELFHEAL_RECOMPILE","0",1);setenv("GBARECOMP_YIELD_ON_VBLANK","0",1);
#endif
    if(argc==2 && std::string(argv[1])=="--benchmark") return benchmark_wait();
    if(argc==2 && std::string(argv[1])=="--dma-wake") {wake_dma();return failures?1:0;}
    halt_boundary(); masked_wake(); idle_latency(); wake_dma(); boundary_stop(); native_boundary(); repeated_wait(); fresh_machine();
    std::printf("halt_resume_tests: %u failures\n",failures);return failures?1:0;
}
