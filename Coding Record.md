# Coding this repository

## Roles

There is one planner/integrator agent and several layer-specific worker agents.

The planner/integrator agent (you) is responsible for:

- reading the planning documents
- implementing the shared interfaces first
- freezing shared contracts before delegation
- assigning workers to disjoint write scopes
- reviewing and integrating worker output
- resolving interface conflicts

You must spawn worker agents. Worker agents are responsible only for their assigned layer. Workers must not redefine shared interfaces or edit files outside their write scope unless explicitly instructed by the planner.

## Required reading for the planner

Read:

- `/planning/01_lexical_bundle_extraction.md`
- `/planning/02_data_available.md`
- `/planning/03_code_plan.md`

If anything remains unclear, inspect `../corpus_builder/` to understand the existing corpus-binaries format and metadata layout.

## Phase order

Implement in this order:

1. `experiment_shared_layer`
2. `data_access_layer`
3. `sampling_layer`
4. `extraction_layer`
5. `analysis_layer`
6. `orchestration_layer` and `main.cpp`

Do not start later phases until the planner has stabilized the interfaces needed by them.

## Planner ownership

The planner must implement and freeze:

### 1. `experiment_shared_layer`

Responsible for:

- `ExperimentOptions`
- JSON/CLI config parsing
- shared structs such as:
  - `ExperimentCondition`
  - `SampledCorpus`
  - `SampleBundleStats`
  - `ConditionAggregate`
  - `RobustBundleSet`
  - experiment/artifact metadata types

### 2. `data_access_layer`

Responsible for:

- `CorpusDataset`
- `MetadataRepository`
- `SparseMatrixReader`
- `LexiconReader`

These two layers define the interfaces the other layers depend on.

Before spawning workers, the planner must ensure:

- type names are settled
- method signatures are stable
- artifact naming/path conventions are clear
- resume/load contracts are clear

## Worker rules

For every worker:

- read only the planning docs and the specific shared interfaces needed
- write only inside the assigned layer
- do not modify `experiment_shared_layer` or `data_access_layer`
- do not change method signatures defined by the planner
- if a shared contract seems insufficient, report it back rather than changing it directly
- do not revert edits made by other workers

## Sampling worker

### Write scope

- `sampling_layer/*`

### Read context

- planning docs
- shared-layer headers
- data-access-layer headers

### Responsibilities

Implement:

- `SampleConditionPlanner`
- `DocumentPoolBuilder`
- `CorpusSampler`

Must support:

- sampling-design-first execution
- size-specific deltas
- heuristic 5-try sampling rule
- sample membership artefacts
- sample summary JSON
- sample artefact loading for resume flows

## Extraction worker

### Write scope

- `extraction_layer/*`

### Read context

- planning docs
- shared-layer headers
- data-access-layer headers
- sampling-layer public headers if needed for sample artefact loading contracts

### Responsibilities

Implement:

- `BundleCounter`
- `ThresholdPolicy`
- `ConditionalExtractor`
- `UnconditionalExtractor`

Must support:

- shared sample reuse within a sampling design
- aggregate artefact writing
- aggregate artefact loading for `analyzeOnly`
- optional debug outputs without enabling them by default

## Analysis worker

### Write scope

- `analysis_layer/*`

### Read context

- planning docs
- shared-layer headers
- data-access-layer headers

### Responsibilities

Implement:

- `RobustSetBuilder`
- `ComparisonAnalyzer`
- `StabilityAnalyzer`
- `ResultFormatter`
- `ArtifactInspector`

Must support:

- robust-set artefacts
- final comparison artefact
- artifact inspection JSON for the app
- loading prior robust-set artefacts when needed

## Orchestration worker

### Write scope

- `orchestration_layer/*`
- `main.cpp`

### Read context

- all public headers from completed lower layers
- planning docs

### Responsibilities

Implement:

- `ExperimentEngine`
- progress emitters
- CLI mode
- JSON mode
- run modes:
  - `runExperiment`
  - `sampleOnly`
  - `extractOnly`
  - `analyzeOnly`
  - `inspectArtifacts`
  - `funRun`

This layer should be implemented last, after lower-layer interfaces are stable.

## Integration rules

The planner/integrator should:

- review each worker result before merging
- ensure artifact names and paths match the plan
- ensure sampling-design-first orchestration is preserved
- ensure no worker bypasses the experiment registry / artifact model
- make final cross-layer fixes centrally
