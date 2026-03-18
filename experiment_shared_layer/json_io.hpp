#pragma once

#include "experiment_types.hpp"
#include "../third_party/nlohmann_json.hpp"

namespace teknegram {

nlohmann::json ToJson(const CorpusSizeConfig& config);
nlohmann::json ToJson(const CompositionConfig& config);
nlohmann::json ToJson(const SamplingDesign& design);
nlohmann::json ToJson(const ExperimentCondition& condition);
nlohmann::json ToJson(const SampledCorpus& sample);
nlohmann::json ToJson(const SampleSummary& summary);
nlohmann::json ToJson(const FeatureMassStat& stat);
nlohmann::json ToJson(const ConditionAggregate& aggregate);
nlohmann::json ToJson(const RobustBundleSet& robust_set);
nlohmann::json ToJson(const ArtifactDescriptor& artifact);
nlohmann::json ToJson(const ComparisonScore& comparison);
nlohmann::json ToJson(const ExperimentManifest& manifest);
nlohmann::json ToJson(const ConditionRunResult& result);
nlohmann::json ToJson(const ExperimentRunResult& result);

CorpusSizeConfig CorpusSizeConfigFromJson(const nlohmann::json& value);
CompositionConfig CompositionConfigFromJson(const nlohmann::json& value);
ExperimentCondition ExperimentConditionFromJson(const nlohmann::json& value);
SampledCorpus SampledCorpusFromJson(const nlohmann::json& value);
SampleSummary SampleSummaryFromJson(const nlohmann::json& value);
ConditionAggregate ConditionAggregateFromJson(const nlohmann::json& value);
RobustBundleSet RobustBundleSetFromJson(const nlohmann::json& value);

} // namespace teknegram
