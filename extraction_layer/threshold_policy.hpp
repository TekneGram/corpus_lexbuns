#pragma once

#include <cstdint>

#include "../experiment_shared_layer/experiment_types.hpp"

namespace teknegram {

class ThresholdPolicy {
    public:
        bool passes_conditional_rule(std::uint32_t raw_count,
                                     std::uint16_t document_range,
                                     const ExperimentCondition& condition) const;

        std::uint32_t raw_frequency_cutoff(const ExperimentCondition& condition) const;
};

} // namespace teknegram
