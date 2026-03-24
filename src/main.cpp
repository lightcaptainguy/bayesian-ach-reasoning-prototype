#include "../include/types.h"
#include <iostream>
#include <vector>
#include "../include/bayesian.h"
using namespace std;
int main() {
    //harcdoed scenario 
vector<Hypotheses> Hypotheses = {
    {"H1", 0.5, 0.0},
    {"H2", 0.3, 0.0},
    {"H3", 0.2, 0.0}
};
vector<vector<Weight>> allevidence = {
{Weight::HIGH_SUPPORT, Weight::MEDIUM_SUPPORT, Weight::LOW_SUPPORT},
{Weight::HIGH_REFUTE, Weight::MEDIUM_REFUTE, Weight::LOW_REFUTE}
};
for (int e=0; e < allevidence.size(); e++) {
    double probB = uncondprob(Hypotheses, allevidence[e]) ;
    if (probB == 0) continue;
    posteriorvalue(Hypotheses, allevidence[e], probB);
    updatePriors(Hypotheses);
}
for (int i=0; i<Hypotheses.size(); i++) {
    cout << "H" << i << ":" << Hypotheses[i].posterior << endl;
}
}
