#pragma once

#include <string>
#include <vector>

#include "../data_access_layer/corpus_dataset.hpp"
#include "../experiment_shared_layer/experiment_options.hpp"
#include "../experiment_shared_layer/experiment_types.hpp"
#include "progress_emitter.hpp"

namespace teknegram {

class ExperimentEngine {
    public:
        ExperimentEngine();

        ExperimentRunResult run(const ExperimentOptions& options,
                                const ProgressEmitter* progress_emitter = 0) const;
        void validate_options(const ExperimentOptions& options) const;
        CorpusDataset open_dataset(const ExperimentOptions& options) const;
        std::vector<ExperimentCondition> build_conditions(const ExperimentOptions& options,
                                                          const CorpusDataset& dataset) const;
        std::vector<ConditionRunResult> run_sampling_designs(
            const CorpusDataset& dataset,
            const std::vector<ExperimentCondition>& conditions,
            const ExperimentOptions& options,
            const ProgressEmitter* progress_emitter) const;
        ExperimentRunResult run_stepwise(const ExperimentOptions& options,
                                         const ProgressEmitter* progress_emitter = 0) const;
        void write_run_manifest(const ExperimentOptions& options,
                                const std::vector<ExperimentCondition>& conditions) const;
        void write_final_comparison_artifact(const ExperimentRunResult& result) const;
        ExperimentRunResult run_fun_mode(const ExperimentOptions& options,
                                         const ProgressEmitter* progress_emitter = 0) const;
        ExperimentRunResult run_sample_only(const CorpusDataset& dataset,
                                            const std::vector<ExperimentCondition>& conditions,
                                            const ExperimentOptions& options,
                                            const ProgressEmitter* progress_emitter = 0) const;
        ExperimentRunResult run_extract_only(const CorpusDataset& dataset,
                                             const std::vector<ExperimentCondition>& conditions,
                                             const ExperimentOptions& options,
                                             const ProgressEmitter* progress_emitter = 0) const;
        ExperimentRunResult run_analyze_only(const CorpusDataset& dataset,
                                             const std::vector<ExperimentCondition>& conditions,
                                             const ExperimentOptions& options,
                                             const ProgressEmitter* progress_emitter = 0) const;
        ExperimentRunResult run_sampling_diagnostics_only(const ExperimentOptions& options,
                                                          const ProgressEmitter* progress_emitter = 0) const;

    private:
        void emit_stage_progress(const ProgressEmitter* emitter,
                                 const std::string& stage,
                                 const std::string& message,
                                 int percent) const;
        void ensure_output_layout(const ExperimentOptions& options,
                                  std::string* artifact_dir,
                                  std::string* experiment_code) const;
        std::vector<SamplingDesign> build_sampling_designs(const ExperimentOptions& options) const;
        std::string timestamp_now() const;
        static void write_json_file(const std::string& path, const nlohmann::json& payload);
        static void ensure_directory(const std::string& path);
        static void ensure_directory_recursive(const std::string& path);
        static std::string join_path(const std::string& left, const std::string& right);
        static bool file_exists(const std::string& path);
        static std::string read_text_file(const std::string& path);
        static std::string resolve_condition_filename(const std::string& prefix,
                                                      const ExperimentCondition& condition,
                                                      const std::string& suffix);
        static ExperimentCondition build_condition(const SamplingDesign& sampling_design,
                                                   bool conditional_mode,
                                                   std::uint32_t bundle_size,
                                                   std::uint32_t frequency_threshold_pm,
                                                   std::uint32_t frequency_threshold_raw,
                                                   std::uint32_t document_dispersion_threshold,
                                                   std::uint32_t sample_count,
                                                   double coverage_target);
};

} // namespace teknegram
