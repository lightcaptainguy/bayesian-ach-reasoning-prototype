#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#include "../include/types.h"
#include <iostream>
#include <vector>
#include "../include/bayesian.h"
#include "../include/json.hpp"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "../include/httplib.h"
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
    httplib::Client cli("https://api.gdeltproject.org");
    cli.enable_server_certificate_verification(false);
    cli.set_connection_timeout(30);
    cli.set_read_timeout(30);
    auto res = cli.Get("/api/v2/doc/doc?query=Taiwan+China+military+sourcelang:english+sourcecountry:US&mode=artlist&maxrecords=10&format=json&timespan=7d");
  if (!res || res->body.empty()) {
    cout << "GDELT request failed\n";
    return 1;
}
json gdeltdata = json::parse(res->body);
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
vector<Hypotheses> originalHypotheses = allHypotheses;
vector<Evidence> allevidence;
for (const auto& item : gdeltdata["articles"]) {
        cout << item["title"].get<string>() << "\n";
        cout << item["url"].get<string>() << "\n";
        cout << "Enter evidence weight according to analysis" << "\n\n";
        Evidence ev;
        ev.description = item ["title"].get<string>();
        for ( const auto& h : allHypotheses) {
            cout << "Weight of evidence for " << h.name << ":" << endl;
            string w;
            cin >> w;
         ev.Weight.push_back(weightMap[w]);
        }
     
        float c;
        cout << "Enter evidence crediblity according to source";
        cin >> c;
        ev.credibility = c;
        allevidence.push_back(ev);
    }

for (auto& ev : allevidence) {
    vector<Weight> originalWeights = ev.Weight;
    double probB = uncondprob(allHypotheses, ev.Weight);
    if (probB == 0) continue;
    posteriorvalue(allHypotheses, ev.Weight, probB);
    updatePriors(allHypotheses);

}

for (auto& h : allHypotheses) h.inconsistency = 0.0;
vector<Evidence> allEvidence;
vector<vector<Weight>> evidenceWeights;
for (const auto& ev : allevidence) {
    evidenceWeights.push_back(ev.Weight);
}
calculateInconsistency(allHypotheses, allevidence);
vector<Hypotheses> baselinePosteriors = allHypotheses;
vector<vector<double>> sensitivityDeltas;
for (auto& ev : allevidence) {
    vector<Weight> savedWeights = ev.Weight;
    fill (ev.Weight.begin(), ev.Weight.end(), Weight::NA);
    allHypotheses = originalHypotheses;
   for (auto& e : allevidence) {
    double probB = uncondprob(allHypotheses, e.Weight);
    if (probB == 0) continue;
    posteriorvalue(allHypotheses, e.Weight, probB);
    updatePriors(allHypotheses);
}
    vector<Hypotheses> sensitivityPosteriors = allHypotheses;
    vector<double> evDeltas;
    for (size_t i=0; i<allHypotheses.size(); i++) {
        double delta = baselinePosteriors[i].posterior - sensitivityPosteriors[i].posterior;
        evDeltas.push_back(delta);
    }
    sensitivityDeltas.push_back(evDeltas);
    ev.Weight = savedWeights;
}

cout << "\nSENSITIVITY ANALYSIS\n";
cout << string(80, '-') << "\n";
cout << "Evidence\t\t\t\t| ";
for (const auto& h : allHypotheses) cout << h.name << "\t| ";
cout << "\n" << string(80, '-') << "\n";
for (size_t e = 0; e < allevidence.size(); e++) {
    cout << allevidence[e].description << "\t| ";
    for (size_t i = 0; i < sensitivityDeltas[e].size(); i++) {
        printf("%.4f\t| ", sensitivityDeltas[e][i]);
    }
    cout << "\n";
}
cout << string(80, '-') << "\n";

cout << "Reasoning Output\n";

for (const auto& h : allHypotheses) {
int filled = (int)(h.posterior * 15); 
 printf("ID: %-2s | Probability: %6.2f%% | Friction: %.2f\n", 
        h.name.c_str(), h.posterior * 100, h.inconsistency);

    for (int i = 0; i < 15; ++i) {
        printf(i < filled ? "|" : ".");
    }
    printf("\n\n");
}
printdetailedreport(allHypotheses, allevidence, evidenceWeights, originalHypotheses);
}

