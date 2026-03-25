#include "../include/types.h"
#include <iostream>
#include <vector>
#include "../include/bayesian.h"
#include "../include/json.hpp"
#include <fstream>
using namespace std;
using json = nlohmann::json;
map<string, Weight> weightMap = {
    {"HIGH_SUPPORT", Weight::HIGH_SUPPORT},
    {"MEDIUM_SUPPORT", Weight::MEDIUM_SUPPORT},
    {"LOW_SUPPORT", Weight::LOW_SUPPORT},
    {"NA", Weight::NA},
    {"LOW_REFUTE", Weight::LOW_REFUTE},
    {"MEDIUM_REFUTE", Weight::MEDIUM_REFUTE},
    {"HIGH_REFUTE", Weight::HIGH_REFUTE}
};
int main() {
   
   std::ifstream f("trial.json");
   json data = json::parse(f);

vector<Hypotheses> allHypotheses;
  for (const auto& item : data["Hypotheses"]) {
    Hypotheses hyp;
    hyp.name = item["name"].get<string>();
    hyp.prior = item["prior"].get<double>();
    hyp.posterior = item["posterior"].get<double>();
    hyp.inconsistency = item["inconsistency"].get<double>();
    allHypotheses.push_back(hyp);
  }

vector<Evidence> allevidence;
for (const auto& item : data["Evidence"]) {
     Evidence ev;
    ev.description = item["description"].get<string>();
    for (const auto& w : item["Weight"]) {
        ev.Weight.push_back(weightMap[w.get<string>()]);
    }
    ev.credibility = item["credibility"].get<double>();
    allevidence.push_back(ev);
}

for (const auto& ev : allevidence) {
    double probB = uncondprob(allHypotheses, ev.Weight) ;
    if (probB == 0) continue;
    posteriorvalue(allHypotheses, ev.Weight, probB);
    updatePriors(allHypotheses);
}

for (auto& h : allHypotheses) h.inconsistency = 0.0;

calculateInconsistency(allHypotheses, allevidence);
cout << "Reasoning Output\n";
for (const auto& h : allHypotheses) {
    printf("ID: %-20s | Probability: %6.2f%% | Friction: %4.2f\n",
    h.name.c_str(), h.posterior*100, h.inconsistency);
}
}
