#include "artifact_inspector.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "result_formatter.hpp"

namespace teknegram {

namespace {

bool FileExists(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    return static_cast<bool>(input);
}

} // namespace

ArtifactInspector::ArtifactInspector() {}

ArtifactInspectionResult ArtifactInspector::inspect(const std::string& artifact_path) const {
    const nlohmann::json payload = parse_payload(artifact_path);
    ArtifactInspectionResult result;
    result.artifact_path = artifact_path;
    result.artifact_type = payload.value("artifactType", "unknown");
    result.summary_json = summarize_json(payload);
    return result;
}

nlohmann::json ArtifactInspector::inspect_json(const std::string& artifact_path) const {
    return ResultFormatter::artifact_inspection_json(inspect(artifact_path));
}

std::vector<ArtifactInspectionResult> ArtifactInspector::inspect_many(const std::vector<std::string>& artifact_paths) const {
    std::vector<ArtifactInspectionResult> results;
    for (std::size_t i = 0; i < artifact_paths.size(); ++i) {
        results.push_back(inspect(artifact_paths[i]));
    }
    return results;
}

std::string ArtifactInspector::summarize_json(const nlohmann::json& payload) {
    return payload.dump(2);
}

nlohmann::json ArtifactInspector::parse_payload(const std::string& artifact_path) {
    if (!FileExists(artifact_path)) {
        throw std::runtime_error("Artifact not found: " + artifact_path);
    }

    std::ifstream input(artifact_path.c_str(), std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open artifact: " + artifact_path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(buffer.str());
    } catch (const std::exception&) {
        throw std::runtime_error("Failed to parse artifact JSON: " + artifact_path);
    }
    return payload;
}

} // namespace teknegram
