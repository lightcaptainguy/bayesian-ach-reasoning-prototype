#include "../include/types.h"
#include <iostream>
#include <vector>
#include "../include/bayesian.h"
using namespace std;
int main() {
    //harcdoed scenario 
vector<Hypotheses> Hypotheses = {
    {"H1", 0.5, 0.0, 0.0},
    {"H2", 0.3, 0.0, 0.0},
    {"H3", 0.2, 0.0, 0.0}
};
vector<Evidence> allevidence = {
{"E1", {Weight::HIGH_SUPPORT, Weight::MEDIUM_REFUTE}, 0.95f},
{"E2", {Weight::MEDIUM_REFUTE, Weight::HIGH_SUPPORT}, 0.30f}
};
for (const auto& ev : allevidence) {
    double probB = uncondprob(Hypotheses, ev.Weight) ;
    if (probB == 0) continue;
    posteriorvalue(Hypotheses, ev.Weight, probB);
    updatePriors(Hypotheses);
}
calculateInconsistency(Hypotheses, allevidence);
cout << "Reasoning Output\n";
for (const auto& h : Hypotheses) {
    printf("ID: %-20s | Probability: %6.2f%% | Friction: %4.2f\n",
    h.name.c_str(), h.posterior*100, h.inconsistency);
}
}
