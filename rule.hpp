#pragma once
#include <string>
#include "near_triangulation.hpp"

using std::string;

class Rule {
private:
    NearTriangulation rule_;
    int send_edgeid_, amount_;

public:
    Rule(int from, int to, int amount, const NearTriangulation &rule);
    static Rule readRuleFile(const string &filename);

    const NearTriangulation &nearTriangulation(void) const;
    int sendEdgeId(void) const;
    int amount(void) const;
};

vector<Rule> getRules(const std::string &dirname);
