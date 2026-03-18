#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../experiment_shared_layer/experiment_options.hpp"

namespace teknegram {

class SampleConditionPlanner {
    public:
        SampleConditionPlanner();

        std::vector<SamplingDesign> build_sampling_designs(const ExperimentOptions& options) const;
        std::vector<ExperimentCondition> build_conditions(const ExperimentOptions& options) const;
        std::vector<ExperimentCondition> build_conditions_for_design(const SamplingDesign& design,
                                                                     const ExperimentOptions& options) const;
        SamplingDesign build_sampling_design(const CorpusSizeConfig& size_config,
                                             const CompositionConfig& composition_config,
                                             const ExperimentOptions& options) const;
        ExperimentCondition build_condition(const SamplingDesign& design,
                                            bool conditional_mode,
                                            std::uint32_t frequency_threshold_pm,
                                            double coverage_target,
                                            std::uint32_t document_dispersion_threshold,
                                            const ExperimentOptions& options) const;
        std::uint32_t frequency_threshold_raw(std::uint32_t frequency_threshold_pm,
                                              std::uint32_t corpus_size_tokens) const;
};

} // namespace teknegram
