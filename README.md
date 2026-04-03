# bayesian-ach-reasoning-prototype

A real-time intelligence analysis prototype built in C++ that combines 
Heuer's Analysis of Competing Hypotheses (ACH) with sequential Bayesian 
updating, from live news data from the GDELT Project DOC 2.0 API.

## What it does

The system models an analyst's reasoning process:

1. Load hypotheses from a JSON scenario file with initial priors
2. Fetch recent news articles from GDELT (live, real-time) on a background thread
3. Local LLM (via Ollama) pre-scores each article against hypotheses automatically
4. Analyst reviews LLM suggestions, accepts or overrides weights per hypothesis
5. Engine runs sequential Bayesian updates across all submitted evidence
6. Outputs a structured ACH matrix, posterior probabilities, 
   friction scores, and sensitivity analysis, and a posterior probabilities bar graph.


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
├── main.cpp — terminal pipeline: GDELT fetch → analyst input → reasoning loop → report
├── gui_test.cpp — ImGui GUI: dockable panels, live GDELT feed, LLM pre-scoring, interactive weight input
├── bayesian.cpp — core math: uncondprob, posteriorvalue,
│ calculateInconsistency, sensitivity analysis
├── imgui/ — ImGui docking branch + SDL2/OpenGL3 backends
include/
├── types.h — Hypotheses, Evidence, Article structs; Weight enum
├── bayesian.h — function declarations
├── json.hpp — nlohmann/json (header-only)
├── httplib.h — cpp-httplib (header-only)
src/
├── trial.json — scenario file (hypotheses, priors, GDELT queries)


```

## Output

- ACH matrix (evidence × hypotheses with weight labels)
- Bayesian updates table (prior → posterior, friction, interpretation)
- Sensitivity analysis (posterior delta per evidence item per hypothesis)

## Build

**Terminal pipeline:**
```bash
g++ src/main.cpp src/bayesian.cpp -o src/main -Iinclude -lws2_32 -lssl -lcrypto -lcrypt32
```

**GUI:**
```bash
g++ src/gui_test.cpp src/bayesian.cpp src/imgui/*.cpp -o src/gui_test -I/ucrt64/include -I/ucrt64/include/SDL2 -Iinclude -lmingw32 -lSDL2main -lSDL2 -lopengl32 -lws2_32 -lssl -lcrypto -lcrypt32 -mconsole

```

Requires MSYS2 UCRT64 toolchain with:
- `mingw-w64-ucrt-x86_64-openssl`
- `mingw-w64-ucrt-x86_64-SDL2`

For LLM Pre Scoring, requires Ollama + Qwen 2.5 7b model:
- ``` ollama pull qwen2.5:7b ```
- The LLM endpoint is configurable in the Scenario Config panel at runtime, defaults to http://localhost:11434


## Status
Active development — LLM Pre Scoring shipped April 3rd, 2026.

## Reports

### Report 01 — Will the USA-Iran War Stretch? (March 28, 2026)

Scenario: Day 27 of US-Israel-Iran war, assessing near-term outcome

Hypotheses: Negotiated Ceasefire / Ground Invasion / Iran Capitulates / Protracted Stalemate

Priors: Equal (0.25 each), genuine ambiguity, let evidence decide

Result: H3 (Iran Capitulates) 70.4%, H4 (Stalemate) 20.5%, H2 (Invasion) 5.5%, H1 (Ceasefire) 3.6%

Key insight: ACH elimination logic — H3 leads not because evidence confirms capitulation, but because ceasefire, invasion, and stalemate face stronger contradictions in the current evidence set


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

***Date 28th March, 2026*** -> Complete evidence pipeline shipped + first report generated:

- Multi-query GDELT fetch with deduplication
- Analyst credibility scoring (0.0–1.0) per source at terminal
- Sensitivity analysis fully working — posterior delta per evidence item per hypothesis
- Friction scoring reimplemented correctly
- Dual output report -- ACH matrix + Bayesian updates in single formatted output
- Critical bug fix -- default weight for unscored articles changed HS → NA
- Report 01 complete -- USA-Iran War near-term outcome assessment

***Date 1st April, 2026*** -> GUI shipped:

- ImGui docking branch + SDL2 + OpenGL3 backend
- 5 dockable panels: Scenario Config, Evidence Feed, Weight Input, ACH Matrix, Bayesian Output
- Scenario Config loads hypotheses live from JSON
- Evidence Feed fetches 93+ articles from GDELT on startup, clickable with URL tooltip on hover
- Weight Input panel: per-hypothesis dropdowns (7-level weight scale) + credibility slider, linked to selected article via index
- ACH Matrix: live `ImGui::BeginTable` updating on each evidence submission
- Bayesian Output: reruns full sequential Bayesian loop on submitted evidence each frame, displays posteriors + friction scores live

***Date 3rd April, 2026*** -> LLM Pre-Scoring + UI polish:
- Local LLM integration via Ollama (OpenAI-compatible API) — fires on article selection, pre-fills weight dropdowns with suggested weights and one-line justifications per hypothesis
- Human in the loop design — LLM suggestions are always analyst-reviewable, never submitted autonomously
- Configurable LLM endpoint in Scenario Config panel (defaults to localhost:11434)
- Sensitivity analysis section now scrollable with horizontal scroll support
- Bar chart x-axis labels shortened to H1/H2/H3/H4 to fix overlap


