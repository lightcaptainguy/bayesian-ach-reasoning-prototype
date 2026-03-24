# bayesian-ach-reasoning-prototype
Prototype Implementation of Heuer's ACH ( Analysis of Competing Hypotheses ) + simple Bayesian updating

First version -> matrix + basic posterior calculation + inconsistency shaping 

## Methodology 
- Heuer's Analysis of Competing Hypotheses (ACH)

- Karvetski's "Structuring and analyzing competing hypotheses
with Bayesian networks for intelligence analysis" & Valtorta's probabilistic ACH extension

 - Bayesian updating via new evidence
 
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
  - `weightTolikelihood()` — maps Weight enum → numeric likelihood (0.1–0.9) via clean switch  
  - `uncondprob()` — computes P(B) normalizing constant using Law of Total Probability (∑ P(B|Ai) × P(Ai))  
  - `posteriorvalue()` — performs Bayesian update in-place on all hypotheses given one piece of evidence  
- Core Bayesian reasoning is now functional for sequential ACH updates (one evidence at a time, posteriors become new priors)  


