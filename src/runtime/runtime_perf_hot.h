// Android live performance HUD: diagnostic-only bridge hotspots.
// No guest state or instruction timing is changed by this API.
#pragma once
#include <cstdint>

#if defined(GBARECOMP_PERF_HUD)
struct RuntimePerfHotBridge {
    std::uint32_t pc = 0;
    std::uint32_t thumb = 0;
    std::uint64_t elapsed_ns = 0;
    std::uint64_t instructions = 0;
    std::uint64_t calls = 0;
};

// Returns the top three completed bridge entry PCs by elapsed host time
// since the previous sample. Timings are inclusive and may overlap on nested
// bridge calls; empty results contain zero counts. Single guest thread only.
extern "C" void runtime_perf_bridge_top_sample(RuntimePerfHotBridge output[3]);
extern "C" std::uint32_t runtime_perf_bridge_active_pc();
extern "C" std::uint32_t runtime_perf_bridge_active_pc();

// Only compiled in the Android diagnostic targets. Filled at the *actual*
// unsupported SWI entry before the architecture switches to SVC mode.
// Return address is converted to the true guest ARM/Thumb callsite.
extern "C" void runtime_perf_note_swi(std::uint32_t number,
                                       std::uint32_t callsite,
                                       std::uint32_t caller_thumb);

struct RuntimePerfSwiSite {
    std::uint32_t number = 0, callsite = 0, thumb = 0;
    std::uint64_t elapsed_ns = 0, instructions = 0, calls = 0;
};
struct RuntimePerfPcSample {
    std::uint32_t pc = 0, thumb = 0;
    std::uint64_t samples = 0;
};
extern "C" void runtime_perf_swi_top_sample(RuntimePerfSwiSite output[3]);
extern "C" void runtime_perf_internal_pc_top_sample(
    RuntimePerfPcSample output[3], std::uint64_t* dropped,
    std::uint64_t* halted_samples);

// Timed only while an interpreter bridge is active. Already included in
// bridge time; never subtract without accounting for nested work explicitly.
extern "C" unsigned long long g_perf_irq_in_bridge_ns;
#endif
