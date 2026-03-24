#include <iostream>
#include "../include/bayesian.h"
#include <vector>
using namespace std;

double weightTolikelihood(Weight w) {
    switch(w) {
        case Weight::HIGH_SUPPORT: return 0.9;
        case Weight::MEDIUM_SUPPORT: return 0.6;
        case Weight::LOW_SUPPORT: return 0.3;
        case Weight::NA: return 0.5;
        case Weight::LOW_REFUTE:     return 0.2;
        case Weight::MEDIUM_REFUTE:  return 0.15;
        case Weight::HIGH_REFUTE:    return 0.1;
        default:                     return 0.5;
    }
}
double uncondprob(vector <Hypotheses>& Hypotheses, vector<Weight>& Weight) {
double sum = 0.0;
for (int i=0; i < Hypotheses.size(); i++) {
    sum += Hypotheses[i].prior * weightToLikelihood(Weight[i]);  // summation 
}
return sum;
}
void posteriorvalue(vector<Hypotheses>& Hypotheses, vector<Weight>& Weight, double probB) {
    for (int i=0; i < Hypotheses.size(); i++) {
        double likelihood = weightTolikelihood(Weight[i]);
        Hypotheses[i].posterior = (likelihood * Hypotheses[i].prior) / probB;
    }
}