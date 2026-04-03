#include "SDL_video.h"
#define SDL_MAIN_HANDLED
#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#include <SDL.h>
#include "../include/imgui.h"
#include "../include/imgui_internal.h"
#include "../include/imgui_impl_sdl2.h"
#include "../include/imgui_impl_opengl3.h"
#include "../include/implot.h"
#include "../include/implot_internal.h"
#include <GL/GL.h>
#include "../include/types.h"
#include "../include/bayesian.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "../include/httplib.h"
#include "../include/json.hpp"
#include <vector>
#include <fstream>
#include <thread>
#include <chrono>
#include <set>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <map>
#include <cstdlib>
using namespace std;
using json = nlohmann::json;

struct LLMSuggestion {
    vector<int> weightIndices;
    vector<string> justifications;
    bool ready = false;
    bool failed = false;
    string errorMsg;
    int forArticle = -1;
};

void callClaudeForWeights(
    string llmBaseUrl,
    string articleTitle,
    vector<Hypotheses> hypotheses,
    LLMSuggestion* suggestion,
    mutex* suggMutex,
    atomic<bool>* pending,
    int articleIndex
) {
    string prompt =
        "You are an intelligence analyst using ACH (Analysis of Competing Hypotheses). "
        "Score this news article against each hypothesis.\n\n"
        "Article: " + articleTitle + "\n\nHypotheses:\n";
    for (int i = 0; i < (int)hypotheses.size(); i++)
        prompt += "H" + to_string(i + 1) + ": " + hypotheses[i].name + "\n";
    prompt +=
        "\nWeight options: HIGH_SUPPORT, MEDIUM_SUPPORT, LOW_SUPPORT, NA, LOW_REFUTE, MEDIUM_REFUTE, HIGH_REFUTE\n"
        "\nACH principle: look for inconsistency, not confirmation. "
        "Assign NA unless the article directly speaks to that hypothesis.\n"
        "\nReturn ONLY valid JSON, no other text:\n"
        "{\"weights\":[\"NA\",...],\"justifications\":[\"one sentence for H1\",...]}";

    json reqBody = {
        {"model", "qwen2.5:7b"},
        {"max_tokens", 512},
        {"messages", {{{"role", "user"}, {"content", prompt}}}}
    };

    httplib::Client cli(llmBaseUrl);
    cli.enable_server_certificate_verification(false);
    cli.set_connection_timeout(30);
    cli.set_read_timeout(60);

    httplib::Headers headers = {
        {"content-type", "application/json"}
    };

    LLMSuggestion result;
    result.forArticle = articleIndex;

    auto res = cli.Post("/v1/chat/completions", headers, reqBody.dump(), "application/json");
    if (!res || res->status != 200) {
        result.failed = true;
        result.errorMsg = res ? ("HTTP " + to_string(res->status)) : "connection failed";
    } else {
        try {
            json resp = json::parse(res->body);
            string content = resp["choices"][0]["message"]["content"].get<string>();
            size_t start = content.find('{');
            size_t end = content.rfind('}');
            if (start == string::npos || end == string::npos)
                throw runtime_error("no JSON in response");

            json parsed = json::parse(content.substr(start, end - start + 1));
            map<string, int> wmap = {
                {"HIGH_SUPPORT", 0}, {"MEDIUM_SUPPORT", 1}, {"LOW_SUPPORT", 2},
                {"NA", 3}, {"LOW_REFUTE", 4}, {"MEDIUM_REFUTE", 5}, {"HIGH_REFUTE", 6}
            };
            for (const auto& w : parsed["weights"]) {
                string ws = w.get<string>();
                result.weightIndices.push_back(wmap.count(ws) ? wmap.at(ws) : 3);
            }
            for (const auto& j : parsed["justifications"])
                result.justifications.push_back(j.get<string>());
            result.ready = true;
        } catch (exception& e) {
            result.failed = true;
            result.errorMsg = string("parse error: ") + e.what();
        }
    }

    {
        lock_guard<mutex> lock(*suggMutex);
        *suggestion = result;
    }
    *pending = false;
}

int main() {
    
    ifstream f("trial.json");
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

    vector<Article> articles;
    mutex articlesMutex;
    atomic<bool> fetchDone{false};
    string fetchStatus = "Fetching articles...";

    thread fetchThread([&]() {
        httplib::Client cli("https://api.gdeltproject.org");
        cli.enable_server_certificate_verification(false);
        cli.set_connection_timeout(30);
        cli.set_read_timeout(30);
        set<string> seenTitles;
        for (const auto& q : data["Queries"]) {
            string query = q.get<string>();
            while (true) {
                auto res = cli.Get("/api/v2/doc/doc?query=" + query + "&mode=artlist&maxrecords=30&format=json&timespan=1d");
                if (!res || res->body.empty() || res->body[0] != '{') {
                    this_thread::sleep_for(chrono::seconds(30));
                    continue;
                }
                json gdeltdata = json::parse(res->body);
                {
                    lock_guard<mutex> lock(articlesMutex);
                    for (const auto& art : gdeltdata["articles"]) {
                        string title = art["title"].get<string>();
                        if (seenTitles.find(title) == seenTitles.end()) {
                            seenTitles.insert(title);
                            Article a;
                            a.title = title;
                            a.url = art["url"].get<string>();
                            articles.push_back(a);
                        }
                    }
                }
                break;
            }
            this_thread::sleep_for(chrono::seconds(10));
        }
        fetchDone = true;
    });
    fetchThread.detach();


    vector<Evidence> allevidence;
    int selectedArticleIndex = -1;
    map<int, int> submittedMap;

    const char* weightNames[] = {"HIGH_SUPPORT", "MEDIUM_SUPPORT", "LOW_SUPPORT", "NA", "LOW_REFUTE", "MEDIUM_REFUTE", "HIGH_REFUTE"};
    Weight weightValues[] = {Weight::HIGH_SUPPORT, Weight::MEDIUM_SUPPORT, Weight::LOW_SUPPORT, Weight::NA, Weight::LOW_REFUTE, Weight::MEDIUM_REFUTE, Weight::HIGH_REFUTE};
    vector<int> currentWeightIndices(allHypotheses.size(), 3);
    float credibility = 0.5f;

    char llmUrl[256] = "http://localhost:11434";

    LLMSuggestion llmSuggestion;
    mutex llmMutex;
    atomic<bool> llmPending{false};

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("ACH Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::Begin("DockSpace", nullptr, flags);
        ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_None);
        ImGui::End();

        ImGui::Begin("Scenario Config", nullptr, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::Text("Hypotheses");
        ImGui::Separator();
        for (const auto& h : allHypotheses) {
            ImGui::Text("%s", h.name.c_str());
        }
        ImGui::Separator();
        ImGui::Text("Local LLM");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##llmurl", llmUrl, sizeof(llmUrl));
        ImGui::End();

    
        ImGui::Begin("Evidence Feed", nullptr, ImGuiWindowFlags_HorizontalScrollbar);
        {
            lock_guard<mutex> lock(articlesMutex);
            if (!fetchDone)
                ImGui::Text("Fetching articles... %d loaded", (int)articles.size());
            else
                ImGui::Text("%d articles loaded", (int)articles.size());
            ImGui::Separator();
            for (int i = 0; i < (int)articles.size(); i++) {
                bool selected = (selectedArticleIndex == i);
                if (ImGui::Selectable(articles[i].title.c_str(), selected)) {
                    selectedArticleIndex = i;
                    if (submittedMap.count(i) > 0) {
                        int evidenceIndex = submittedMap[i];
                        for (int j = 0; j < (int)allHypotheses.size(); j++)
                            currentWeightIndices[j] = (int)allevidence[evidenceIndex].Weight[j];
                        credibility = allevidence[evidenceIndex].credibility;
                    } else {
                        fill(currentWeightIndices.begin(), currentWeightIndices.end(), 3);
                        credibility = 0.5f;
                        // Trigger LLM pre-scoring for new article
                        if (llmUrl[0] != '\0' && !llmPending && llmSuggestion.forArticle != i) {
                            {
                                lock_guard<mutex> llmLock(llmMutex);
                                llmSuggestion = LLMSuggestion{};
                            }
                            llmPending = true;
                            string title = articles[i].title;
                            vector<Hypotheses> hypCopy = allHypotheses;
                            thread t(callClaudeForWeights, string(llmUrl), title, hypCopy,
                                     &llmSuggestion, &llmMutex, &llmPending, i);
                            t.detach();
                        }
                    }
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", articles[i].url.c_str());
            }
        }
        ImGui::End();

    
        ImGui::Begin("Weight Input", nullptr, ImGuiWindowFlags_HorizontalScrollbar);
        if (selectedArticleIndex == -1) {
            ImGui::Text("Select an article from Evidence Feed");
        } else {
            ImGui::TextWrapped("%s", articles[selectedArticleIndex].title.c_str());
            ImGui::Separator();

            // LLM suggestion display
            {
                lock_guard<mutex> llmLock(llmMutex);
                if (llmPending && llmSuggestion.forArticle != selectedArticleIndex) {
                    // pending for a different article, ignore
                } else if (llmPending) {
                    ImGui::TextDisabled("AI: analyzing...");
                    ImGui::Separator();
                } else if (llmSuggestion.ready && llmSuggestion.forArticle == selectedArticleIndex) {
                    if (ImGui::Button("Accept AI Weights")) {
                        for (int i = 0; i < (int)llmSuggestion.weightIndices.size() && i < (int)currentWeightIndices.size(); i++)
                            currentWeightIndices[i] = llmSuggestion.weightIndices[i];
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("AI pre-scored (review before submitting)");
                    for (int i = 0; i < (int)allHypotheses.size(); i++) {
                        if (i < (int)llmSuggestion.weightIndices.size()) {
                            const char* justif = i < (int)llmSuggestion.justifications.size()
                                ? llmSuggestion.justifications[i].c_str() : "";
                            ImGui::TextDisabled("  H%d %s: %s — %s", i + 1,
                                allHypotheses[i].name.c_str(),
                                weightNames[llmSuggestion.weightIndices[i]],
                                justif);
                        }
                    }
                    ImGui::Separator();
                } else if (llmSuggestion.failed && llmSuggestion.forArticle == selectedArticleIndex) {
                    ImGui::TextDisabled("AI error: %s", llmSuggestion.errorMsg.c_str());
                    ImGui::Separator();
                }
            }

            for (int i = 0; i < (int)allHypotheses.size(); i++) {
                ImGui::Text("%s", allHypotheses[i].name.c_str());
                ImGui::SameLine();
                string comboId = "##weight" + to_string(i);
                ImGui::SetNextItemWidth(160);
                ImGui::Combo(comboId.c_str(), &currentWeightIndices[i], weightNames, 7);
            }
            ImGui::Separator();
            ImGui::SliderFloat("Credibility", &credibility, 0.0f, 1.0f);
            if (ImGui::Button("Submit Evidence")) {
                Evidence ev;
                ev.description = articles[selectedArticleIndex].title;
                for (int i = 0; i < (int)allHypotheses.size(); i++) {
                    ev.Weight.push_back(weightValues[currentWeightIndices[i]]);
                }
                ev.credibility = credibility;
                if (submittedMap.count(selectedArticleIndex) > 0) {
                    allevidence[submittedMap[selectedArticleIndex]] = ev;
                }
                else {
                    allevidence.push_back(ev);
                    submittedMap[selectedArticleIndex] = allevidence.size() - 1;
                }
        
                selectedArticleIndex = -1;
            }
        }
        ImGui::End();

        
        ImGui::Begin("ACH Matrix", nullptr, ImGuiWindowFlags_HorizontalScrollbar);
        if (allevidence.empty()) {
            ImGui::Text("No evidence submitted yet");
        } else {
            if (ImGui::BeginTable("matrix", (int)allHypotheses.size() + 1, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX)) {
                ImGui::TableSetupColumn("Evidence");
                for (const auto& h : allHypotheses) {
                    ImGui::TableSetupColumn(h.name.c_str());
                }
                ImGui::TableHeadersRow();

                for (const auto& ev : allevidence) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(ev.description.substr(0, 40).c_str());
                    for (int i = 0; i < (int)ev.Weight.size(); i++) {
                        ImGui::TableNextColumn();
                        const char* label = "NA";
                        switch (ev.Weight[i]) {
                            case Weight::HIGH_SUPPORT:   label = "HS"; break;
                            case Weight::MEDIUM_SUPPORT: label = "MS"; break;
                            case Weight::LOW_SUPPORT:    label = "LS"; break;
                            case Weight::LOW_REFUTE:     label = "LR"; break;
                            case Weight::MEDIUM_REFUTE:  label = "MR"; break;
                            case Weight::HIGH_REFUTE:    label = "HR"; break;
                            default:                     label = "NA"; break;
                        }
                        ImGui::Text("%s", label);
                    }
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();

        
        ImGui::Begin("Bayesian Output", nullptr, ImGuiWindowFlags_HorizontalScrollbar);
        if (allevidence.empty()) {
            ImGui::Text("No evidence submitted yet");
        } else {
            if (ImGui::Button("Export Report")) {
                vector<Hypotheses> exportHyp = originalHypotheses;
                for (const auto& ev : allevidence) {
                    double probB = uncondprob(exportHyp, ev.Weight);
                    if (probB == 0) continue;
                    posteriorvalue(exportHyp, ev.Weight, probB);
                    updatePriors(exportHyp);
                }
                for (auto& h : exportHyp) h.inconsistency = 0.0;
                calculateInconsistency(exportHyp, allevidence);
                vector<vector<Weight>> evidenceWeights;
                for (const auto& ev : allevidence)
                    evidenceWeights.push_back(ev.Weight);
                ofstream outfile("report.txt");
                printdetailedreport(exportHyp, allevidence, evidenceWeights, originalHypotheses, outfile);
                outfile.close();
                cout << "Report exported to report.txt\n";
            }
            ImGui::Separator();
            vector<Hypotheses> runHypotheses = originalHypotheses;
            for (const auto& ev : allevidence) {
                double probB = uncondprob(runHypotheses, ev.Weight);
                if (probB == 0) continue;
                posteriorvalue(runHypotheses, ev.Weight, probB);
                updatePriors(runHypotheses);
            }
            for (auto& h : runHypotheses) h.inconsistency = 0.0;
            calculateInconsistency(runHypotheses, allevidence);

            ImGui::Text("Evidence count: %d", (int)allevidence.size());
            ImGui::Separator();
            for (const auto& h : runHypotheses) {
                ImGui::Text("%-40s  %.2f%%   friction: %.3f", h.name.c_str(), h.posterior * 100.0, h.inconsistency);
            }
            ImGui::Separator();
            ImGui::Text("Sensitivity Analysis");
            ImGui::Separator();
            vector<Hypotheses> baselinePosteriors = runHypotheses;
            vector<vector<double>> sensitivityDeltas;

            for (auto& ev : allevidence) {
                vector<Weight> savedWeights = ev.Weight;
                fill(ev.Weight.begin(), ev.Weight.end(), Weight::NA);
                vector<Hypotheses> tempHyp = originalHypotheses;
                for (auto& e : allevidence) {
                    double probB = uncondprob(tempHyp, e.Weight);
                    if (probB == 0) continue;
                    posteriorvalue(tempHyp, e.Weight, probB);
                    updatePriors(tempHyp);
                }
                vector<double> evDeltas;
                for (size_t i = 0; i < tempHyp.size(); i++) {
                    double delta = baselinePosteriors[i].posterior - tempHyp[i].posterior;
                    evDeltas.push_back(delta);
                }
                sensitivityDeltas.push_back(evDeltas);
                ev.Weight = savedWeights;
            }
            ImGui::BeginChild("##sensitivity_scroll", ImVec2(0, 160), false,
                ImGuiWindowFlags_HorizontalScrollbar);
            for (int e = 0; e < (int)allevidence.size(); e++) {
                ImGui::Text("E%d: %s", e + 1, allevidence[e].description.substr(0, 30).c_str());
                for (int h = 0; h < (int)allHypotheses.size(); h++) {
                    ImGui::Text("  H%d %s: %+.3f", h + 1, allHypotheses[h].name.c_str(), sensitivityDeltas[e][h]);
                }
                ImGui::Spacing();
            }
            ImGui::EndChild();
            ImGui::Separator();
            ImGui::Text("Posterior Probabilities");
            vector<double> posteriorValues;
            vector<const char*> shortLabels;
            vector<string> shortLabelStrs;
            for (int i = 0; i < (int)runHypotheses.size(); i++) {
                posteriorValues.push_back(runHypotheses[i].posterior * 100.0);
                shortLabelStrs.push_back("H" + to_string(i + 1));
            }
            for (const auto& s : shortLabelStrs) shortLabels.push_back(s.c_str());
            if (ImPlot::BeginPlot("##posteriors", ImVec2(-1, 200))) {
                ImPlot::SetupAxes(nullptr, "Probability %");
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImGuiCond_Always);
                ImPlot::SetupAxisTicks(ImAxis_X1, 0, (int)posteriorValues.size() - 1, (int)posteriorValues.size(), shortLabels.data());
                ImPlot::PlotBars("##bars", posteriorValues.data(), (int)posteriorValues.size(), 0.6);
                ImPlot::EndPlot();
            }
        }

        ImGui::End();

        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
