#pragma once

#include <string>
#include <vector>

#include "../experiment_shared_layer/experiment_types.hpp"
#include "../experiment_shared_layer/json_io.hpp"

namespace teknegram {

class ArtifactInspector {
    public:
        ArtifactInspector();

        ArtifactInspectionResult inspect(const std::string& artifact_path) const;
        nlohmann::json inspect_json(const std::string& artifact_path) const;
        std::vector<ArtifactInspectionResult> inspect_many(const std::vector<std::string>& artifact_paths) const;

    private:
        static std::string summarize_json(const nlohmann::json& payload);
        static nlohmann::json parse_payload(const std::string& artifact_path);
};

} // namespace teknegram
