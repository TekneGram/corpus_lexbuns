#include "sample_condition_planner.hpp"

#include <cmath>
#include <stdexcept>

#include "../experiment_shared_layer/json_io.hpp"

namespace teknegram {

SampleConditionPlanner::SampleConditionPlanner() {}

std::vector<SamplingDesign> SampleConditionPlanner::build_sampling_designs(
    const ExperimentOptions& options) const {
    std::vector<SamplingDesign> designs;
    for (std::size_t i = 0; i < options.size_configs.size(); ++i) {
        for (std::size_t j = 0; j < options.composition_configs.size(); ++j) {
            designs.push_back(build_sampling_design(options.size_configs[i],
                                                    options.composition_configs[j],
                                                    options));
        }
    }
    return designs;
}

std::vector<ExperimentCondition> SampleConditionPlanner::build_conditions(
    const ExperimentOptions& options) const {
    std::vector<ExperimentCondition> conditions;
    const std::vector<SamplingDesign> designs = build_sampling_designs(options);
    for (std::size_t i = 0; i < designs.size(); ++i) {
        const std::vector<ExperimentCondition> design_conditions =
            build_conditions_for_design(designs[i], options);
        conditions.insert(conditions.end(), design_conditions.begin(), design_conditions.end());
    }
    return conditions;
}

std::vector<ExperimentCondition> SampleConditionPlanner::build_conditions_for_design(
    const SamplingDesign& design,
    const ExperimentOptions& options) const {
    std::vector<ExperimentCondition> conditions;

    if (options.run_conditional) {
        for (std::size_t i = 0; i < options.frequency_thresholds_pm.size(); ++i) {
            for (std::size_t j = 0; j < options.document_dispersion_thresholds.size(); ++j) {
                for (std::size_t k = 0; k < options.coverage_targets.size(); ++k) {
                    conditions.push_back(build_condition(design,
                                                         true,
                                                         options.frequency_thresholds_pm[i],
                                                         options.coverage_targets[k],
                                                         options.document_dispersion_thresholds[j],
                                                         options));
                }
            }
        }
    }

    if (options.run_unconditional) {
        for (std::size_t i = 0; i < options.coverage_targets.size(); ++i) {
            conditions.push_back(build_condition(design, false, 0, options.coverage_targets[i], 0, options));
        }
    }

    return conditions;
}

SamplingDesign SampleConditionPlanner::build_sampling_design(const CorpusSizeConfig& size_config,
                                                             const CompositionConfig& composition_config,
                                                             const ExperimentOptions& options) const {
    SamplingDesign design;
    design.sampling_design_id = BuildSamplingDesignId(size_config, composition_config);
    design.corpus_size_tokens = size_config.target_tokens;
    design.corpus_delta_tokens = size_config.delta_tokens;
    design.composition_label = composition_config.label;
    design.sample_count = options.sample_count;
    design.random_seed = options.random_seed;
    return design;
}

ExperimentCondition SampleConditionPlanner::build_condition(
    const SamplingDesign& design,
    bool conditional_mode,
    std::uint32_t frequency_threshold_pm,
    double coverage_target,
    std::uint32_t document_dispersion_threshold,
    const ExperimentOptions& options) const {
    ExperimentCondition condition;
    condition.condition_id = BuildConditionId(design,
                                              conditional_mode,
                                              options.bundle_size,
                                              frequency_threshold_pm,
                                              document_dispersion_threshold,
                                              coverage_target);
    condition.sampling_design_id = design.sampling_design_id;
    condition.bundle_size = options.bundle_size;
    condition.conditional_mode = conditional_mode;
    condition.corpus_size_tokens = design.corpus_size_tokens;
    condition.corpus_delta_tokens = design.corpus_delta_tokens;
    condition.composition_label = design.composition_label;
    condition.frequency_threshold_pm = conditional_mode ? frequency_threshold_pm : 0U;
    condition.frequency_threshold_raw = conditional_mode
        ? frequency_threshold_raw(frequency_threshold_pm, design.corpus_size_tokens)
        : 0U;
    condition.document_dispersion_threshold = conditional_mode ? document_dispersion_threshold : 0U;
    condition.sample_count = design.sample_count;
    condition.coverage_target = coverage_target;
    return condition;
}

std::uint32_t SampleConditionPlanner::frequency_threshold_raw(std::uint32_t frequency_threshold_pm,
                                                             std::uint32_t corpus_size_tokens) const {
    const double raw = (static_cast<double>(frequency_threshold_pm) * static_cast<double>(corpus_size_tokens)) /
                       1000000.0;
    const double rounded = std::ceil(raw);
    return static_cast<std::uint32_t>(rounded);
}

} // namespace teknegram
