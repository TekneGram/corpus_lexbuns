# Experiment Shared Layer

## Purpose

The experiment shared layer defines the common types, option parsing, and JSON serialization used across all other layers.

It is responsible for:
- shared structs such as `SamplingDesign`, `ExperimentCondition`, `ConditionAggregate`, and `ExperimentRunResult`
- run mode parsing
- request/response JSON serialization
- option validation
- stable identifiers such as sampling-design IDs and condition IDs

Key files:
- `experiment_types.hpp`
- `experiment_options.cpp`
- `json_io.cpp`

## Artifacts Emitted At This Layer

This layer does not emit experiment artifacts directly.

Instead, it defines the serialized shapes used by artifacts emitted in other layers, including:
- run manifests
- sample summaries
- condition aggregates
- robust bundle sets
- comparison results
- inspection results

## Purpose Of This Layer's JSON Support

The JSON support in this layer exists so that:
- artifacts can be written with stable schemas
- the frontend can consume consistent payloads
- inspection and stepwise execution can reload prior state safely

