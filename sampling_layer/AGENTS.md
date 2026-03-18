# Sampling Layer

## Purpose

The sampling layer creates the accepted sampled corpora used by the experiment.

It is responsible for:
- expanding sampling designs from corpus-size and composition settings
- filtering the eligible document pool by composition
- repeatedly drawing document sets until an accepted token target is reached
- persisting sample membership data
- writing summary statistics for the accepted samples

Key files:
- `sample_condition_planner.cpp`
- `document_pool_builder.cpp`
- `corpus_sampler.cpp`

## Artifacts Emitted At This Layer

- `sample_membership_<sampling_design_id>.bin`
  - Purpose: stores the accepted sampled corpora for one sampling design
  - Used by:
    - `extractOnly`
    - `runExperiment`
    - `inspectSamplingDiagnostics`

- `sample_summary_<sampling_design_id>.json`
  - Purpose: provides a compact human-readable summary of the accepted samples for one sampling design
  - Typical contents:
    - corpus-size target
    - composition label
    - accepted sample count
    - token-count summary statistics
    - document-count summary statistics
    - paths to related artifacts

## Purpose Of The Sampling Artifacts

- The binary membership artifact is the canonical stage-1 output.
- The sample summary JSON is an inspection-oriented overview.

The sampling layer does not compute diagnostics beyond the sample summary. Richer diagnostics over the stage-1 output are handled in the analysis layer.

