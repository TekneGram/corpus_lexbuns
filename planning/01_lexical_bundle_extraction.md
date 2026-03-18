# What is lexical bundle extraction?

Usually, when extracting lexical bundles, a corpus is selected and 3-grams and 4-gram (sometimes 5-grams and 6-grams and longer) are counted. Those that are above a normalized frequency (standard is 40 per million, but less for smaller corpora) are extracted. This cutoff threshold is quite arbitrary but has been established over the years.

However, the problem with this is that is makes comparing corpora of different sizes difficult because the Zipfian distribution means that what is 40 times per million in a one million word corpus might be 50 times per million in a 500,000 word corpus.

So, we need a way to extract lexical bundles that is not dependent on the corpus size. This experiment will attempt to show that a robust bundle set can be extracted using the following methodology.

## 1. Build a set of corpus samples, e.g., 10,000 samples
Vary the size of the samples in each set.
- 10,000 samples of size 100,000 words
- 10,000 samples of size 250,000 words
- 10,000 samples of size 500,000 words
- 10,000 samples of size 750,000 words
- 10,000 samples of size 1,000,000 words

Repeat the above for different corpus compositions
- Texts of length between 100 and 700 words
- Texts of length between 700 and 1500 words
- Texts of length greater than 1500 words

## 2. Extract lexical bundles with frequency threshold and dispersion criteria
- Set cutoff threshold:
    - Cutoff frequency of 40 per million
    - Cutoff frequency of 20 per million
    - Cutoff frequency of 10 per million
    - Cutoff frequency of 5 per million
- Set document range criteria (dispersion)
    - Must occur in at least 10 documents
    - Must occur in at least 5 documents
- Set subcorpora range criteria (dispersion)
    - Must occur in at least 3 subcorpora

For all the sampling criteria above, we perform extraction on each sample for all the combinations of the threshold and dispersion criteria.

## Experimental setup

### Approach 1: Setting threshold frequencies and dispersion criteria
Based on the above sections 1 and 2 the following experimental conditions can be established for a corpus with 4 main subcorpora. We wil not use subcorpora range criteria. The table below shows the 24 experimental conditions varying the dispersion, frequency threshold and composition of a 100,000 word corpus. The same table can be created for a corpus of 250,000 words, 500,000 words, 750,000 words and 1,000,000 words. This totals to 24 x 5 experimental conditions, which is 120 experiments. Sample size remains the same. Depending on the time taken to perform these experiments, the sample size might later be increased to 100,000.

| Experiment Number | Corpus size | Sample size | Composition (text length range) | Frequency Threshold | Dispersion: Documents |
| --- | --- | --- | --- | --- | --- |
| 1 | 100,000 | 10,000 | 100-700 | 40pm | 10 |
| 2 | 100,000 | 10,000 | 100-700 | 40pm | 5 |
| 3 | 100,000 | 10,000 | 100-700 | 20pm | 10 |
| 4 | 100,000 | 10,000 | 100-700 | 20pm | 5 |
| 5 | 100,000 | 10,000 | 100-700 | 10pm | 10 |
| 6 | 100,000 | 10,000 | 100-700 | 10pm | 5 |
| 7 | 100,000 | 10,000 | 100-700 | 5pm | 10 |
| 8 | 100,000 | 10,000 | 100-700 | 5pm | 5 |
| 9 | 100,000 | 10,000 | 700-1500 | 40pm | 10 |
| 10 | 100,000 | 10,000 | 700-1500 | 40pm | 5 |
| 11 | 100,000 | 10,000 | 700-1500 | 20pm | 10 |
| 12 | 100,000 | 10,000 | 700-1500 | 20pm | 5 |
| 13 | 100,000 | 10,000 | 700-1500 | 10pm | 10 |
| 14 | 100,000 | 10,000 | 700-1500 | 10pm | 5 |
| 15 | 100,000 | 10,000 | 700-1500 | 5pm | 10 |
| 16 | 100,000 | 10,000 | 700-1500 | 5pm | 5 |
| 17 | 100,000 | 10,000 | >1500 | 40pm | 10 |
| 18 | 100,000 | 10,000 | >1500 | 40pm | 5 |
| 19 | 100,000 | 10,000 | >1500 | 20pm | 10 |
| 20 | 100,000 | 10,000 | >1500 | 20pm | 5 |
| 21 | 100,000 | 10,000 | >1500 | 10pm | 10 |
| 22 | 100,000 | 10,000 | >1500 | 10pm | 5 |
| 23 | 100,000 | 10,000 | >1500 | 5pm | 10 |
| 24 | 100,000 | 10,000 | >1500 | 5pm | 5 |

### Approach 2: No threshold frequencies or dispersion criteria
This approach uses no threshold frequencies or dispersion criteria and so will extract the bundles only with the final accumulation of mass at the end of the experiment.
Under this much simpler set of conditions, there are three experiments per corpus size, each for a different composition of the corpus. Since there are 5 corpus sizes, this means there are 5 x 3 experiments: 15.

| Experiment Number | Corpus size | Sample size | Composition (text length range) | 
| --- | --- | --- | --- |
| 1 | 100,000 | 10,000 | 100-700 |
| 2 | 100,000 | 10,000 | 700-1500 |
| 3 | 100,000 | 10,000 | >1500 |

### Lexical bundle lengths
Both approaches must be run for lexical bundles of lengths 3 and 4, i.e., 3grams and 4grams. This means that the total number of experimental conditions is (120 + 15) * 2 = 270.

## 3. Analysis

The analysis should be carried out separately for the two approaches, but with the same general logic: repeated random samples are used to estimate which bundles contribute stable frequency mass under each experimental condition.

### 3.1 Define the sampled corpora

For each experimental condition, let the accepted random corpus samples be:

$$
S_1, S_2, \dots, S_M
$$

where $M = 10{,}000$ unless the number of samples is later increased. Each $S_m$ is one sampled corpus satisfying the corpus-size and composition constraints for that condition.

For every lexical bundle $b$ in every sampled corpus $S_m$, record:

- its raw frequency $F_b(S_m)$
- the number of sampled documents in which it occurs, $R_b^{text}(S_m)$

We will not use subcorpus-range criteria in this experiment, so no discipline- or subcorpus-range threshold is applied in the main analysis.

### 3.2 Analysis for Approach 1: threshold frequencies and document dispersion

For each sample $S_m$, apply the extraction rule:

$$
A_b(S_m) = 1\{F_b(S_m) \ge c_f,\; R_b^{text}(S_m) \ge c_t\}
$$

where:

- $c_f$ is the raw frequency threshold corresponding to the normalized threshold for that corpus size
- $c_t$ is the document-dispersion threshold, either 5 or 10 documents

Because the frequency thresholds are specified in per-million terms, they must be converted to raw counts for each corpus size. For a corpus of size $N$ tokens and a threshold of $p$ per million, the raw cutoff is:

$$
c_f = \frac{p}{1{,}000{,}000} \times N
$$

For example, in a 100,000-word sample, a threshold of 40 per million corresponds to:

$$
\frac{40}{1{,}000{,}000} \times 100{,}000 = 4
$$

For each bundle $b$, compute the number of samples in which it is extracted:

$$
n_b = \sum_{m=1}^{M} A_b(S_m)
$$

Then estimate its thresholded expected frequency mass:

$$
\hat m^{cond}_b = \frac{1}{M}\sum_{m=1}^{M} F_b(S_m)A_b(S_m)
$$

This quantity represents the average contribution of bundle $b$ across repeated resamples after the extraction criteria have been applied.

Next, normalize these masses:

$$
w^{cond}_b = \frac{\hat m^{cond}_b}{\sum_j \hat m^{cond}_j}
$$

The bundles are then ranked from highest to lowest $w^{cond}_b$. The robust bundle set for that condition is defined as the smallest ranked set whose cumulative mass reaches a chosen target coverage level $q$:

$$
\mathcal{B}^{cond}_q = \{b_{(1)}, \dots, b_{(k)}\}
$$

where $k$ is the smallest value such that:

$$
\sum_{i=1}^{k} w^{cond}_{(i)} \ge q
$$

In practice, several values of $q$ can be examined, for example $q = 0.80$, $0.90$, and $0.95$, to see whether conclusions are stable across different coverage levels.

### 3.3 Analysis for Approach 2: no threshold frequencies or dispersion criteria

For the threshold-free approach, all bundles are retained in each sample. No filtering is applied at the sample level.

For each bundle $b$, estimate its unconditional expected frequency across repeated samples:

$$
\hat\mu_b = \frac{1}{M}\sum_{m=1}^{M} F_b(S_m)
$$

If needed, document-dispersion can still be summarized descriptively as:

$$
\hat\mu^{text}_b = \frac{1}{M}\sum_{m=1}^{M} R_b^{text}(S_m)
$$

The expected frequencies are then normalized:

$$
w_b = \frac{\hat\mu_b}{\sum_j \hat\mu_j}
$$

Bundles are ranked from highest to lowest $w_b$, and the robust bundle set is defined as the smallest set whose cumulative normalized mass reaches the target coverage level $q$:

$$
\mathcal{B}^{uncond}_q = \{b_{(1)}, \dots, b_{(k)}\}
$$

where $k$ is the smallest value satisfying:

$$
\sum_{i=1}^{k} w_{(i)} \ge q
$$

This produces a threshold-free bundle set based only on the cumulative expected mass of bundles across repeated resamples.

### 3.4 Compare robust bundle sets across conditions

Once a robust bundle set has been constructed for every condition, compare them across corpus sizes, corpus compositions, and, in Approach 1, threshold/dispersion settings.

For any two conditions $a$ and $b$, set overlap can be measured with the Jaccard coefficient:

$$
J(\mathcal{B}_a,\mathcal{B}_b) =
\frac{|\mathcal{B}_a \cap \mathcal{B}_b|}{|\mathcal{B}_a \cup \mathcal{B}_b|}
$$

High Jaccard values indicate that the bundle sets are similar despite changes in corpus size or corpus composition. This is the main test of robustness and invariance.

The main comparisons are:

- across corpus sizes within the same composition
- across corpus compositions within the same corpus size
- in Approach 1, across different frequency-threshold and document-dispersion settings
- between Approach 1 and Approach 2 for matched corpus size and composition conditions

### 3.5 Assess within-condition stability

In addition to comparing final robust sets, it is useful to assess how stable the extraction is within each condition across the repeated samples themselves.

For Approach 1, each sampled corpus already produces an extracted set $E(S_m)$. Pairwise similarity between two sampled corpora $S_i$ and $S_j$ can be measured as:

$$
J(E(S_i), E(S_j))
$$

The mean pairwise Jaccard value within a condition provides a measure of within-condition stability. If this value is high, the extraction criteria produce similar bundle sets across repeated random samples. If it is low, the condition is unstable even before the final aggregation step.

For Approach 2, there is no sample-level thresholded set, so stability is assessed primarily through the final robust sets and through the smoothness of the bundle rankings across repeated samples.

### 3.6 Interpret the results

The analysis should be interpreted in terms of robustness. A method can be considered robust if:

- the final bundle sets are highly similar across corpus sizes
- the final bundle sets are highly similar across corpus compositions
- in Approach 1, the final bundle sets remain similar across different threshold and dispersion settings
- the within-condition stability is reasonably high across repeated random samples

If Approach 2 yields bundle sets that are as stable as or more stable than Approach 1, this would support the claim that lexical bundles can be extracted without relying on arbitrary frequency thresholds. If Approach 1 remains highly stable across its threshold settings, this would suggest that the traditional threshold-based approach can also be made more principled when evaluated over repeated resampled corpora rather than a single corpus snapshot.
