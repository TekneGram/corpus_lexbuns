#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

#include "experiment_shared_layer/experiment_options.hpp"
#include "experiment_shared_layer/json_io.hpp"
#include "orchestration_layer/experiment_engine.hpp"
#include "orchestration_layer/progress_emitter.hpp"
#include "third_party/nlohmann_json.hpp"

namespace {

using json = nlohmann::json;

void EmitJson(const json& payload) {
    std::cout << payload.dump() << std::endl;
    std::cout.flush();
}

class JsonProgressEmitter : public teknegram::ProgressEmitter {
    public:
        virtual void emit(const std::string& message, int percent) const {
            EmitJson(json{
                {"type", "progress"},
                {"message", message},
                {"percent", percent}
            });
        }
};

bool HasNonWhitespace(const std::string& value) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') {
            return true;
        }
    }
    return false;
}

std::string ReadStdin() {
    return std::string(
        std::istreambuf_iterator<char>(std::cin),
        std::istreambuf_iterator<char>()
    );
}

void EmitResult(const json& data) {
    EmitJson(json{
        {"type", "result"},
        {"data", data}
    });
}

void EmitError(const std::string& message, const std::string& code) {
    EmitJson(json{
        {"type", "error"},
        {"code", code},
        {"message", message}
    });
}

std::string RequireString(const json& input, const char* key) {
    if (!input.contains(key) || !input[key].is_string()) {
        throw std::runtime_error(std::string("Missing or invalid ") + key);
    }
    return input[key].get<std::string>();
}

teknegram::ExperimentOptions ParseOptionsFromCli(int argc, char** argv) {
    teknegram::ExperimentOptions options;
    if (argc > 1) {
        options.dataset_dir = argv[1];
    }
    if (argc > 2) {
        options.mode = teknegram::ParseRunMode(argv[2]);
    }
    if (argc > 3) {
        options.experiment_title = argv[3];
    }
    return options;
}

int RunCliMode(int argc, char** argv) {
    teknegram::ExperimentOptions options = ParseOptionsFromCli(argc, argv);
    if (options.dataset_dir.empty()) {
        options.dataset_dir = ".";
    }
    if (options.experiment_title.empty()) {
        options.experiment_title = "Lexical bundle experiment";
    }

    teknegram::ExperimentEngine engine;
    teknegram::NullProgressEmitter progress_emitter;
    const teknegram::ExperimentRunResult result = engine.run(options, &progress_emitter);

    std::cout << "Experiment completed: " << result.experiment_code << "\n";
    std::cout << "Artifact dir: " << result.artifact_dir << "\n";
    std::cout << "Mode: " << teknegram::RunModeToString(result.run_mode) << "\n";
    return 0;
}

int RunJsonMode(const std::string& input_text) {
    json input_data;
    try {
        input_data = json::parse(input_text);
    } catch (const json::parse_error&) {
        EmitError("Invalid JSON input", "INVALID_JSON");
        return 1;
    }

    try {
        const std::string command = RequireString(input_data, "command");
        if (command != "runExperiment" &&
            command != "sampleOnly" &&
            command != "extractOnly" &&
            command != "analyzeOnly" &&
            command != "inspectSamplingDiagnostics" &&
            command != "inspectArtifacts" &&
            command != "funRun") {
            throw std::runtime_error("Unknown command: " + command);
        }

        if (!input_data.contains("mode")) {
            input_data["mode"] = command;
        }
        teknegram::ExperimentOptions options = teknegram::ExperimentOptions::FromJson(input_data);
        options.mode = teknegram::ParseRunMode(command);

        teknegram::ExperimentEngine engine;
        JsonProgressEmitter progress_emitter;
        const teknegram::ExperimentRunResult result = engine.run(options, &progress_emitter);

        EmitResult(teknegram::ToJson(result));
        return 0;
    } catch (const std::exception& ex) {
        EmitError(ex.what(), "COMMAND_FAILED");
        return 1;
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::string stdin_text = ReadStdin();

    try {
        if (HasNonWhitespace(stdin_text)) {
            return RunJsonMode(stdin_text);
        }
        return RunCliMode(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
