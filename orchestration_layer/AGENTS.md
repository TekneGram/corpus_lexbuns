# Orchestration Layer

## Purpose

The orchestration layer coordinates end-to-end experiment execution.

It is responsible for:
- validating options and selecting run modes
- opening the dataset
- expanding sampling designs and experiment conditions
- invoking the sampling, extraction, and analysis layers in the correct order
- writing the run manifest
- registering and locating experiment artifact directories
- returning high-level run results and progress events

Key entrypoint:
- `experiment_engine.cpp`

## Artifacts Emitted At This Layer

This layer emits or coordinates the emission of the top-level run artifacts.

Directly written here:
- `run_manifest.json`
  - Purpose: records the run metadata, serialized options, and expanded conditions for the experiment
- `final_comparisons.json`
  - Purpose: stores pairwise Jaccard comparisons across all finalized robust bundle sets

Indirectly coordinated here:
- sampling artifacts from the sampling layer
- aggregate artifacts from the extraction layer
- robust-set, stability, and diagnostics artifacts from the analysis layer

## Artifact Routing By Stage

- Stage 1 (`sampleOnly`)
  - emits sample membership and sample summary artifacts
- Stage 2 (`extractOnly`)
  - emits aggregate artifacts
- Stage 3 (`analyzeOnly`)
  - emits robust-set, stability, and comparison artifacts
- Sampling diagnostics (`inspectSamplingDiagnostics`)
  - emits `sampling_diagnostics_<sampling_design_id>.json`
- Generic inspection (`inspectArtifacts`)
  - reads JSON artifacts already written under the experiment artifact directory

