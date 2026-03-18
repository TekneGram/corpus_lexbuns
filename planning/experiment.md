# Experiment Outline

This experiment has two parallel versions:

- **Conditional version**: apply a frequency threshold within each sampled corpus before constructing the set
- **Unconditional version**: apply no frequency threshold within sampled corpora; construct the set directly from cumulative resampled mass

Both versions share the same sampling design.

## 1. Define the corpus population

Start with a fixed corpus of documents. Each document has:

- token count
- discipline label
- any text-type label you want to control

Choose the bundle definition in advance, for example contiguous 4-word bundles. Keep preprocessing fixed across the whole experiment.

## 2. Define experimental conditions

Each condition specifies the corpus design to sample from. For example:

- corpus sample size: `100k`, `300k`, `500k`, `1,000k`
- corpus composition: long, mid, short, proportional, discipline-balanced
- threshold frequency condition:
  - conditional version: include levels such as `10`, `20`, `30`, `40` per million, converted into raw sample thresholds for that corpus size
  - unconditional version: no threshold frequency condition is applied during sampling

If range constraints are also part of the design, define them here too, such as minimum text range or minimum discipline range.

## 3. Generate repeated random corpus samples

For each condition, repeatedly draw random corpora from the relevant document pool.

For each sampled corpus:

- accept only samples meeting the target token count and composition constraints
- repeat until you have `M` accepted samples, e.g. `10,000`

Denote the accepted sampled corpora by:

\[
S_1, S_2, \dots, S_M
\]

Each `S_m` is a random sampled corpus under the condition.

## 4. Count bundles in each sampled corpus

For every sample `S_m`, count all lexical bundles and record for each bundle `b`:

- frequency:
\[
F_b(S_m)
\]
- text range:
\[
R_b^{text}(S_m)
\]
- discipline range:
\[
R_b^{disc}(S_m)
\]

Definitions:

- `F_b(S_m)` = total number of occurrences of bundle `b` in sampled corpus `S_m`
- `R_b^{text}(S_m)` = number of distinct sampled documents in `S_m` containing at least one occurrence of `b`
- `R_b^{disc}(S_m)` = number of distinct disciplines represented among documents in `S_m` that contain `b`

This gives the unconditional bundle statistics for each condition.

## Conditional Version: Threshold Applied Within Each Sample

## 5C. Apply the sample-level extraction rule

For each bundle `b` in each sample `S_m`, define extraction by:

\[
A_b(S_m)=1\{F_b(S_m)\ge c_f,\; R_b^{text}(S_m)\ge c_t,\; R_b^{disc}(S_m)\ge c_d\}
\]

where:

- `c_f` is the raw frequency threshold for that condition
- `c_t` is the text-range threshold
- `c_d` is the discipline-range threshold

Only bundles satisfying the rule are retained in that sample's extracted output.

## 6C. Aggregate the thresholded outputs across resamples

Across the `M` sampled corpora, collect the extracted bundles and compute for each bundle:

- number of samples in which it was extracted:
\[
n_b = \sum_{m=1}^M A_b(S_m)
\]

- conditional mean extracted frequency:
\[
\mu^{cond}_b = \frac{\sum_{m=1}^M F_b(S_m)A_b(S_m)}{\sum_{m=1}^M A_b(S_m)}
\]

- conditional extracted mass:
\[
m^{cond}_b = E[F_b(S)\cdot 1\{A_b(S)=1\}]
\]

Estimated empirically by:
\[
\hat m^{cond}_b = \frac{1}{M}\sum_{m=1}^M F_b(S_m)A_b(S_m)
\]

This is the expected frequency mass contributed by bundle `b` after thresholding.

## 7C. Define the robust bundle set

Normalize these masses:
\[
w^{cond}_b = \frac{\hat m^{cond}_b}{\sum_j \hat m^{cond}_j}
\]

Rank bundles from largest to smallest `w^{cond}_b`.

Define the robust bundle set at coverage level `q` as the smallest set of top-ranked bundles whose cumulative normalized mass reaches `q`:

\[
\mathcal{B}^{cond}_q = \{b_{(1)}, \dots, b_{(k)}\}
\]

where `k` is the smallest index satisfying:

\[
\sum_{i=1}^k w^{cond}_{(i)} \ge q
\]

This is the thresholded version of the set.

## Unconditional Version: No Threshold Applied Within Each Sample

## 5U. Do not filter bundles within samples

For each sample `S_m`, retain all bundles and their counts. No frequency threshold is applied at this stage.

## 6U. Aggregate unconditional frequencies across resamples

For each bundle `b`, estimate its unconditional expected frequency:

\[
\mu_b = E[F_b(S)]
\]

Empirically:
\[
\hat\mu_b = \frac{1}{M}\sum_{m=1}^M F_b(S_m)
\]

This includes zeros implicitly when a bundle is absent from a sample.

If you also want text- or discipline-range summaries, estimate:

\[
\hat\mu^{text}_b = \frac{1}{M}\sum_{m=1}^M R_b^{text}(S_m)
\]

\[
\hat\mu^{disc}_b = \frac{1}{M}\sum_{m=1}^M R_b^{disc}(S_m)
\]

## 7U. Define the robust bundle set

Normalize the unconditional expected frequencies:

\[
w_b = \frac{\hat\mu_b}{\sum_j \hat\mu_j}
\]

Rank bundles from largest to smallest `w_b`.

Define the robust bundle set at coverage level `q` as the smallest set of top-ranked bundles whose cumulative normalized expected mass reaches `q`:

\[
\mathcal{B}^{uncond}_q = \{b_{(1)}, \dots, b_{(k)}\}
\]

where `k` is the smallest index such that:

\[
\sum_{i=1}^k w_{(i)} \ge q
\]

This is the threshold-free version of the set.

## Common Downstream Analysis for Both Versions

## 8. Repeat for all conditions

Construct one robust bundle set for every condition:

- different corpus sizes
- different corpus compositions
- and, in the conditional version, different threshold frequency levels

## 9. Compare sets across conditions

For two conditions `a` and `b`, measure set similarity using Jaccard:

\[
J(\mathcal{B}_a,\mathcal{B}_b) =
\frac{|\mathcal{B}_a \cap \mathcal{B}_b|}{|\mathcal{B}_a \cup \mathcal{B}_b|}
\]

Compute pairwise Jaccard values across all conditions.

This is the main test of invariance:

- high Jaccard across corpus sizes suggests size-independence
- high Jaccard across corpus compositions suggests composition-independence
- in the conditional version, high Jaccard across threshold values suggests threshold-independence

## 10. Assess within-condition stability

For each condition, you can also examine the stability of the extracted or sample-level sets themselves by comparing individual sample-level sets pairwise. This is especially natural for the conditional version.

For resamples `S_i` and `S_j`, define:

\[
J(E(S_i), E(S_j))
\]

Then summarize mean pairwise Jaccard within condition.

This tells you whether the condition itself produces a stable bundle set under repeated sampling.

## 11. Assess parameter sensitivity

Examine whether the robust bundle set changes sharply or smoothly when you vary:

- corpus size
- corpus composition
- threshold frequency level
- coverage level `q`

Compare neighboring conditions using Jaccard. A robust region is one in which small parameter changes do not substantially alter the set.

## 12. Formal hypothesis

The main substantive hypothesis is:

There exists a robust bundle set that is approximately invariant across corpus sizes and corpus compositions under repeated sampling.

In the conditional version, this extends to threshold frequencies:

There exists a robust bundle set that is approximately invariant across corpus sizes, corpus compositions, and threshold frequency conditions under repeated sampling.

## 13. Recommended outputs

For each condition, report:

- size of the robust bundle set
- the bundle members
- cumulative mass covered
- pairwise Jaccard similarities with other conditions
- optionally, within-condition stability summaries

If needed, attach bootstrap confidence intervals to the Jaccard estimates.

## Compact difference between the two versions

- **Conditional version**:
  threshold first within each sampled corpus, then define the robust set from thresholded resampling mass

- **Unconditional version**:
  no threshold within sampled corpora; define the robust set directly from unconditional expected frequency mass
