# What is lexical bundle extraction?

Usually, when extracting lexical bundles, a corpus is selected and 3-grams and 4-gram (sometimes 5-grams and 6-grams and longer) are counted. Those that are above a normalized frequency (standard is 40 per million, but less for smaller corpora) are extracted. This cutoff threshold is quite arbitrary but has been established over the years.

However, the problem with this is that is makes comparing corpora of different sizes difficult because the Zipfian distribution means that what is 40 times per million in a one million word corpus might be 50 times per million in a 500,000 word corpus.

So, we need a way to extract lexical bundles that is not dependent on the corpus size. This experiment will attempt to show that a robust bundle set can be extracted using the following methodology.

## 1. Build a set of corpus samples, e.g., 10,000 samples
Vary the size of the samples in each set.
- 10,000 samples of size 100,000 words
- 10,000 samples of size 200,000 words
- 10,000 samples of size 300,000 words
- 10,000 samples of size 500,000 words
- 10,000 samples of size 750,000 words
- 10,000 samples of size 1,000,000 words

Repeat the above for different corpus compositions
- Texts of length between 100 and 700 words
- Texts of length between 700 and 1500 words
- Texts of length greater than 1500 words.

## 2. 