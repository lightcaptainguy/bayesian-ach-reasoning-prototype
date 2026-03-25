#include <iostream>
#include "../include/bayesian.h"
#include "../include/types.h"
#include <vector>
using namespace std;

double weightTolikelihood(Weight w) {
    switch(w) {
        case Weight::HIGH_SUPPORT:   return 0.9;
        case Weight::MEDIUM_SUPPORT: return 0.6;
        case Weight::LOW_SUPPORT:    return 0.3;
        case Weight::NA:             return 0.5;
        case Weight::LOW_REFUTE:     return 0.2;
        case Weight::MEDIUM_REFUTE:  return 0.15;
        case Weight::HIGH_REFUTE:    return 0.1;
        default:                     return 0.5;
    }
}
double weightToinconsistency(Weight w) {
    switch(w) {
        case Weight::HIGH_SUPPORT:   return 0.1;
        case Weight::MEDIUM_SUPPORT: return 0.15;
        case Weight::LOW_SUPPORT:    return 0.2;
        case Weight::NA:             return 0.5;
        case Weight::LOW_REFUTE:     return 0.3;
        case Weight::MEDIUM_REFUTE:  return 0.6;
        case Weight::HIGH_REFUTE:    return 0.9;
        default:                     return 0.5;
    }
}
double uncondprob(vector<Hypotheses>& Hypotheses, const vector<Weight>& weights) {
double sum = 0.0;
for (int i=0; i < Hypotheses.size(); i++) {
    Weight w = (i < weights.size()) ? weights[i] : Weight::NA;
    sum += Hypotheses[i].prior * weightTolikelihood(w);  // summation 
}
return sum;
}
void posteriorvalue(vector<Hypotheses>& Hypotheses, const vector<Weight>& weights, double probB) {
    for (int i=0; i < Hypotheses.size(); i++) {
        Weight w = (i < weights.size()) ? weights[i] : Weight::NA;
        double likelihood = weightTolikelihood(w);
        Hypotheses[i].posterior = (likelihood * Hypotheses[i].prior) / probB;
        Hypotheses[i].inconsistency = weightToinconsistency(w);
    }
}
void updatePriors(vector<Hypotheses>& Hypotheses) {
    for (int i=0; i < Hypotheses.size(); i++) {
        Hypotheses[i].prior = Hypotheses[i].posterior;
    }
}
void calculateInconsistency(vector<Hypotheses>& Hypotheses, const vector<Evidence>& Evidence_list) {
    for (const auto& ev : Evidence_list) {
        for (size_t i=0; i < Hypotheses.size(); i++) {
            Weight w = (i < ev.Weight.size()) ? ev.Weight[i] : Weight::NA;
            double base_penalty = weightToinconsistency(w);
            Hypotheses[i].inconsistency += (base_penalty * ev.credibility);
        }
    }
}