#pragma once

#include <string>
#include <vector>

#include "experiment_types.hpp"
#include "../third_party/nlohmann_json.hpp"

namespace teknegram {

struct ExperimentOptions {
    std::string dataset_dir;
    std::string experiment_title;
    RunMode mode;
    std::vector<CorpusSizeConfig> size_configs;
    std::vector<CompositionConfig> composition_configs;
    std::vector<std::uint32_t> frequency_thresholds_pm;
    std::vector<std::uint32_t> document_dispersion_thresholds;
    std::uint32_t sample_count;
    std::uint32_t bundle_size;
    double coverage_target;
    std::uint32_t random_seed;
    bool run_conditional;
    bool run_unconditional;
    bool emit_sample_level_sets;
    bool emit_intermediate_artifacts;
    bool emit_debug_bundle_counts;
    bool emit_sample_summary_json;
    std::string artifact_input_dir;
    std::string requested_experiment_code;

    ExperimentOptions();

    static ExperimentOptions FromJson(const nlohmann::json& input);
    static ExperimentOptions FromCli(int argc, char** argv);
    nlohmann::json ToJson() const;
    void Validate() const;
};

} // namespace teknegram
