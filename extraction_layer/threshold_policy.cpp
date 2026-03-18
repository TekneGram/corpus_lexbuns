#include "threshold_policy.hpp"

namespace teknegram {

bool ThresholdPolicy::passes_conditional_rule(std::uint32_t raw_count,
                                              std::uint16_t document_range,
                                              const ExperimentCondition& condition) const {
    return raw_count >= raw_frequency_cutoff(condition) &&
           document_range >= static_cast<std::uint16_t>(condition.document_dispersion_threshold);
}

std::uint32_t ThresholdPolicy::raw_frequency_cutoff(const ExperimentCondition& condition) const {
    return condition.frequency_threshold_raw;
}

} // namespace teknegram
