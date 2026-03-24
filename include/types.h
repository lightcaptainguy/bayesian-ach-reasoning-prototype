#pragma once
#include <iostream>
#include <string>

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
};

struct Evidence {
    std::string description;
    float credibility;

};