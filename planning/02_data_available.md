# Data Available for the Lexical Bundle Experiment

This document summarizes the data already available for the lexical bundle experiment and the shape of that data on disk. It is intended to be self-contained, so that a future implementation can understand the corpus structure without needing to re-read the corpus-builder planning documents or inspect an example dataset.

The corpus is stored as a binary analysis dataset plus a SQLite metadata database. The binary layer is optimized for fast counting and Monte Carlo resampling. The SQLite layer stores document metadata and grouping information.

## 1. Top-level data model

The corpus is represented as one contiguous global token stream of length $N$ tokens, plus document boundaries over that stream.

The key dimensions are:

- $N$: total number of tokens in the corpus
- $D$: total number of documents in the corpus
- $F$: total number of features in a feature family, for example words, lemmas, 2-grams, 3-grams, or 4-grams

All token-level binary arrays are aligned by global token position $i$, where $0 \le i < N$.

At each token position $i$, the following aligned values are available:

- `word[i]`: word ID
- `lemma[i]`: lemma ID
- `pos[i]`: POS ID
- `head[i]`: dependency-head token position
- `deprel[i]`: dependency-relation ID
- `word_doc[i]`: document ID containing token $i$

This means the token layer is columnar: each linguistic attribute is stored in its own file, but all files share the same token indexing.

## 2. Core token-layer files

These files define the primary corpus stream.

| File | Element type | Shape | Meaning |
| --- | --- | --- | --- |
| `word.bin` | `uint32` | length $N$ | word ID per token |
| `lemma.bin` | `uint32` | length $N$ | lemma ID per token |
| `pos.bin` | `uint8` | length $N$ | POS ID per token |
| `head.bin` | `uint32` | length $N$ | global head-token position per token |
| `deprel.bin` | `uint8` | length $N$ | dependency-relation ID per token |
| `word_doc.bin` | `uint32` | length $N$ | document ID per token |

Interpretation at token position $i$:

$$
(\text{word}[i], \text{lemma}[i], \text{pos}[i], \text{head}[i], \text{deprel}[i], \text{word\_doc}[i])
$$

For lexical bundle work, the most directly relevant files are `word.bin`, `pos.bin`, and `word_doc.bin`.

## 3. Structural files

These files define sentence and document boundaries over the token stream.

| File | Element type | Shape | Meaning |
| --- | --- | --- | --- |
| `sentence_bounds.bin` | `uint32` | variable length | token positions at which sentences begin |
| `doc_ranges.bin` | `uint32` pairs | length $D$ pairs | start and end token positions for each document |

`doc_ranges.bin` should be interpreted as one row per document:

$$
\text{doc\_ranges}[d] = (\text{start}_d, \text{end}_d)
$$

where document $d$ occupies token positions from $\text{start}_d$ to $\text{end}_d$ inclusive or by implementation convention start/end boundary. The important point for the experiment is that every document has a recoverable token span in the global token stream.

These structural files support:

- building random samples as sets of documents
- summing document token counts to a target corpus size
- preventing n-grams from crossing sentence boundaries
- mapping bundle hits back to documents

## 4. Dictionary files

The token-layer IDs are integer-coded. These lexicon files map IDs back to strings.

| File | Maps | Purpose |
| --- | --- | --- |
| `word.lexicon.bin` | `word_id -> word string` | reconstruct word forms |
| `lemma.lexicon.bin` | `lemma_id -> lemma string` | reconstruct lemmas |
| `pos.lexicon.bin` | `pos_id -> POS string` | interpret POS codes |
| `deprel.lexicon.bin` | `deprel_id -> dependency label` | interpret dependency labels |

For the bundle experiment, `word.lexicon.bin` is required if extracted n-gram IDs need to be turned into readable lexical bundles in the final output.

## 5. N-gram feature families

The corpus builder already materializes 2-gram, 3-gram, and 4-gram feature families. These are contiguous word n-grams built by sliding window over the token stream without crossing sentence boundaries.

The bundle IDs are word-based, with aligned POS sidecars.

For each $n \in \{2,3,4\}$, the following files exist:

| File family | Shape | Meaning |
| --- | --- | --- |
| `${n}gram.lexicon.bin` | length $F_n$ | bundle ID to tuple of word IDs |
| `${n}gram.pos.lexicon.bin` | length $F_n$ | bundle ID to tuple of POS IDs |
| `${n}gram.freq.bin` | length $F_n$ | global corpus frequency of each n-gram |
| `${n}gram.index.header` | length $F_n$ header entries | bundle ID to posting-list location |
| `${n}gram.index.positions` | variable length | flattened start-token positions for occurrences |
| `${n}gram.docfreq.header` | length $F_n$ header entries | bundle ID to doc-frequency posting location |
| `${n}gram.docfreq.entries` | variable length | flattened `(doc_id, count)` entries |
| `${n}gram.spm.meta.bin` | fixed 64 bytes | sparse-matrix metadata |
| `${n}gram.spm.offsets.bin` | length $D+1$ | document row offsets |
| `${n}gram.spm.entries.bin` | length `NNZ_n` | sparse `(feature_id, count)` entries |

For example, a 4-gram feature ID maps to:

$$
\text{4gram\_id} \to (w_1, w_2, w_3, w_4)
$$

with a parallel POS tuple:

$$
\text{4gram\_id} \to (p_1, p_2, p_3, p_4)
$$

This is the most important feature family for the current lexical bundle experiment, although the same data shape also exists for 2-grams and 3-grams.

## 6. Posting indexes for fast occurrence lookup

For search-style access, the dataset includes inverted indexes. These are useful when the experiment needs exact token positions of a known feature.

Relevant file families include:

- `word.index.header` and `word.index.positions`
- `lemma.index.header` and `lemma.index.positions`
- `pos.index.header` and `pos.index.positions`
- `dep.index.header` and `dep.index.positions`
- `${n}gram.index.header` and `${n}gram.index.positions` for $n=2,3,4$

The general shape is:

$$
\text{feature\_id} \to [\text{sorted token positions}]
$$

For n-grams, the stored positions are the start-token positions of each occurrence.

These indexes are useful for:

- concordance reconstruction
- finding all occurrences of one bundle
- recovering which documents contain a bundle by combining positions with `word_doc.bin`

However, for repeated Monte Carlo analysis they are not the fastest primary structure. For that task, the document-frequency and sparse-matrix layers are more important.

## 7. Document-frequency layer

Each major feature family also has a doc-frequency representation storing feature counts by document. This is one of the key layers for the lexical bundle experiment because it avoids rescanning the token stream for every random sample.

Available families include:

- `word.docfreq.header`, `word.docfreq.entries`
- `lemma.docfreq.header`, `lemma.docfreq.entries`
- `2gram.docfreq.header`, `2gram.docfreq.entries`
- `3gram.docfreq.header`, `3gram.docfreq.entries`
- `4gram.docfreq.header`, `4gram.docfreq.entries`

The conceptual mapping is:

$$
\text{feature\_id} \to [(\text{doc\_id}, \text{count}), (\text{doc\_id}, \text{count}), \dots]
$$

For the lexical bundle experiment, the most relevant family is:

$$
\text{4gram\_id} \to [(\text{doc\_id}, \text{count}), \dots]
$$

This supports:

- computing the total frequency of a bundle in a sampled corpus
- computing document dispersion for a bundle in a sampled corpus
- aggregating counts over a selected set of documents

If a random sample consists of a set of document IDs $\mathcal{D}$, then the sample frequency of bundle $b$ can be computed as:

$$
F_b(S) = \sum_{(d,c)\in \text{DocFreq}(b)} c \cdot 1\{d \in \mathcal{D}\}
$$

and document range as:

$$
R_b^{text}(S) = \sum_{(d,c)\in \text{DocFreq}(b)} 1\{d \in \mathcal{D} \land c > 0\}
$$

This layer alone is sufficient for many threshold-based bundle-counting tasks.

## 8. Sparse matrix layer

The sparse matrix layer is the main scientific-computing representation for large-scale repeated sampling. It stores the same information as the doc-frequency layer, but in document-first order rather than feature-first order.

For each feature family there are three files:

- `.spm.meta.bin`
- `.spm.offsets.bin`
- `.spm.entries.bin`

The most relevant family for this experiment is the 4-gram sparse matrix:

- `4gram.spm.meta.bin`
- `4gram.spm.offsets.bin`
- `4gram.spm.entries.bin`

### 8.1 Sparse-matrix metadata

Each `.spm.meta.bin` file is a fixed 64-byte header with:

- magic number
- format version
- `num_docs = D`
- `num_features = F`
- `num_nonzero = NNZ`
- `entry_size_bytes = 8`

So the matrix dimensions are explicit on disk.

### 8.2 Sparse-matrix row offsets

Each `.spm.offsets.bin` file is an array of `uint64` with length $D+1$.

Interpretation:

$$
\text{entries for document } d = \text{entries}[\text{offsets}[d] \dots \text{offsets}[d+1]-1]
$$

Properties:

- `offsets[0] = 0`
- `offsets[D] = NNZ`
- rows are contiguous
- row access is $O(1)$

### 8.3 Sparse-matrix entries

Each `.spm.entries.bin` file is an array of structs:

$$
(\text{feature\_id}, \text{count})
$$

with each field stored as `uint32`, so each entry is 8 bytes.

Conceptually, the sparse matrix row for document $d$ is:

$$
\text{DocRow}(d) = [(\text{feature\_id}_1, c_1), (\text{feature\_id}_2, c_2), \dots]
$$

For the 4-gram experiment, this means each document can be accessed directly as a sparse bag of 4-gram counts.

This is the most efficient structure for repeated Monte Carlo runs:

1. sample a set of document IDs
2. read each sampled document row
3. add the row counts into an accumulator vector over 4-gram IDs
4. derive total bundle frequencies and document dispersion from the accumulated result

Time complexity per run is proportional to the total number of non-zero features in the sampled documents, not to the total number of features in the whole corpus.

## 9. SQLite metadata layer

The dataset also includes `corpus.db`, a SQLite database containing document metadata and grouping information.

The current schema includes these tables:

- `document`
- `folder_segment`
- `document_segment`
- `semantic_key`
- `semantic_value`
- `document_group`

### 9.1 Document table

`document` contains one row per document:

| Column | Type | Meaning |
| --- | --- | --- |
| `document_id` | integer primary key | document ID used by binary layers |
| `title` | text | document title |
| `author` | text | document author |
| `relative_path` | text | document path inside the corpus |

This table is the main metadata anchor for sampled documents.

### 9.2 Folder-segment tables

`folder_segment` and `document_segment` encode path-like corpus structure.

`folder_segment`:

- `segment_id`
- `segment_text`

`document_segment`:

- `document_id`
- `depth`
- `segment_id`

Together they allow documents to be grouped by path segments, which can be used as corpus partitions or subcorpus labels if needed.

### 9.3 Semantic grouping tables

The semantic grouping tables are:

- `semantic_key`
- `semantic_value`
- `document_group`

They encode document-level categorical labels.

Conceptually:

$$
\text{document\_id} \to (\text{key}, \text{value})
$$

for one or more metadata dimensions such as discipline, genre, or any custom grouping used by the corpus builder.

This layer is important because the lexical bundle experiment may need:

- discipline-balanced sampling
- subcorpus-aware sampling
- range or dispersion summaries by higher-level category

## 10. What data is directly usable for the experiment

For the lexical bundle experiment, the following data is already sufficient to run the full sampling and analysis workflow:

### Required for random corpus sampling

- `doc_ranges.bin` to know document token spans and document lengths
- `word_doc.bin` if token-to-document mapping is needed directly
- `corpus.db` tables to retrieve grouping or composition metadata for documents

### Required for 4-gram lexical bundle extraction

- `4gram.lexicon.bin` to map 4-gram IDs to word-ID tuples
- `4gram.pos.lexicon.bin` if POS side information is needed
- `word.lexicon.bin` to render bundle strings from word IDs

### Required for frequency and dispersion counting

One of the following two routes can be used:

1. `4gram.docfreq.*`
   This is feature-first and convenient when counting a restricted set of bundles.
2. `4gram.spm.*`
   This is document-first and best for repeated Monte Carlo sampling over many bundles.

### Optional but useful

- `4gram.index.*` for recovering exact occurrence positions
- `sentence_bounds.bin` if bundle reconstruction or validation at sentence boundaries is needed
- `lemma.*`, `word.*`, `2gram.*`, `3gram.*` if parallel experiments are later extended beyond 4-grams

## 11. Example dataset shape: `CheckTest`

The current working example dataset at `electron/bin/corpus-binaries/CheckTest` confirms that the above structures are materially present.

From the binary shapes visible in that dataset:

- the sparse-matrix offset files are 440 bytes, which corresponds to $(D+1)$ `uint64` values, so the example dataset has $D = 54$ documents
- `doc_ranges.bin` is 432 bytes, which corresponds to 54 document start/end pairs stored as `uint32`
- `word.bin`, `lemma.bin`, and `head.bin` are each about 2.5 MB, showing a token stream large enough for real bundle analysis
- `2gram`, `3gram`, and `4gram` families all include lexicon, frequency, posting-index, doc-frequency, and sparse-matrix files
- the SQLite database contains document and grouping tables needed for document-level sampling

The example dataset also contains semantic sidecar files:

- `semantic.key.lexicon.bin`
- `semantic.value.lexicon.bin`
- `semantic.doc_groups.offsets.bin`
- `semantic.doc_groups.entries.bin`
- `semantic.value_doc.header`
- `semantic.value_doc.entries`

These appear to provide binary encodings of document-group information parallel to the SQLite metadata. They are not strictly required to understand the main corpus shape for the lexical bundle experiment, but they indicate that document-level grouping metadata may also be available in binary form.

## 12. Practical conclusion

The experiment does not need to derive lexical bundles from raw text each time. The corpus builder has already produced the data structures needed for efficient repeated sampling and bundle analysis.

In practical terms, the available data already supports:

- selecting documents by metadata or composition class
- constructing random sampled corpora to a target token size
- counting 4-gram frequencies over sampled documents
- measuring document dispersion of each 4-gram within a sample
- repeating this process at large Monte Carlo scale
- mapping final bundle IDs back to readable bundle strings

The core representation for implementation should be treated as:

$$
\text{documents} \times \text{4-gram features}
$$

with document metadata in SQLite and count data available either as feature-first doc-frequency postings or as document-first sparse rows.
