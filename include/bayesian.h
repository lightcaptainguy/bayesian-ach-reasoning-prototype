#pragma once
#include "types.h"
#include <vector>
//function 1 = calculation of P(B) - the normalizing constant
// takes in - prior from hypotheses, and Probability P(B|Ahypotheses )
// ( probability that B happens whenever hypotheses happens regardless)
//outputs -  P(B) = Σ P(B|Ai) × P(Ai)

//function 2 - calculation of posterior gices value to hypotheses struct
//update all hypothesis posteriors given one piece of evidence
// Takes in - P(B) from function 1, Prior of hypotheses ( only one this time)
//probability of B conditioned that the taken hypotheses occurs regardless
//Outputs - updates the posterior float and updates the table according to the evidence
