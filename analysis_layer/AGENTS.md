# Analysis Layer

## Purpose

The analysis layer turns stage-1 and stage-2 outputs into inspection artifacts, robust sets, stability summaries, and cross-condition comparisons.

It is responsible for:
- building robust bundle sets from aggregate artifacts
- computing stability summaries
- computing pairwise Jaccard comparisons across finalized robust sets
- inspecting JSON artifacts
- generating sampling diagnostics from existing stage-1 sample artifacts

Key files:
- `robust_set_builder.cpp`
- `stability_analyzer.cpp`
- `comparison_analyzer.cpp`
- `artifact_inspector.cpp`
- `sampling_diagnostics_builder.cpp`
- `extraction_diagnostics_builder.cpp`

## Artifacts Emitted At This Layer

- `robust_set_<condition_id>.json`
  - Purpose: stores the ranked robust bundle set for one condition after coverage truncation

- `stability_<condition_id>.json`
  - Purpose: stores compact stability-oriented summaries for one condition

- `final_comparisons.json`
  - Purpose: stores pairwise Jaccard comparisons across all finalized robust sets

- `sampling_diagnostics_<sampling_design_id>.json`
  - Purpose: stores stage-1 diagnostics derived from the accepted sample membership artifact for one sampling design

- `extraction_diagnostics_<condition_id>.json`
  - Purpose: stores stage-2 diagnostics derived from an existing conditional aggregate artifact for one extraction condition

## Purpose Of The Sampling Diagnostics Artifact

The sampling diagnostics JSON exists to help the user evaluate whether stage-1 sampling behaved sensibly before moving on to extraction and final analysis.

The diagnostics are intended to answer questions such as:
- Are documents being included in samples at roughly similar rates?
- Are some documents strongly over-represented or under-represented?
- Is there visible length bias in the sampling process?
- How much overlap is there between sampled corpora?
- Are the accepted samples close to the intended token target and document-count profile?

These diagnostics are especially useful for:
- validating the sampling methodology
- spotting pathologies before expensive later-stage analysis
- giving the frontend a compact but information-rich inspection payload

## How To Answer The Diagnostics Questions

Use the fields in `sampling_diagnostics_<sampling_design_id>.json` to answer each stage-1 diagnostics question directly.

### 1. Are documents being included in samples at roughly similar rates?

Primary fields:
- `documentInclusionSummary.meanInclusionCount`
- `documentInclusionSummary.medianInclusionCount`
- `documentInclusionSummary.standardDeviation`
- `documentInclusionSummary.coefficientOfVariation`
- `documentInclusionHistogram`

How to interpret:
- If the mean and median are close, that suggests the inclusion distribution is fairly centered rather than heavily skewed.
- A smaller standard deviation and smaller coefficient of variation indicate more even inclusion.
- The histogram shows whether most documents cluster tightly around the center or whether the distribution is wide.

Using the example JSON:
- mean inclusion count is `595.23`
- median inclusion count is `594`
- standard deviation is `21.84`
- coefficient of variation is `0.0367`

That example suggests the inclusion counts are fairly tightly clustered around the center, which is consistent with reasonably balanced sampling.

### 2. Are some documents strongly over-represented or under-represented?

Primary fields:
- `documentInclusionSummary.minInclusionCount`
- `documentInclusionSummary.maxInclusionCount`
- `documentInclusionExtremes.leastIncludedDocuments`
- `documentInclusionExtremes.mostIncludedDocuments`

How to interpret:
- Compare the minimum and maximum to the mean.
- Inspect the explicit low and high extremes to see which documents are far from the center.
- If the extremes are only modestly different from the mean, imbalance is limited.
- If the extremes are very far from the mean, this may indicate bias or a structural effect in the sampling method.

Using the example JSON:
- mean inclusion count is `595.23`
- minimum is `521`
- maximum is `676`
- the least-included example document has `521`
- the most-included example document has `676`

These values show spread, but not extreme spread. The front end can flag a warning only if the tails are much farther from the center than expected for the design.

### 3. Is there visible length bias in the sampling process?

Primary fields:
- `lengthBiasSummary.pearsonCorrelationTokenCountVsInclusionCount`
- `lengthBiasSummary.lengthBins`
- `documentInclusionExtremes` together with document `tokenCount`

How to interpret:
- The correlation gives the overall direction and strength of the relationship between document length and inclusion count.
- The length bins show whether shorter or longer documents are systematically sampled more often.
- The extremes help verify whether highly included or weakly included documents are concentrated at one end of the length range.

Using the example JSON:
- correlation is `-0.41`
- the `1800-1999` bin has mean inclusion count `628.7`
- the `2600-2800` bin has mean inclusion count `560.4`

That example indicates a negative relationship: shorter documents are being included more often than longer ones, which is plausible for a sampler that stops when it reaches a token target.

### 4. How much overlap is there between sampled corpora?

Primary fields:
- `sampleOverlapSummary.pairSampleCount`
- `sampleOverlapSummary.documentOverlapCount`
- `sampleOverlapSummary.jaccard`
- `sampleOverlapSummary.jaccardHistogram`

How to interpret:
- `pairSampleCount` tells you how many random sample-pairs were inspected.
- `documentOverlapCount` gives the average and range of shared documents between two samples.
- `jaccard` gives normalized overlap, which is easier to compare across designs.
- The histogram shows whether overlaps are tightly concentrated or whether some sample pairs are unusually similar.

Using the example JSON:
- `pairSampleCount` is `10000`
- mean document overlap count is `29.7`
- mean Jaccard is `0.0305`
- max Jaccard is `0.0562`

That example suggests low pairwise overlap, meaning the accepted samples are fairly distinct from one another.

### 5. Are the accepted samples close to the intended token target and document-count profile?

Primary fields:
- `samplingDesign.corpusSizeTokens`
- `samplingDesign.corpusDeltaTokens`
- `sampleSummary.meanSampleTokens`
- `sampleSummary.minSampleTokens`
- `sampleSummary.maxSampleTokens`
- `sampleSummary.meanDocumentsPerSample`
- `sampleSummary.minDocumentsPerSample`
- `sampleSummary.maxDocumentsPerSample`

How to interpret:
- Compare the observed token-count range with the target token count and allowed delta.
- The document-count summary shows how variable the number of documents per sample is under the same sampling design.
- If token counts fall outside the intended acceptance band, that would indicate a problem in stage-1 sampling.

Using the example JSON:
- target token count is `1000000`
- allowed delta is `10000`
- mean sample tokens are `999412.6`
- min sample tokens are `990104`
- max sample tokens are `1009988`
- mean documents per sample are `501.24`

Those example values are all consistent with accepted samples staying inside the intended token window while showing moderate variability in the number of documents needed to reach that target.

### 6. Is the eligible pool being well covered?

Primary fields:
- `poolSummary.eligibleDocumentCount`
- `poolSummary.uniqueDocumentsSampled`
- `poolSummary.neverSampledDocuments`
- `poolSummary.sampledAtLeastOnceProportion`

How to interpret:
- This section tells you whether the stage-1 run is repeatedly reusing only part of the eligible pool or whether the pool is broadly covered.
- A large number of never-sampled documents can indicate a problem or a strong structural bias.

Using the example JSON:
- eligible document count is `8421`
- unique sampled documents are `8421`
- never-sampled documents are `0`
- sampled-at-least-once proportion is `1.0`

That example indicates full pool coverage: every eligible document appeared in at least one accepted sample.

## Example JSON Shape For `sampling_diagnostics_<sampling_design_id>.json`

```json
{
  "artifactType": "sampling_diagnostics",
  "samplingDesign": {
    "samplingDesignId": "sz_1000000_dl_10000_cp_mid",
    "corpusSizeTokens": 1000000,
    "corpusDeltaTokens": 10000,
    "compositionLabel": "mid",
    "sampleCount": 10000,
    "randomSeed": 12345
  },
  "poolSummary": {
    "eligibleDocumentCount": 8421,
    "acceptedSampleCount": 10000,
    "totalDocumentSelections": 5012387,
    "uniqueDocumentsSampled": 8421,
    "neverSampledDocuments": 0,
    "sampledAtLeastOnceProportion": 1.0
  },
  "sampleSummary": {
    "meanSampleTokens": 999412.6,
    "minSampleTokens": 990104,
    "maxSampleTokens": 1009988,
    "meanDocumentsPerSample": 501.24,
    "minDocumentsPerSample": 487,
    "maxDocumentsPerSample": 516
  },
  "documentInclusionSummary": {
    "meanInclusionCount": 595.23,
    "medianInclusionCount": 594,
    "minInclusionCount": 521,
    "maxInclusionCount": 676,
    "standardDeviation": 21.84,
    "coefficientOfVariation": 0.0367
  },
  "documentInclusionHistogram": {
    "binWidth": 10,
    "bins": [
      { "start": 520, "end": 529, "documentCount": 14 }
    ]
  },
  "documentInclusionExtremes": {
    "leastIncludedDocuments": [
      {
        "documentId": 411,
        "title": "Document 411",
        "relativePath": "mid/doc_00411.txt",
        "tokenCount": 2783,
        "inclusionCount": 521
      }
    ],
    "mostIncludedDocuments": [
      {
        "documentId": 77,
        "title": "Document 77",
        "relativePath": "mid/doc_00077.txt",
        "tokenCount": 1834,
        "inclusionCount": 676
      }
    ]
  },
  "lengthBiasSummary": {
    "pearsonCorrelationTokenCountVsInclusionCount": -0.41,
    "lengthBins": [
      {
        "label": "1800-1999",
        "documentCount": 1738,
        "meanTokenCount": 1896.4,
        "meanInclusionCount": 628.7
      }
    ]
  },
  "sampleOverlapSummary": {
    "pairSampleCount": 10000,
    "documentOverlapCount": {
      "mean": 29.7,
      "median": 29,
      "min": 12,
      "max": 54
    },
    "jaccard": {
      "mean": 0.0305,
      "median": 0.0298,
      "min": 0.0121,
      "max": 0.0562
    },
    "jaccardHistogram": {
      "binWidth": 0.005,
      "bins": [
        { "start": 0.01, "end": 0.015, "pairCount": 412 }
      ]
    }
  }
}
```

## Purpose Of The Extraction Diagnostics Artifact

The extraction diagnostics JSON exists to help the user evaluate whether a conditional stage-2 extraction rule is too strict, unstable, dominated by dispersion filtering, or dominated by a small stable core before relying on the final robust set.

The diagnostics are intended to answer questions such as:
- Is this condition retaining enough mass, or is it so strict that very little survives?
- Is retained mass stable from sample to sample, or does it swing sharply?
- Is frequency or document dispersion doing most of the filtering?
- Is the retained aggregate concentrated in a small stable core with a weak unstable tail?
- What does the retained aggregate mass look like before coverage truncation?

These diagnostics are especially useful for:
- evaluating threshold severity before interpreting the robust set
- separating frequency effects from dispersion effects
- showing whether the robust set is being formed from a compact stable head or a noisy long tail

## How To Answer The Stage-2 Diagnostics Questions

Use the fields in `extraction_diagnostics_<condition_id>.json` to answer each stage-2 diagnostics question directly.

### 1. Is this condition too strict?

Primary fields:
- `retainedMassShareBySample`
- `filterCauseMassSplit.passedShare`

How to interpret:
- If most samples fall in very low retained-mass bins, the condition is strict.
- If `passedShare` is small, most observed mass is being filtered away.

Using the example JSON:
- the largest retained-mass bin is `0.15-0.20` with `3210` samples
- the next largest is `0.20-0.25` with `2874` samples
- `passedShare` is `0.226`

That example shows a fairly strict condition: most samples retain only about one fifth of observed mass, and less than one quarter of total observed mass survives overall.

### 2. Is this condition unstable from sample to sample?

Primary fields:
- `retainedMassShareBySample`

How to interpret:
- A narrow concentration of bins implies the amount retained is fairly stable across samples.
- A wide spread across low and high bins implies instability.

Using the example JSON:
- bins with non-trivial counts run from `0.05-0.10` up through `0.35-0.40`
- the mass is centered around `0.15-0.25`, but it is not confined to one narrow bin

That example suggests moderate instability: the condition is not wildly erratic, but retained mass still moves noticeably across samples.

### 3. Is frequency or dispersion doing most of the filtering?

Primary fields:
- `filterCauseMassSplit.failedFrequencyOnlyShare`
- `filterCauseMassSplit.failedDispersionOnlyShare`
- `filterCauseMassSplit.failedBothShare`

How to interpret:
- Compare the frequency-only and dispersion-only shares directly.
- If one is much larger, that filter is doing more of the work.
- `failedBothShare` shows how much mass misses both criteria and should not be credited to only one rule.

Using the example JSON:
- `failedFrequencyOnlyShare` is `0.118`
- `failedDispersionOnlyShare` is `0.471`
- `failedBothShare` is `0.185`

That example shows dispersion doing much more of the filtering than frequency. Nearly half of all observed mass fails dispersion alone, while only about one eighth fails frequency alone.

### 4. Is the condition dominated by a small stable core or by an unstable long tail?

Primary fields:
- `rankMassStabilityProfile.buckets`
- `rankMassStabilityProfile.coverageCrossedAtRank`

How to interpret:
- Look for large early `massShare` values together with high early `meanPassRate`; that is the stable core.
- Later buckets with much smaller `massShare` and falling `meanPassRate` form the unstable tail.
- If coverage is crossed at a low rank, the set is dominated by a compact head.

Using the example JSON:
- ranks `1-5` contribute `0.34` mass share with `meanPassRate` `0.93`
- ranks `6-10` contribute another `0.16` with `meanPassRate` `0.88`
- ranks `11-20` add `0.14` with `meanPassRate` `0.74`
- ranks `161-320` contribute only `0.05` with `meanPassRate` `0.19`
- coverage is crossed at rank `142`

That example shows a strong stable core and a weak unstable tail. Most mass is accumulated early by features that pass often, while later ranks contribute little and pass much less consistently.

### 5. What is the shape of the retained aggregate mass before truncation?

Primary fields:
- `rankMassStabilityProfile.buckets[*].massShare`
- `rankMassStabilityProfile.buckets[*].cumulativeMassEnd`
- `rankMassStabilityProfile.nonZeroFeatureCount`

How to interpret:
- The bucketed rank profile shows how quickly cumulative retained mass accumulates before the coverage target cuts the set off.
- A steep early rise means a head-heavy aggregate.
- A slow continued rise means a longer tail contributes meaningful mass.

Using the example JSON:
- cumulative mass reaches `0.34` by rank `5`
- it reaches `0.50` by rank `10`
- it reaches `0.76` by rank `40`
- it reaches `0.90` by rank `142`
- there are `6132` non-zero retained features overall

That example shows a sharply head-heavy retained aggregate: the top ranks carry much of the mass, but there is still a long tail of retained features beyond the coverage boundary.

## Example JSON Shape For `extraction_diagnostics_<condition_id>.json`

```json
{
  "artifactType": "extraction_diagnostics",
  "condition": {
    "conditionId": "sz_100000_dl_5000_cp_mid_cond_b4_f20_d10_q90",
    "samplingDesignId": "sz_100000_dl_5000_cp_mid",
    "bundleSize": 4,
    "conditionalMode": true,
    "corpusSizeTokens": 100000,
    "corpusDeltaTokens": 5000,
    "compositionLabel": "mid",
    "frequencyThresholdPm": 20,
    "frequencyThresholdRaw": 2,
    "documentDispersionThreshold": 10,
    "sampleCount": 10000,
    "coverageTarget": 0.9
  },
  "retainedMassShareBySample": {
    "binWidth": 0.05,
    "bins": [
      { "start": 0.00, "end": 0.05, "sampleCount": 104 },
      { "start": 0.05, "end": 0.10, "sampleCount": 913 },
      { "start": 0.10, "end": 0.15, "sampleCount": 2148 },
      { "start": 0.15, "end": 0.20, "sampleCount": 3210 },
      { "start": 0.20, "end": 0.25, "sampleCount": 2874 },
      { "start": 0.25, "end": 0.30, "sampleCount": 593 },
      { "start": 0.30, "end": 0.35, "sampleCount": 126 },
      { "start": 0.35, "end": 0.40, "sampleCount": 32 }
    ]
  },
  "filterCauseMassSplit": {
    "passedShare": 0.226,
    "failedFrequencyOnlyShare": 0.118,
    "failedDispersionOnlyShare": 0.471,
    "failedBothShare": 0.185
  },
  "rankMassStabilityProfile": {
    "nonZeroFeatureCount": 6132,
    "coverageTarget": 0.9,
    "coverageCrossedAtRank": 142,
    "buckets": [
      {
        "rankStart": 1,
        "rankEnd": 5,
        "featureCount": 5,
        "massShare": 0.34,
        "cumulativeMassEnd": 0.34,
        "meanPassRate": 0.93
      },
      {
        "rankStart": 6,
        "rankEnd": 10,
        "featureCount": 5,
        "massShare": 0.16,
        "cumulativeMassEnd": 0.50,
        "meanPassRate": 0.88
      },
      {
        "rankStart": 11,
        "rankEnd": 20,
        "featureCount": 10,
        "massShare": 0.14,
        "cumulativeMassEnd": 0.64,
        "meanPassRate": 0.74
      },
      {
        "rankStart": 21,
        "rankEnd": 40,
        "featureCount": 20,
        "massShare": 0.12,
        "cumulativeMassEnd": 0.76,
        "meanPassRate": 0.58
      },
      {
        "rankStart": 41,
        "rankEnd": 80,
        "featureCount": 40,
        "massShare": 0.09,
        "cumulativeMassEnd": 0.85,
        "meanPassRate": 0.36
      },
      {
        "rankStart": 81,
        "rankEnd": 160,
        "featureCount": 80,
        "massShare": 0.04,
        "cumulativeMassEnd": 0.89,
        "meanPassRate": 0.24
      },
      {
        "rankStart": 161,
        "rankEnd": 320,
        "featureCount": 160,
        "massShare": 0.05,
        "cumulativeMassEnd": 0.94,
        "meanPassRate": 0.19
      }
    ]
  }
}
```

## Inspector Support

The generic artifact inspector reads JSON artifacts in this layer and can return:
- artifact type
- artifact path
- full JSON summary text

That makes the analysis-layer JSON artifacts suitable for direct frontend processing without bespoke backend formatting.
