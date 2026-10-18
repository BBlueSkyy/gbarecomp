#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "function_finder.h"

namespace gbarecomp {

// Exact, already-discovered instruction spans. Compiled before the game runs;
// runtime matching neither decodes instructions nor creates executable code.
struct RamCodeTemplate {
    Function function;
    std::vector<uint8_t> bytes;
    Function cart_function{};
};

std::vector<RamCodeTemplate> collect_ram_code_templates(
    const std::vector<Function>& functions, const uint8_t* image,
    std::size_t image_size, uint32_t image_base, const std::string& prefix,
    const std::vector<DataRange>* data_ranges = nullptr,
    const std::vector<CodeCopyRange>* code_copies = nullptr);

}  // namespace gbarecomp
