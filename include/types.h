#pragma once
#include <iostream>
#include <string>
#include <vector>

enum class Weight {

    HIGH_SUPPORT,
    MEDIUM_SUPPORT,
    LOW_SUPPORT,
    NA,
    LOW_REFUTE,
    MEDIUM_REFUTE,
    HIGH_REFUTE

};

struct Hypotheses {
    std::string name;
    float prior;
    float posterior;
    float inconsistency;
};

struct Evidence {
    std::string description;
    std::vector<Weight> Weight;
    float credibility;

};