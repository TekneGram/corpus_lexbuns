#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p build

g++ -std=c++11 -Wall -Wextra -pedantic -I. \
  -c experiment_shared_layer/experiment_options.cpp -o build/experiment_options.o
g++ -std=c++11 -Wall -Wextra -pedantic -I. \
  -c experiment_shared_layer/json_io.cpp -o build/json_io.o
g++ -std=c++11 -Wall -Wextra -pedantic -I. \
  -c data_access_layer/sparse_matrix_reader.cpp -o build/sparse_matrix_reader.o
g++ -std=c++11 -Wall -Wextra -pedantic -I. \
  -c data_access_layer/lexicon_reader.cpp -o build/lexicon_reader.o
g++ -std=c++11 -Wall -Wextra -pedantic -I. \
  -c data_access_layer/metadata_repository.cpp -o build/metadata_repository.o
g++ -std=c++11 -Wall -Wextra -pedantic -I. \
  -c data_access_layer/corpus_dataset.cpp -o build/corpus_dataset.o
g++ -std=c++11 -Wall -Wextra -pedantic -I. \
  -c sampling_layer/sample_condition_planner.cpp -o build/sample_condition_planner.o
g++ -std=c++11 -Wall -Wextra -pedantic -I. \
  -c sampling_layer/document_pool_builder.cpp -o build/document_pool_builder.o
g++ -std=c++11 -Wall -Wextra -pedantic -I. \
  -c sampling_layer/corpus_sampler.cpp -o build/corpus_sampler.o

g++ -std=c++11 -Wall -Wextra -pedantic -I. \
  main.cpp \
  experiment_shared_layer/experiment_options.cpp \
  experiment_shared_layer/json_io.cpp \
  data_access_layer/sparse_matrix_reader.cpp \
  data_access_layer/lexicon_reader.cpp \
  data_access_layer/metadata_repository.cpp \
  data_access_layer/corpus_dataset.cpp \
  sampling_layer/sample_condition_planner.cpp \
  sampling_layer/document_pool_builder.cpp \
  sampling_layer/corpus_sampler.cpp \
  extraction_layer/bundle_counter.cpp \
  extraction_layer/threshold_policy.cpp \
  extraction_layer/conditional_extractor.cpp \
  extraction_layer/unconditional_extractor.cpp \
  analysis_layer/robust_set_builder.cpp \
  analysis_layer/stability_analyzer.cpp \
  analysis_layer/comparison_analyzer.cpp \
  analysis_layer/result_formatter.cpp \
  analysis_layer/artifact_inspector.cpp \
  orchestration_layer/progress_emitter.cpp \
  orchestration_layer/experiment_engine.cpp \
  -lsqlite3 \
  -o build/corpus_lexbuns_pipeline

echo "Build passed."
