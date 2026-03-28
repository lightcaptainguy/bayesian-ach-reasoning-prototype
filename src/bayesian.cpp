#include <iostream>
#include "../include/bayesian.h"
#include "../include/types.h"
#include <vector>
#include <iomanip>
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
            Hypotheses[i].inconsistency += (base_penalty * ev.credibility)/Evidence_list.size();
        }
    }
}
void printdetailedreport(vector<Hypotheses>& hypotheses, const vector<Evidence>& evidencenames, const vector<vector<Weight>>& evidenceweights, const vector<Hypotheses>& originalHypotheses, ostream& out) {
    out << "\n Reasoning Output \n";
   out << "ACH MATRIX (Evidence vs Hypotheses)\n" ;
    out << std::string(90, '-') << "\n";
    out << left << setw(55) << "Evidence / Hypothesis";

    for (const auto& h : hypotheses) {
        out << "| " << h.name << " ";
    }
    out << "\n" << std::string(90, '-') << "\n";
    for (size_t e = 0; e < evidencenames.size(); ++e) {
        out << left << setw(75) << evidencenames[e].description.substr(0, 74);

        
        for (size_t h = 0; h < hypotheses.size(); ++h) {

            Weight w = ( h < evidenceweights[e].size() ) ? evidenceweights[e][h] : Weight::NA;
            std::string label;
            switch(w) {
                case Weight::HIGH_SUPPORT:    label = "HS"; break;
                case Weight::MEDIUM_SUPPORT:  label = "MS"; break;
                case Weight::LOW_SUPPORT:     label = "LS"; break;
                case Weight::HIGH_REFUTE:     label = "HR"; break;
                case Weight::MEDIUM_REFUTE:   label = "MR"; break;
                case Weight::LOW_REFUTE:      label = "LR"; break;
                case Weight::NA:              label = "NA"; break;
                default:                      label = "NA";
            }
           out << "| " << setw(5) << label;

        }
        out << "\n\n";
    }

    out << std::string(90, '-') << "\n\n";

    out << "BAYESIAN UPDATES\n";
    out << std::string(80, '-') << "\n";
    out << left << setw(6) << "ID" << "| " << setw(20) << "Hypothesis" << "| " << setw(10) << "Prior" << "| " << setw(10) << "Posterior" << "| " << setw(10) << "Friction" << "| Interpretation\n";

    out << std::string(80, '-') << "\n";

    for (size_t i = 0; i < hypotheses.size(); ++i) {
        double incons = hypotheses[i].inconsistency;  
        std::string interp;
        if (incons < 0.3)      interp = "Strongly consistent";
        else if (incons < 0.5) interp = "Mildly consistent";
        else if (incons < 0.7) interp = "Neutral";
        else if (incons < 0.9) interp = "Mildly inconsistent";
        else                   interp = "Strongly inconsistent";

       out << left << setw(6) << ("H" + to_string(i+1))
          << "| " << setw(20) << hypotheses[i].name
          << "| " << setw(10) << to_string(originalHypotheses[i].prior*100).substr(0,6) + "%"
          << "| " << setw(10) << to_string(hypotheses[i].posterior*100).substr(0,6) + "%"
          << "| " << setw(10) << incons
          << "| " << interp << "\n";

    }
    out << std::string(80, '-') << "\n";
}