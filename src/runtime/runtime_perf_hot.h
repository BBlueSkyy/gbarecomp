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
#endif
