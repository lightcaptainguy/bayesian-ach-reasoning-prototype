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
#include <thread>
#include <chrono>
#include <set>
#include <algorithm>
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
    ofstream outfile("report.txt");
    std::ifstream f("trial.json");
   json data = json::parse(f);

    httplib::Client cli("https://api.gdeltproject.org");
    cli.enable_server_certificate_verification(false);
    cli.set_connection_timeout(30);
    cli.set_read_timeout(30);
    json allarticles = json::array();
    for (const auto& q : data["Queries"]) {
    string query = q.get<string>();
    while (true) {

        auto res = cli.Get("/api/v2/doc/doc?query="+ query +"&mode=artlist&maxrecords=30&format=json&timespan=1d");
        if (!res || res->body.empty() || res->body[0] != '{') {
            cout << "Rate limited, retrying in 30s...\n";
            this_thread::sleep_for(chrono::seconds(30));
            continue;
        }
        json gdeltdata = json::parse(res->body);
        for (const auto& article : gdeltdata["articles"]) {
            allarticles.push_back(article);
        }
        break;
  }
  this_thread::sleep_for(chrono::seconds(10));
}


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
set<string> seenTitles;
json uniquetitles = json::array();
for (const auto& article : allarticles) {
    string title = article["title"].get<string>();
    if (seenTitles.find(title) == seenTitles.end()) {
        seenTitles.insert(title);
        uniquetitles.push_back(article);
    }
}
for (const auto& item : uniquetitles) {
        cout << item["title"].get<string>() << "\n";
        cout << item["url"].get<string>() << "\n";
        cout << "Enter evidence weight according to analysis" << "\n\n";
        Evidence ev;
        ev.description = item ["title"].get<string>();
        for ( const auto& h : allHypotheses) {
            cout << "Weight of evidence for " << h.name << ":" << endl;
            string w;
            cin >> w;
            transform(w.begin(), w.end(), w.begin(), ::toupper);

         if (weightMap.find(w) == weightMap.end()) {
    cout << "Invalid, defaulting to NA\n";
    ev.Weight.push_back(Weight::NA);
} else {
    ev.Weight.push_back(weightMap[w]);
}

        }
     
        float c;
        cout << "Enter evidence crediblity according to source\n";
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
outfile << "\nSENSITIVITY ANALYSIS\n";
cout << string(80, '-') << "\n";
outfile << string(80, '-') << "\n";

cout << left << setw(55) << "Evidence" << "| ";
outfile << left << setw(55) << "Evidence" << "| ";
for (const auto& h : allHypotheses) { cout << setw(8) << h.name << "| "; outfile << setw(8) << h.name << "| "; }
cout << "\n" << string(90, '-') << "\n";
outfile << "\n" << string(90, '-') << "\n";

for (size_t e = 0; e < allevidence.size(); e++) {
    cout << left << setw(55) << allevidence[e].description.substr(0, 54);
    outfile << left << setw(55) << allevidence[e].description.substr(0, 54);
    for (size_t i = 0; i < sensitivityDeltas[e].size(); i++) {
        cout << "| " << setw(8) << fixed << setprecision(4) << sensitivityDeltas[e][i];
        outfile << "| " << setw(8) << fixed << setprecision(4) << sensitivityDeltas[e][i];
    }
    cout << "\n"; outfile << "\n";
}

cout << string(80, '-') << "\n";
outfile << string(80, '-') << "\n";
cout << "Reasoning Output\n";
outfile << "Reasoning Output\n";
string Scenarioname = data["Scenario"].get<string>();
cout << "\n========================================\n";
outfile << "\n========================================\n";
cout << "SCENARIO: " << Scenarioname << "\n";
outfile << "SCENARIO: " << Scenarioname << "\n";
cout << "========================================\n\n";
outfile << "========================================\n\n";


for (const auto& h : allHypotheses) {
    int filled = (int)(h.posterior * 15);
    string bar = "";
    for (int i = 0; i < 15; ++i) bar += (i < filled ? "|" : ".");
    
    string line = "ID: " + h.name + " | Probability: " + to_string(h.posterior*100).substr(0,6) + "% | Friction: " + to_string(h.inconsistency).substr(0,4) + "\n" + bar + "\n\n";
    cout << line;
    outfile << line;
}

printdetailedreport(allHypotheses, allevidence, evidenceWeights, originalHypotheses, cout);
printdetailedreport(allHypotheses, allevidence, evidenceWeights, originalHypotheses, outfile);
outfile.close();
}

