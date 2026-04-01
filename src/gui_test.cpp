#include "SDL_video.h"
#define SDL_MAIN_HANDLED
#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#include <SDL.h>
#include "../include/imgui.h"
#include "../include/imgui_internal.h"
#include "../include/imgui_impl_sdl2.h"
#include "../include/imgui_impl_opengl3.h"
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
using namespace std;
using json = nlohmann::json;

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
    httplib::Client cli("https://api.gdeltproject.org");
    cli.enable_server_certificate_verification(false);
    cli.set_connection_timeout(30);
    cli.set_read_timeout(30);

    set<string> seenTitles;
    for (const auto& q : data["Queries"]) {
        string query = q.get<string>();
        cout << "Fetching: " << query << "\n";
        while (true) {
            auto res = cli.Get("/api/v2/doc/doc?query=" + query + "&mode=artlist&maxrecords=30&format=json&timespan=1d");
            if (!res || res->body.empty() || res->body[0] != '{') {
                cout << "Rate limited, retrying in 30s...\n";
                this_thread::sleep_for(chrono::seconds(30));
                continue;
            }
            json gdeltdata = json::parse(res->body);
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
            break;
        }
        this_thread::sleep_for(chrono::seconds(10));
    }


    vector<Evidence> allevidence;
    int selectedArticleIndex = -1;

    const char* weightNames[] = {"HIGH_SUPPORT", "MEDIUM_SUPPORT", "LOW_SUPPORT", "NA", "LOW_REFUTE", "MEDIUM_REFUTE", "HIGH_REFUTE"};
    Weight weightValues[] = {Weight::HIGH_SUPPORT, Weight::MEDIUM_SUPPORT, Weight::LOW_SUPPORT, Weight::NA, Weight::LOW_REFUTE, Weight::MEDIUM_REFUTE, Weight::HIGH_REFUTE};
    vector<int> currentWeightIndices(allHypotheses.size(), 3);
    float credibility = 0.5f;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("ACH Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
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

        ImGui::Begin("Scenario Config");
        ImGui::Text("Hypotheses");
        ImGui::Separator();
        for (const auto& h : allHypotheses) {
            ImGui::Text("%s", h.name.c_str());
        }
        ImGui::End();

    
        ImGui::Begin("Evidence Feed");
        ImGui::Text("%d articles loaded", (int)articles.size());
        ImGui::Separator();
        for (int i = 0; i < (int)articles.size(); i++) {
            bool selected = (selectedArticleIndex == i);
            if (ImGui::Selectable(articles[i].title.c_str(), selected)) {
                selectedArticleIndex = i;
                fill(currentWeightIndices.begin(), currentWeightIndices.end(), 3); // reset to NA
                credibility = 0.5f;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", articles[i].url.c_str());
            }
        }
        ImGui::End();

    
        ImGui::Begin("Weight Input");
        if (selectedArticleIndex == -1) {
            ImGui::Text("Select an article from Evidence Feed");
        } else {
            ImGui::TextWrapped("%s", articles[selectedArticleIndex].title.c_str());
            ImGui::Separator();
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
                allevidence.push_back(ev);
                selectedArticleIndex = -1;
            }
        }
        ImGui::End();

        
        ImGui::Begin("ACH Matrix");
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

        
        ImGui::Begin("Bayesian Output");
        if (allevidence.empty()) {
            ImGui::Text("No evidence submitted yet");
        } else {
            
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
    ImGui::DestroyContext();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
