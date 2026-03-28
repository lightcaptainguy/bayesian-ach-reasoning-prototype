# bayesian-ach-reasoning-prototype

A real-time intelligence analysis prototype built in C++ that combines 
Heuer's Analysis of Competing Hypotheses (ACH) with sequential Bayesian 
updating, from live news data from the GDELT Project DOC 2.0 API.

## What it does

The system models an analyst's reasoning process:

1. Load hypotheses from a JSON scenario file with initial priors
2. Fetch recent news articles from GDELT (live, real-time)
3. Analyst reviews each article and assigns evidence weights 
   (HIGH_SUPPORT → HIGH_REFUTE) and source credibility (0.0–1.0)
4. Engine runs sequential Bayesian updates across all evidence
5. Outputs a structured ACH matrix, posterior probabilities, 
   friction scores, and sensitivity analysis

## Methodology

- **ACH** — Heuer's *Psychology of Intelligence Analysis* (1999)
- **Probabilistic extension** — Karvetski & Valtorta's Bayesian network 
  extension of ACH for intelligence analysis
- **Weight system** — Extended from Heuer's binary C/I to 7-level scale:
  HIGH_SUPPORT / MEDIUM_SUPPORT / LOW_SUPPORT / NA / 
  LOW_REFUTE / MEDIUM_REFUTE / HIGH_REFUTE
  Treated as Bayesian likelihoods (0.1–0.9), preserving more 
  information than binary while staying analytically grounded

## Architecture
```
src/
├── main.cpp — pipeline: GDELT fetch → analyst input → reasoning loop
├── bayesian.cpp — core math: uncondprob, posteriorvalue,
calculateInconsistency, sensitivity analysis
include/
├── types.h — Hypotheses struct, Evidence struct, Weight enum
├── bayesian.h — function declarations
├── json.hpp — nlohmann/json (header-only)
├── httplib.h — cpp-httplib (header-only)
```

## Output

- ACH matrix (evidence × hypotheses with weight labels)
- Bayesian updates table (prior → posterior, friction, interpretation)
- Sensitivity analysis (posterior delta per evidence item per hypothesis)

## Build

```bash
g++ src/main.cpp src/bayesian.cpp -o src/main -lws2_32 -lssl -lcrypto -lcrypt32
```
Requires OpenSSL (mingw-w64-x86_64-openssl via pacman on MSYS2).

## Status
Working prototype, active development.

## ***(old devlogs) Timeline****
 
***Date 12th March, 2026*** -> still in research phase, reading Heuer's "Psychology of Intelligence Analysis" and the two research papers by Valtorta and Karveski on extension of ACH using probabilistic methods for complex analysis before implementation

***Date 16th March, 2026*** -> Refined the system :

Updated ACH scoring for computational use:

- Replace classic C/I/Neutral with **High / Medium / Low / N/A / High Refute / Medium Refute / Low Refute** weights
- Treat these as Bayesian likelihood strengths (High = strong update, N/A = no update)
- Allows dynamic downgrading of prior evidence when new contradictory info arrives
- More information-preserving than binary
- Closer to how real analysts think + mathematically cleaner for Bayes integration

The goal of this is to keep ACH's focus on elimination of confirmation bias while making it suitable for probabilistic updating.

Also architected the workflow ( high-level, on paper) and defined what is needed to be done :
1. **Scenario**  
   Define the core question/context.

2. **Hypotheses**  
   Generate mutually exclusive set (H1–HN), assign initial priors (equal or base-rate informed).

3. **Evidence**  
   Collect items (manual hardcoded values first, later via GDELT API for real-time news/events).

4. **ACH Matrix**  
   Rows: Evidence (E1–EN).  
   Columns: Hypotheses.  
   Cells: **High / Medium / Low / N/A / High Refute / Medium Refute / Low Refute** (weighted consistency + diagnostic strength).

5. **Probability Updater**  
   Use matrix weights as likelihoods → apply sequential Bayes' rule.  
   Normalize posteriors.  
   Allow downgrading of prior evidence on new contradictions.

6. **Report**  
   Ranked hypotheses by posterior %.  
   Highlight most diagnostic evidence + sensitivity/changes over time.

***Date 24th March, 2026*** ->
- Defined complete type system in `types.h` (Hypothesis struct with name/prior/posterior, Weight enum covering HIGH_SUPPORT through HIGH_REFUTE + NA)  
- Created full architecture in `bayesian.h` / `bayesian.cpp`:  
  - `weightTolikelihood()` — maps Weight enum → numeric likelihood (0.1–0.9)   
  - `uncondprob()` — computes P(B) normalizing constant using Law of Total Probability (∑ P(B|Ai) × P(Ai))  
  - `posteriorvalue()` — performs Bayesian update in-place on all hypotheses given one piece of evidence  
- Core Bayesian reasoning is now functional for sequential ACH updates (one evidence at a time, posteriors become new priors)  


