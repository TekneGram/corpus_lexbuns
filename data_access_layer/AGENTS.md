# Data Access Layer

## Purpose

The data access layer opens and validates the existing corpus dataset and exposes efficient access to metadata and sparse feature counts.

It is responsible for:
- opening `corpus.db`
- loading document metadata
- attaching document ranges from `doc_ranges.bin`
- opening the `n`-gram sparse matrix family for the requested bundle size
- validating required dataset files before a run begins

Key files:
- `corpus_dataset.cpp`
- `metadata_repository.cpp`
- `sparse_matrix_reader.cpp`

## Artifacts Emitted At This Layer

This layer does not emit experiment artifacts.

Its purpose is to read existing dataset artifacts produced elsewhere, including:
- `corpus.db`
- `doc_ranges.bin`
- `<n>gram.spm.meta.bin`
- `<n>gram.spm.offsets.bin`
- `<n>gram.spm.entries.bin`
- `<n>gram.lexicon.bin`

## Why This Layer Matters

All later artifacts depend on correct data access. If this layer loads the wrong sparse matrix family or mismatched metadata, the downstream sampling, extraction, and analysis artifacts will all be invalid.

