# Extraction Layer

## Purpose

The extraction layer counts bundle frequencies inside each accepted sample and accumulates condition-level extraction statistics.

It is responsible for:
- reading sparse-matrix rows for the sampled documents
- computing per-sample feature counts and document ranges
- applying the conditional extraction rule when needed
- accumulating condition-level mass across samples
- persisting aggregate artifacts for later analysis

Key files:
- `bundle_counter.cpp`
- `threshold_policy.cpp`
- `conditional_extractor.cpp`
- `unconditional_extractor.cpp`

## Artifacts Emitted At This Layer

- `aggregate_<condition_id>.bin`
  - Purpose: stores the accumulated stage-2 extraction totals for one condition
  - Contents:
    - `condition_id`
    - `sample_count`
    - `conditional_mode`
    - `accumulated_mass`
    - `extracted_sample_counts`
  - Used by:
    - `analyzeOnly`
    - `runExperiment`

Optional debug artifacts:
- `debug_bundle_counts_<condition_id>_sample_<sample_index>.json`
  - Purpose: stores full per-sample feature counts and document ranges for debugging
- `sample_sets_<condition_id>.json`
  - Purpose: stores conditional sample-level extracted sets for debugging

## Purpose Of The Aggregate Artifact

The aggregate artifact is the canonical stage-2 output. It compresses all repeated sample-level extraction results into a form that supports:
- expected-count estimation
- normalized-mass ranking
- robust-set construction
- later re-analysis without rerunning extraction

