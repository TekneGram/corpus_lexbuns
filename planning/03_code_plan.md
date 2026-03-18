# C++ Code Plan for the Lexical Bundle Experiment

This document proposes a C++ code structure for the lexical bundle experiment. It follows the same broad pattern as `native/corpus_builder`: a `main.cpp` entrypoint, an orchestration layer, and subfolders grouped by responsibility.

## Experiment execution model

The experiment should be run by sampling design, not by running a completely separate end-to-end process for each threshold condition.

Threshold and document-dispersion conditions should be treated as paired conditions within the same samples. That is, for a fixed corpus size and composition, the same accepted sample is reused for every threshold/dispersion setting under that sampling design. This is methodologically useful because it isolates the effect of the extraction rule from random variation in the sampled corpus.

Corpus size conditions and corpus composition conditions should be treated as separate sampling conditions. They are different sampling designs rather than paired threshold variants within the exact same sample stream.

## Practical form

So the real loop is:

```text
for each corpus size:
    for each composition:
        initialize aggregates for all conditions under this sampling design
        repeat 10,000 times:
            create one sample
            count bundle frequencies and document ranges once
            update every matching condition aggregate
        finalize robust sets for those conditions

run cross-condition analysis on all finalized robust sets
```

This loop describes the full production run. The same design should also support stepwise execution so that a user can stop after sampling, inspect artefacts, and resume later from those artefacts rather than rerunning the entire pipeline.

This code is an experiment runner, not a corpus builder. It assumes that the corpus dataset has already been created by `corpus_builder` and that the dataset directory already contains `corpus.db`, sparse matrix files, lexicons, and other required binary artefacts. The lexical-bundle code should open and use those existing files. It should not create or rebuild the underlying corpus dataset. The only new writes under the dataset directory should be experiment records in the existing `corpus.db` and new experiment artefact folders under `experiments/`.

Recommended run modes:

- `runExperiment`
  Full end-to-end run: sampling, extraction, robust-set construction, and final comparison.
- `sampleOnly`
  Builds samples, writes sampling artefacts, and stops.
- `extractOnly`
  Loads existing sample artefacts, runs extraction, writes aggregate artefacts, and stops.
- `analyzeOnly`
  Loads aggregate artefacts, builds robust sets, runs comparisons, and writes final analysis artefacts.
- `inspectArtifacts`
  Reads previously written artefacts and emits JSON summaries for app inspection.
- `funRun`
  A lightweight exploratory run with reduced sample count and compact outputs so users can validate the pipeline before a full run.

In stepwise mode, each stage should be able to start either from the raw dataset or from the artefacts produced by the previous stage.

All experimental variables should be user-configurable before the run begins. In particular, the request format should allow the user to specify:

- corpus sizes
- the delta associated with each corpus size
- composition ranges
- cutoff frequencies
- document-dispersion thresholds
- number of samples per experiment
- random seed

Because delta depends on corpus size, the configuration should not treat these as independent lists. Instead, each corpus size should be paired explicitly with its allowed delta.

Recommended configuration shape:

```json
{
  "datasetDir": "...",
  "mode": "runExperiment",
  "sampleCount": 10000,
  "randomSeed": 12345,
  "bundleSize": 4,
  "coverageTarget": 0.9,
  "sizeConfigs": [
    { "targetTokens": 100000, "deltaTokens": 5000 },
    { "targetTokens": 250000, "deltaTokens": 7500 },
    { "targetTokens": 500000, "deltaTokens": 10000 }
  ],
  "compositionConfigs": [
    { "label": "short", "minTokens": 0, "maxTokens": 1800 },
    { "label": "mid", "minTokens": 1800, "maxTokens": 2800 },
    { "label": "long", "minTokens": 2800, "maxTokens": 0 }
  ],
  "frequencyThresholdsPm": [40, 20, 10, 5],
  "documentDispersionThresholds": [10, 5]
}
```

Here `maxTokens = 0` can be treated as open-ended by convention, or the final implementation can use an explicit nullable/open-ended representation.

## Artefacts and progress

The run should emit a small number of artefacts and frequent progress events. Artefacts should be written at stage boundaries so that later stages can be inspected or resumed without repeating the entire pipeline.

Each experiment run should have its own dedicated artefact folder. New runs should never reuse an old artefact directory. The experiment folders should live inside the corpus dataset directory produced by `corpus_builder`, alongside `corpus.db` and the binary matrix artefacts.

Recommended dataset-local layout:

```text
<dataset_dir>/
├── corpus.db
├── 4gram.spm.meta.bin
├── 4gram.spm.offsets.bin
├── 4gram.spm.entries.bin
└── experiments/
    ├── exp_000001/
    ├── exp_000002/
    └── exp_000003/
```

Each experiment folder should contain only the artefacts for one run. Filenames inside that folder should be fixed by the code and should not be user-editable. This makes the run easy to inspect and means the app only needs the experiment ID and folder path to locate the outputs.

Recommended examples of fixed filenames:

- `run_manifest.json`
- `sample_summary_<sampling_design_id>.json`
- `sample_membership_<sampling_design_id>.bin`
- `aggregate_<condition_id>.bin`
- `robust_set_<condition_id>.json`
- `final_comparisons.json`

The database should be used as an experiment registry, not as the main artefact store. Large binary artefacts should remain on disk.

The lexical-bundle code should not create `corpus.db` itself. That database is assumed to have already been created by `corpus_builder` in the dataset directory passed to this code. The lexical-bundle runner only needs to open the existing `corpus.db`, read metadata from it, and append experiment-registry rows to it.

Recommended `experiments` table in `corpus.db`:

- `experiment_id INTEGER PRIMARY KEY`
- `experiment_code TEXT NOT NULL UNIQUE`
- `title TEXT NOT NULL`
- `artifact_dir TEXT NOT NULL`
- `mode TEXT NOT NULL`
- `status TEXT NOT NULL`
- `created_at TEXT NOT NULL`
- `completed_at TEXT`
- `random_seed INTEGER`
- `options_json TEXT NOT NULL`

This table is enough to:

- list past experiment runs in the app
- retrieve an experiment by ID
- locate the correct artefact directory
- know the run mode, seed, and options used

An optional future table can be added if finer querying is needed:

- `experiment_artifacts`
  This would index individual artefacts by type and condition ID, but it is not required for the first implementation if filenames remain fixed and predictable.

Recommended lifecycle:

1. create a new experiment record in `corpus.db` with status `running`
2. create a new dataset-local folder such as `experiments/exp_000001/`
3. write `run_manifest.json`
4. write stage artefacts into that folder as the run proceeds
5. update the experiment record to `completed` or `failed`

Recommended minimal artefacts:

- one run manifest for the whole run
- one sample summary JSON manifest per sampling design
- one sample membership artefact per sampling design
- one condition aggregate artefact per final condition
- one robust set artefact per final condition
- one final comparison artefact for the whole run

Optional debug artefacts:

- sample-level extracted bundle sets
- sample-level bundle count dumps

These debug artefacts should be off by default. Sample-level bundle count dumps are especially not recommended because they grow quickly and duplicate information that can be recomputed from sample membership plus the sparse matrix.

Sample-level extracted bundle sets are also not required for robust-set construction. The robust set is built from running condition aggregates, not from storing the full history of all sample-level outputs. In practice, each sample is processed once, its bundle statistics are used to update aggregate vectors for the relevant conditions, and the sample-level vectors can then be discarded. For the conditional approach, the aggregate only needs quantities such as extracted-sample counts and accumulated thresholded mass per feature. For the unconditional approach, it only needs accumulated expected mass per feature. Storing all sample-level extracted sets is therefore optional and only useful for debugging, audit, or detailed stability diagnostics.

Recommended progress event points:

- run started
- dataset opened
- conditions expanded
- sampling design started
- accepted sample count advanced
- sampling retry count advanced when needed
- extraction aggregate updated
- robust set finalized
- cross-condition analysis started
- run completed

Each progress event should include:

- stage
- condition or sampling-design identifier
- current count
- total count
- percent
- short message

The sample summary JSON manifest is specifically for inspection. It should expose the key properties of a sampling design without requiring a future reader to decode the binary sample-membership artefact. It should include:

- sampling-design identifier
- corpus size
- composition
- random seed
- target sample count
- accepted sample count
- token-count summary statistics
- document-count summary statistics
- paths to the binary sample-membership artefact and related run manifest

The experiment has three main stages:

1. sampling
2. extraction
3. analysis

It also needs:

- CLI mode
- JSON mode
- progress emission
- shared config and result types

The emphasis should be on efficient repeated Monte Carlo runs over the existing binary corpus dataset, especially the 4-gram sparse matrix and document metadata.

## 1. Suggested folder structure

```text
native/corpus_lexbuns/
├── main.cpp
├── orchestration_layer/
│   ├── experiment_engine.hpp
│   ├── experiment_engine.cpp
│   ├── progress_emitter.hpp
│   └── progress_emitter.cpp
├── experiment_shared_layer/
│   ├── experiment_types.hpp
│   ├── experiment_options.hpp
│   ├── experiment_options.cpp
│   ├── json_io.hpp
│   └── json_io.cpp
├── data_access_layer/
│   ├── corpus_dataset.hpp
│   ├── corpus_dataset.cpp
│   ├── metadata_repository.hpp
│   ├── metadata_repository.cpp
│   ├── lexicon_reader.hpp
│   ├── lexicon_reader.cpp
│   ├── sparse_matrix_reader.hpp
│   └── sparse_matrix_reader.cpp
├── sampling_layer/
│   ├── sample_condition_planner.hpp
│   ├── sample_condition_planner.cpp
│   ├── document_pool_builder.hpp
│   ├── document_pool_builder.cpp
│   ├── corpus_sampler.hpp
│   └── corpus_sampler.cpp
├── extraction_layer/
│   ├── bundle_counter.hpp
│   ├── bundle_counter.cpp
│   ├── threshold_policy.hpp
│   ├── threshold_policy.cpp
│   ├── conditional_extractor.hpp
│   ├── conditional_extractor.cpp
│   ├── unconditional_extractor.hpp
│   └── unconditional_extractor.cpp
├── analysis_layer/
│   ├── robust_set_builder.hpp
│   ├── robust_set_builder.cpp
│   ├── stability_analyzer.hpp
│   ├── stability_analyzer.cpp
│   ├── comparison_analyzer.hpp
│   ├── comparison_analyzer.cpp
│   ├── result_formatter.hpp
│   └── result_formatter.cpp
└── third_party/
    └── nlohmann/json.hpp
```

## 2. Entry point and orchestration

## 2.1 `main.cpp`

Purpose:

- match the `corpus_builder/main.cpp` pattern
- support CLI mode when no JSON is piped to stdin
- support JSON mode when JSON is supplied on stdin

Key functions:

- `std::string ReadStdin()`
  Reads stdin and decides whether to run JSON mode.

- `bool HasNonWhitespace(const std::string& value)`
  Detects whether stdin contains a JSON request.

- `int RunCliMode(int argc, char** argv)`
  Parses CLI arguments into `ExperimentOptions`, runs the engine, writes a concise human-readable summary.

- `int RunJsonMode(const std::string& input_text)`
  Parses JSON into `ExperimentOptions`, runs the engine, emits progress and final result JSON.

- `void EmitResult(const nlohmann::json& data)`
  Emits final JSON result in a stable envelope.

- `void EmitError(const std::string& message, const std::string& code)`
  Emits structured errors in JSON mode.

Input:

- CLI args or JSON request

Output:

- process exit code
- CLI text summary or JSON payload

## 2.2 `orchestration_layer::ExperimentEngine`

Purpose:

- coordinate the full experiment pipeline
- keep `main.cpp` thin
- call sampling, extraction, and analysis in sequence

The engine should assume that `dataset_dir` points to an already-built corpus-binaries directory. It should validate that the required files already exist before any experiment work begins.

Suggested methods:

- `ExperimentRunResult run(const ExperimentOptions& options, const ProgressEmitter* progress_emitter = 0) const;`
  Main orchestration method. Returns the complete experiment result and coordinates stage-level artefact writing.

- `void validate_options(const ExperimentOptions& options) const;`
  Verifies required paths, parameter consistency, and supported modes.

- `CorpusDataset open_dataset(const ExperimentOptions& options) const;`
  Opens binary files and metadata needed by downstream stages.

- `std::vector<ExperimentCondition> build_conditions(const ExperimentOptions& options, const CorpusDataset& dataset) const;`
  Expands high-level settings into explicit experimental conditions.

- `std::vector<ConditionRunResult> run_sampling_designs(const CorpusDataset& dataset, const std::vector<ExperimentCondition>& conditions, const ExperimentOptions& options, const ProgressEmitter* progress_emitter) const;`
  Groups conditions by sampling design, builds one shared sample set per design, and runs all attached conditions against that shared sample set.

- `ExperimentRunResult run_stepwise(const ExperimentOptions& options, const ProgressEmitter* progress_emitter = 0) const;`
  Dispatches to the requested stage-only mode.

- `void write_run_manifest(const ExperimentOptions& options, const std::vector<ExperimentCondition>& conditions) const;`
  Writes the whole-run manifest before sampling begins.

- `void write_final_comparison_artifact(const ExperimentRunResult& result) const;`
  Writes the final comparison table after all robust sets are complete.

- `ExperimentRunResult run_fun_mode(const ExperimentOptions& options, const ProgressEmitter* progress_emitter = 0) const;`
  Runs a lightweight validation pass using smaller defaults.

- `ExperimentRunResult run_sample_only(const CorpusDataset& dataset, const std::vector<ExperimentCondition>& conditions, const ExperimentOptions& options, const ProgressEmitter* progress_emitter = 0) const;`
  Writes shared sample artefacts for each sampling design and stops.

- `ExperimentRunResult run_extract_only(const CorpusDataset& dataset, const std::vector<ExperimentCondition>& conditions, const ExperimentOptions& options, const ProgressEmitter* progress_emitter = 0) const;`
  Loads existing sample artefacts for each sampling design, runs extraction, and writes aggregate artefacts.

- `ExperimentRunResult run_analyze_only(const CorpusDataset& dataset, const std::vector<ExperimentCondition>& conditions, const ExperimentOptions& options, const ProgressEmitter* progress_emitter = 0) const;`
  Loads existing aggregate artefacts, builds robust sets, and writes final analysis artefacts.

Input:

- `ExperimentOptions`
- `CorpusDataset`

Output:

- `ExperimentRunResult`

## 2.3 `orchestration_layer::ProgressEmitter`

Purpose:

- match the existing `corpus_builder` style
- allow both silent and reporting modes

Suggested classes:

- `class ProgressEmitter`
  Interface with `virtual void emit(const std::string& message, int percent) const = 0;`

- `class NullProgressEmitter : public ProgressEmitter`
  No-op emitter for internal use.

- `class JsonProgressEmitter : public ProgressEmitter`
  Emits structured JSON progress messages from `main.cpp`.

Suggested helper method in engine:

- `void emit_stage_progress(const ProgressEmitter* emitter, const std::string& stage, const std::string& message, int percent) const;`

Progress should be emitted:

- at engine start
- after dataset open
- after condition expansion
- when each sampling design starts
- during sampling and extraction loops
- after robust set finalization
- before and after global comparison analysis

## 3. Shared types and options

## 3.1 `experiment_shared_layer::ExperimentOptions`

Purpose:

- hold all runtime settings in one place

Suggested fields:

- `std::string dataset_dir`
- `std::string output_dir`
- `std::string experiment_title`
- `std::string mode`
  Example values: `runExperiment`, `sampleOnly`, `extractOnly`, `analyzeOnly`, `inspectArtifacts`, `funRun`
- `std::vector<CorpusSizeConfig> size_configs`
- `std::vector<CompositionConfig> composition_configs`
- `std::vector<std::uint32_t> frequency_thresholds_pm`
- `std::vector<std::uint32_t> document_dispersion_thresholds`
- `std::uint32_t sample_count`
- `std::uint32_t bundle_size`
  Usually 4, but should not be hard-coded into the option type.
- `double coverage_target`
- `std::uint32_t random_seed`
- `bool run_conditional`
- `bool run_unconditional`
- `bool emit_sample_level_sets`
- `bool emit_intermediate_artifacts`
- `bool emit_debug_bundle_counts`
  Optional debug mode only; not recommended for normal runs.
- `bool emit_sample_summary_json`
- `std::string artifact_input_dir`
  Used when resuming from existing artefacts.

Suggested methods:

- `static ExperimentOptions FromJson(const nlohmann::json& input);`
- `static ExperimentOptions FromCli(int argc, char** argv);`
- `nlohmann::json ToJson() const;`

Input:

- CLI args or JSON

Output:

- validated runtime config object

Suggested supporting structs:

- `CorpusSizeConfig`
  - `std::uint32_t target_tokens`
  - `std::uint32_t delta_tokens`

- `CompositionConfig`
  - `std::string label`
  - `std::uint32_t min_tokens`
  - `std::uint32_t max_tokens`

## 3.2 `experiment_shared_layer::ExperimentTypes`

Purpose:

- define the data objects that move between stages

Suggested structs:

- `DocumentInfo`
  - `std::uint32_t document_id`
  - `std::uint32_t token_start`
  - `std::uint32_t token_end`
  - `std::uint32_t token_count`
  - `std::string composition_label`
  - `std::map<std::string, std::string> metadata`

- `ExperimentCondition`
  Represents one explicit condition.
  - `std::string condition_id`
  - `std::uint32_t corpus_size_tokens`
  - `std::uint32_t corpus_delta_tokens`
  - `std::string composition_mode`
  - `bool conditional_mode`
  - `std::uint32_t frequency_threshold_pm`
  - `std::uint32_t frequency_threshold_raw`
  - `std::uint32_t document_dispersion_threshold`
  - `std::uint32_t sample_count`
  - `double coverage_target`

- `SampleSummary`
  Human-readable summary for one sampling design.
  - `std::string sampling_design_id`
  - `std::uint32_t corpus_size_tokens`
  - `std::uint32_t corpus_delta_tokens`
  - `std::string composition_mode`
  - `std::uint32_t random_seed`
  - `std::uint32_t target_sample_count`
  - `std::uint32_t accepted_sample_count`
  - `double mean_sample_tokens`
  - `std::uint32_t min_sample_tokens`
  - `std::uint32_t max_sample_tokens`
  - `double mean_documents_per_sample`
  - `std::uint32_t min_documents_per_sample`
  - `std::uint32_t max_documents_per_sample`
  - `std::string sample_membership_artifact_path`

- `ArtifactInspectionResult`
  - `std::string artifact_type`
  - `std::string artifact_path`
  - `nlohmann::json summary`

- `SampledCorpus`
  One accepted corpus sample.
  - `std::uint32_t sample_index`
  - `std::vector<std::uint32_t> document_ids`
  - `std::uint32_t total_tokens`
  - `std::int32_t token_distance_from_target`
  - `bool accepted`

- `SampleBundleStats`
  Counts for one sample.
  - `std::vector<std::uint32_t> feature_counts`
  - `std::vector<std::uint16_t> feature_doc_ranges`

- `SampleExtractionResult`
  Output of one sample after conditional filtering.
  - `std::uint32_t sample_index`
  - `std::vector<std::uint32_t> extracted_feature_ids`
  - `std::vector<std::uint32_t> extracted_counts`

- `ConditionAggregate`
  Aggregated result over all samples in one condition.
  - `std::string condition_id`
  - `std::vector<double> expected_mass`
  - `std::vector<std::uint32_t> extracted_sample_counts`
  - `std::vector<double> normalized_mass`

- `RobustBundleSet`
  Final ranked set for one condition.
  - `std::string condition_id`
  - `std::vector<std::uint32_t> ranked_feature_ids`
  - `std::vector<double> ranked_weights`
  - `std::vector<double> cumulative_weights`
  - `std::vector<std::uint32_t> selected_feature_ids`

- `ConditionComparison`
  - `std::string left_condition_id`
  - `std::string right_condition_id`
  - `double jaccard`

- `ConditionRunResult`
  - `ExperimentCondition condition`
  - `ConditionAggregate aggregate`
  - `RobustBundleSet robust_set`
  - `std::vector<ConditionComparison> comparisons`
  - `double within_condition_stability`
  - `std::string aggregate_artifact_path`
  - `std::string robust_set_artifact_path`

- `ExperimentRunResult`
  - `ExperimentOptions options`
  - `std::vector<ConditionRunResult> condition_results`
  - `std::vector<ConditionComparison> global_comparisons`
  - `std::vector<SampleSummary> sample_summaries`

Data flow:

`ExperimentOptions -> ExperimentCondition -> SampledCorpus -> SampleBundleStats -> ConditionAggregate -> RobustBundleSet -> ExperimentRunResult`

## 4. Data access layer

## 4.1 `data_access_layer::CorpusDataset`

Purpose:

- hold opened readers and core corpus dimensions
- provide a stable access point for all stages

`CorpusDataset` is a reader over an already-built dataset. It should open existing binary artefacts and the existing `corpus.db`. It should not create base corpus files.

Suggested fields:

- `std::uint64_t num_docs`
- `std::uint64_t num_features_4gram`
- `std::vector<DocumentInfo> documents`
- `SparseMatrixReader fourgram_matrix`
- `LexiconReader word_lexicon`
- `LexiconReader fourgram_lexicon`
- `MetadataRepository metadata_repository`

Suggested methods:

- `static CorpusDataset Open(const std::string& dataset_dir);`
  Opens all required pre-existing artifacts from disk.

- `const DocumentInfo& document(std::uint32_t document_id) const;`
  Returns one document summary.

- `std::string render_bundle(std::uint32_t feature_id) const;`
  Converts a 4-gram ID into a human-readable string.

Input:

- dataset path

Output:

- ready-to-query in-memory handles

## 4.2 `data_access_layer::MetadataRepository`

Purpose:

- read `corpus.db`
- expose document-level filters and labels

Suggested methods:

- `void open(const std::string& sqlite_path);`
- `std::vector<DocumentInfo> load_documents(const std::vector<std::string>& metadata_keys) const;`
- `std::string classify_composition(std::uint32_t token_count) const;`
  Maps document length to `short`, `mid`, or `long`.
- `std::vector<std::uint32_t> filter_documents_by_composition(const std::vector<DocumentInfo>& documents, const std::string& composition_mode) const;`
- `std::vector<std::uint32_t> filter_documents_by_metadata(const std::vector<DocumentInfo>& documents, const std::map<std::string, std::string>& filters) const;`

- `nlohmann::json summarize_documents(const std::vector<std::uint32_t>& document_ids, const std::vector<DocumentInfo>& documents) const;`
  Produces compact inspection summaries for sample artefacts.

Output:

- vectors of document IDs or enriched `DocumentInfo`

## 4.3 `data_access_layer::SparseMatrixReader`

Purpose:

- read `4gram.spm.meta.bin`, `4gram.spm.offsets.bin`, `4gram.spm.entries.bin`
- expose document rows efficiently

Suggested methods:

- `void open(const std::string& meta_path, const std::string& offsets_path, const std::string& entries_path);`
- `std::uint64_t num_docs() const;`
- `std::uint64_t num_features() const;`
- `std::pair<const SparseEntry*, const SparseEntry*> row(std::uint32_t document_id) const;`
  Returns pointer range for one document row.
- `void accumulate_document(std::uint32_t document_id, std::vector<std::uint32_t>* counts, std::vector<std::uint16_t>* doc_ranges) const;`
  Adds one document row into sample accumulators.

Input:

- document ID

Output:

- sparse row or updated accumulators

## 4.4 `data_access_layer::LexiconReader`

Purpose:

- decode word and 4-gram lexicons

Suggested methods:

- `void open_word_lexicon(const std::string& path);`
- `void open_ngram_lexicon(const std::string& path, std::uint32_t n);`
- `std::string lookup_word(std::uint32_t word_id) const;`
- `std::vector<std::uint32_t> lookup_ngram_word_ids(std::uint32_t feature_id) const;`
- `std::string render_ngram(std::uint32_t feature_id, const LexiconReader& word_lexicon) const;`

## 5. Sampling layer

## 5.1 `sampling_layer::SampleConditionPlanner`

Purpose:

- expand high-level options into explicit conditions

Suggested methods:

- `std::vector<ExperimentCondition> build(const ExperimentOptions& options) const;`
  Creates all conditional and unconditional conditions.

- `std::uint32_t convert_pm_to_raw(std::uint32_t threshold_pm, std::uint32_t corpus_size_tokens) const;`
  Computes the raw threshold for one corpus size.

- `std::string make_sampling_design_id(const CorpusSizeConfig& size_config, const CompositionConfig& composition_config) const;`
  Creates a stable identifier for one size-plus-delta and composition pairing.

Input:

- `ExperimentOptions`

Output:

- fully materialized `ExperimentCondition` objects

## 5.2 `sampling_layer::DocumentPoolBuilder`

Purpose:

- build the eligible document pool for one condition

Suggested methods:

- `std::vector<std::uint32_t> build_pool(const CorpusDataset& dataset, const ExperimentCondition& condition) const;`
  Returns eligible document IDs.

- `bool matches_condition(const DocumentInfo& document, const ExperimentCondition& condition) const;`
  Checks composition and optional metadata constraints.

Input:

- `CorpusDataset`
- `ExperimentCondition`

Output:

- filtered document IDs

## 5.3 `sampling_layer::CorpusSampler`

Purpose:

- generate accepted sampled corpora for a condition
- enforce target token count and composition constraints

Suggested methods:

- `std::vector<SampledCorpus> build_samples(const std::vector<DocumentInfo>& documents, const std::vector<std::uint32_t>& pool_doc_ids, const ExperimentCondition& condition, std::uint32_t random_seed, const ProgressEmitter* progress_emitter = 0) const;`
  Produces `M` accepted samples.

- `SampledCorpus draw_one_sample(const std::vector<DocumentInfo>& documents, const std::vector<std::uint32_t>& pool_doc_ids, const ExperimentCondition& condition, std::mt19937* rng) const;`
  Draws one candidate sample.

- `bool is_accepted(const SampledCorpus& sample, const ExperimentCondition& condition) const;`
  Checks whether the final token count lies inside `[target - delta, target + delta]`.

- `std::uint32_t lower_token_bound(const ExperimentCondition& condition) const;`
  Returns `target - delta`.

- `std::uint32_t upper_token_bound(const ExperimentCondition& condition) const;`
  Returns `target + delta`.

- `bool try_add_document(std::uint32_t document_id, const std::vector<DocumentInfo>& documents, const ExperimentCondition& condition, SampledCorpus* sample) const;`
  Attempts to add a document without exceeding the upper bound.

- `void write_sample_membership_artifact(const ExperimentCondition& condition, const std::vector<SampledCorpus>& samples) const;`
  Writes one binary sample-membership artefact per sampling design after all accepted samples are built.

- `SampleSummary build_sample_summary(const ExperimentCondition& condition, const std::vector<SampledCorpus>& samples) const;`
  Computes compact descriptive statistics for app inspection.

- `void write_sample_summary_json(const SampleSummary& summary) const;`
  Writes one JSON sample manifest per sampling design.

- `std::vector<SampledCorpus> load_samples_from_artifact(const std::string& artifact_path) const;`
  Loads previously written sample-membership artefacts for resume or inspect flows.

- `std::vector<SampleSummary> load_sample_summaries_from_artifact(const std::string& artifact_dir) const;`
  Loads JSON sample manifests for inspection or resume flows.

Input:

- eligible document pool
- condition
- RNG seed

Output:

- `std::vector<SampledCorpus>`

Data change:

- document pool becomes repeated sampled sets of document IDs

Sampling rule:

1. keep the 5-try rule active throughout sample construction, not only near the end
2. repeatedly draw randomly from the eligible document pool
3. if adding the drawn document keeps the running total at or below `target + delta`, accept the document into the sample and reset the remaining retry counter back to 5
4. if adding the drawn document would take the sample above `target + delta`, reject that document, leave the running total unchanged, and decrement the remaining retry counter
5. if the retry counter reaches 0, stop growing the sample
6. accept the sample only if the final total lies within `[target - delta, target + delta]`
7. if the final total is still below `target - delta`, reject the sample corpus and start a new one

Example:

- for a `100000 +/- 5000` design, a running total of `95000` is already acceptable
- from `95000`, the sampler may continue trying to reach a higher acceptable total such as `103000` or `105000`
- if the next five candidate documents all overshoot `105000`, the sampler stops and accepts the `95000` sample
- if the running total were `94000` and five consecutive candidate documents all overshot `105000`, the sampler would stop and reject that sample because it never reached the acceptable lower bound

This is a practical compromise rather than a mathematically exact packing rule. It prevents very small deltas from making sampling unreasonably slow while still keeping samples close to the requested corpus size.

Progress should be emitted:

- when a sampling design starts
- after every accepted sample or every fixed batch of accepted samples
- when repeated near-target retries are happening for one sample
- when the sample membership artefact is written
- when the sample summary JSON is written

## 6. Extraction layer

## 6.1 `extraction_layer::BundleCounter`

Purpose:

- convert one `SampledCorpus` into per-feature counts and document ranges
- use the sparse matrix efficiently

Suggested methods:

- `SampleBundleStats count_sample(const CorpusDataset& dataset, const SampledCorpus& sample, std::uint32_t bundle_size) const;`
  Returns per-feature frequency and document range for one sample.

- `void accumulate_sample(const CorpusDataset& dataset, const SampledCorpus& sample, std::vector<std::uint32_t>* feature_counts, std::vector<std::uint16_t>* feature_doc_ranges) const;`
  Low-level counting routine.

- `void write_debug_bundle_counts(const ExperimentCondition& condition, const SampledCorpus& sample, const SampleBundleStats& stats) const;`
  Optional debug dump of sample-level bundle counts. Not recommended for default runs.

Input:

- `SampledCorpus`
- `CorpusDataset`

Output:

- `SampleBundleStats`

Data change:

- sampled document IDs become dense count vectors over all 4-gram features

Progress should be emitted:

- during long counting passes if a sample is unusually large
- when optional debug bundle-count artefacts are written

## 6.2 `extraction_layer::ThresholdPolicy`

Purpose:

- centralize all sample-level extraction rules

Suggested methods:

- `bool passes_conditional_rule(std::uint32_t raw_count, std::uint16_t document_range, const ExperimentCondition& condition) const;`
  Applies frequency and document-dispersion thresholds.

- `std::uint32_t raw_frequency_cutoff(const ExperimentCondition& condition) const;`
  Returns `condition.frequency_threshold_raw`.

Input:

- one feature's sample-level stats
- one condition

Output:

- pass/fail decision

## 6.3 `extraction_layer::ConditionalExtractor`

Purpose:

- perform Approach 1 aggregation

Suggested methods:

- `ConditionAggregate run(const CorpusDataset& dataset, const ExperimentCondition& condition, const std::vector<SampledCorpus>& samples, const ProgressEmitter* progress_emitter = 0) const;`
  Runs counting and thresholded aggregation over all samples.

- `SampleExtractionResult extract_sample(const SampleBundleStats& stats, const ExperimentCondition& condition, const ThresholdPolicy& policy) const;`
  Produces extracted feature IDs for one sample.

- `void update_aggregate(const SampleBundleStats& stats, const SampleExtractionResult& extracted, ConditionAggregate* aggregate) const;`
  Adds one sample's contribution into the condition aggregate.

- `void write_aggregate_artifact(const ExperimentCondition& condition, const ConditionAggregate& aggregate) const;`
  Writes the condition aggregate after all samples for that condition have been processed.

- `ConditionAggregate load_aggregate_artifact(const std::string& artifact_path) const;`
  Loads a previously written unconditional aggregate for `analyzeOnly` mode.

- `void write_sample_level_sets_debug(const ExperimentCondition& condition, const std::vector<SampleExtractionResult>& sample_results) const;`
  Optional debug artifact only. Not recommended for normal runs.

- `ConditionAggregate load_aggregate_artifact(const std::string& artifact_path) const;`
  Loads a previously written conditional aggregate for `analyzeOnly` mode.

Output:

- `ConditionAggregate`

Data change:

- per-sample vectors become expected conditional mass and extraction counts across samples

Progress should be emitted:

- when conditional extraction starts for a sampling design
- at regular sample-processing intervals
- when the aggregate artefact is written

## 6.4 `extraction_layer::UnconditionalExtractor`

Purpose:

- perform Approach 2 aggregation without sample-level filtering

Suggested methods:

- `ConditionAggregate run(const CorpusDataset& dataset, const ExperimentCondition& condition, const std::vector<SampledCorpus>& samples, const ProgressEmitter* progress_emitter = 0) const;`
  Runs counting and unconditional aggregation over all samples.

- `void update_aggregate(const SampleBundleStats& stats, ConditionAggregate* aggregate) const;`
  Adds raw sample counts directly into expected mass.

- `void write_aggregate_artifact(const ExperimentCondition& condition, const ConditionAggregate& aggregate) const;`
  Writes the condition aggregate after all samples for that condition have been processed.

Output:

- `ConditionAggregate`

Data change:

- per-sample vectors become unconditional expected frequency mass across samples

Progress should be emitted:

- when unconditional extraction starts for a sampling design
- at regular sample-processing intervals
- when the aggregate artefact is written

## 7. Analysis layer

## 7.1 `analysis_layer::RobustSetBuilder`

Purpose:

- convert aggregated masses into ranked bundle sets

Suggested methods:

- `RobustBundleSet build(const ConditionAggregate& aggregate, const ExperimentCondition& condition) const;`
  Sorts features by normalized mass and selects the smallest set reaching the coverage target.

- `std::vector<double> normalize_mass(const std::vector<double>& expected_mass) const;`
- `std::vector<std::uint32_t> rank_features(const std::vector<double>& normalized_mass) const;`
- `std::vector<std::uint32_t> select_until_coverage(const std::vector<std::uint32_t>& ranked_feature_ids, const std::vector<double>& normalized_mass, double coverage_target, std::vector<double>* cumulative_weights) const;`

- `void write_robust_set_artifact(const ExperimentCondition& condition, const RobustBundleSet& robust_set, const CorpusDataset& dataset) const;`
  Writes the final robust-set artifact with readable top bundles and selected feature IDs.

- `RobustBundleSet load_robust_set_artifact(const std::string& artifact_path) const;`
  Loads a previously written robust-set artifact for inspection or comparison reuse.

Input:

- `ConditionAggregate`
- `ExperimentCondition`

Output:

- `RobustBundleSet`

Progress should be emitted:

- when robust-set construction starts
- when robust-set artefact writing completes

## 7.2 `analysis_layer::StabilityAnalyzer`

Purpose:

- estimate within-condition stability

Suggested methods:

- `double compute_mean_pairwise_jaccard(const std::vector<SampleExtractionResult>& sample_sets) const;`
  Used mainly for conditional mode.

- `double compute_rank_stability(const std::vector<SampleBundleStats>& sample_stats, std::size_t top_k) const;`
  Optional fallback for unconditional mode.

Input:

- sample-level extracted sets or counts

Output:

- one scalar stability summary per condition

## 7.3 `analysis_layer::ComparisonAnalyzer`

Purpose:

- compare robust bundle sets across conditions

Suggested methods:

- `std::vector<ConditionComparison> compare_all(const std::vector<RobustBundleSet>& robust_sets) const;`
  Computes all pairwise comparisons.

- `ConditionComparison compare_one(const RobustBundleSet& left, const RobustBundleSet& right) const;`
  Computes one Jaccard score.

- `double jaccard(const std::vector<std::uint32_t>& left_feature_ids, const std::vector<std::uint32_t>& right_feature_ids) const;`

Input:

- robust sets from all conditions

Output:

- pairwise comparison table

Progress should be emitted:

- when pairwise comparison analysis starts
- at regular comparison batches for large condition grids
- when the final comparison artifact is written

## 7.4 `analysis_layer::ResultFormatter`

Purpose:

- prepare compact human-readable and JSON-ready summaries

Suggested methods:

- `nlohmann::json to_json(const ExperimentRunResult& result, const CorpusDataset& dataset) const;`
- `std::string to_cli_summary(const ExperimentRunResult& result, const CorpusDataset& dataset) const;`
- `std::vector<nlohmann::json> render_top_bundles(const RobustBundleSet& robust_set, const CorpusDataset& dataset, std::size_t limit) const;`
- `nlohmann::json render_sample_summary(const SampleSummary& summary) const;`
  Emits the app-facing JSON form of a sampling-design summary.

Output:

- final result payloads with readable bundle strings

## 7.5 `analysis_layer::ArtifactInspector`

Purpose:

- inspect previously written artefacts without rerunning the experiment
- support app workflows that browse sampling and analysis outputs

Suggested methods:

- `ArtifactInspectionResult inspect_run_manifest(const std::string& path) const;`
- `ArtifactInspectionResult inspect_sample_summary(const std::string& path) const;`
- `ArtifactInspectionResult inspect_sample_membership(const std::string& path, const CorpusDataset& dataset, std::size_t sample_limit) const;`
- `ArtifactInspectionResult inspect_condition_aggregate(const std::string& path, std::size_t top_k) const;`
- `ArtifactInspectionResult inspect_robust_set(const std::string& path) const;`
- `ArtifactInspectionResult inspect_final_comparison(const std::string& path) const;`

Output:

- compact JSON summaries suitable for the app

## 8. Recommended execution flow

For one full run:

1. `main.cpp` parses CLI or JSON into `ExperimentOptions`.
2. `ExperimentEngine::validate_options()` checks the request.
3. `ExperimentEngine::write_run_manifest()` writes the run manifest.
4. `CorpusDataset::Open()` opens the dataset.
5. `SampleConditionPlanner::build()` expands all experimental conditions.
6. For each sampling design:
   `DocumentPoolBuilder::build_pool()` returns eligible document IDs.
7. `CorpusSampler::build_samples()` generates accepted sampled corpora using the size-specific delta rule.
8. `CorpusSampler::write_sample_membership_artifact()` writes one sample-membership artefact per sampling design.
9. `CorpusSampler::build_sample_summary()` and `write_sample_summary_json()` emit inspection artefacts.
10. For each conditional condition under that sampling design:
    `ConditionalExtractor::run()` returns a `ConditionAggregate`.
11. `ConditionalExtractor::write_aggregate_artifact()` writes one aggregate artefact per conditional condition.
12. For each unconditional condition under that sampling design:
    `UnconditionalExtractor::run()` returns a `ConditionAggregate`.
13. `UnconditionalExtractor::write_aggregate_artifact()` writes one aggregate artefact per unconditional condition.
14. For each finalized condition aggregate:
    `RobustSetBuilder::build()` constructs the final robust set.
15. `RobustSetBuilder::write_robust_set_artifact()` writes the robust-set artefact.
16. `StabilityAnalyzer` computes within-condition stability where applicable.
17. After all sampling designs are complete, `ComparisonAnalyzer::compare_all()` computes cross-condition Jaccard values.
18. `ExperimentEngine::write_final_comparison_artifact()` writes the final comparison artefact.
19. `ResultFormatter` emits CLI or JSON output.

For stepwise runs:

1. `sampleOnly`
   Writes run manifest, sample-membership artefacts, and sample summary JSON, then stops.
2. `extractOnly`
   Loads existing sample-membership artefacts, groups conditions by sampling design, writes condition aggregate artefacts, then stops.
3. `analyzeOnly`
   Loads aggregate artefacts, writes robust-set artefacts and final comparison artefact.
4. `inspectArtifacts`
   Uses `ArtifactInspector` to return JSON summaries for existing artefacts.
5. `funRun`
   Executes the same pipeline structure with smaller defaults, for example fewer samples and fewer output records.

## 9. Efficiency choices

The plan should be implemented around these efficiency decisions:

- use the `4gram.spm.*` files as the primary counting structure
- treat each sampled corpus as a set of document IDs, not as reconstructed text
- aggregate by integer addition over sparse document rows
- maintain dense accumulator vectors for feature counts and document ranges inside each sample run
- convert to readable bundle strings only at the final reporting stage
- keep sample-level extracted sets only when needed for stability analysis or debugging

This keeps the expensive work in tight numeric loops and delays string reconstruction until the end.