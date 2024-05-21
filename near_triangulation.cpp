#include <spdlog/spdlog.h>
#include <fmt/ranges.h>
#include "near_triangulation.hpp"

using std::ifstream;

Degree::Degree(int lower_deg, int upper_deg) : 
    lower_deg_(lower_deg), 
    upper_deg_(upper_deg) {};

Degree::Degree(int deg) :
    lower_deg_(deg),
    upper_deg_(deg) {};

// "5", "5+", "8-" などの次数を表す文字列から Degree クラスを返す。
Degree Degree::fromString(const string &str) {
    std::size_t i;
    int deg = std::stoi(str, &i);
    if (i == str.size()) {
        return Degree(deg);
    } 
    if (i == str.size() - 1) {
        if (str[i] == '+') return Degree(deg, MAX_DEGREE);
        if (str[i] == '-') return Degree(MIN_DEGREE, deg);
    }
    spdlog::critical("Failed to parse {} as degree", str);
    throw std::runtime_error("Failed to parse " + str + " as degree");
}

int Degree::lower(void) const {
    return lower_deg_;
}

int Degree::upper(void) const {
    return upper_deg_;
}

string Degree::toString(void) const {
    if (fixed()) return std::to_string(lower_deg_);
    if (upper_deg_ == MAX_DEGREE) return std::to_string(lower_deg_) + "+";
    if (lower_deg_ == MIN_DEGREE) return std::to_string(upper_deg_) + "-";
    assert(false);
}

bool Degree::include(const Degree &degree) const {
    return lower_deg_ <= degree.lower_deg_ && degree.upper_deg_ <= upper_deg_;
}

bool Degree::disjoint(const Degree &degree0, const Degree &degree1) {
    return degree0.upper_deg_ < degree1.lower_deg_ || degree1.upper_deg_ < degree0.lower_deg_;
}

bool Degree::fixed(void) const {
    return lower_deg_ == upper_deg_;
}


// degree のとりうる範囲 [l, r] を 5, 6, ..., max_degree+ の次数に分ける。
// 例えば、max_degree = 8 の時
// 5+ -> 5, 6, 7, 8+
// 7+ -> 7, 8+ 
// 6- は 5, 6 のように分ける。
vector<Degree> divideDegree(const Degree &degree, int max_degree) {
    vector<Degree> degrees;
    int deg = degree.lower();
    assert(deg <= max_degree);
    while (deg < degree.upper() && deg < max_degree) {
        degrees.push_back(Degree(deg));
        deg++;
    }
    degrees.push_back(Degree(deg, degree.upper()));
    return degrees;
}

NearTriangulation::NearTriangulation(int vertex_size, const vector<set<int>> &VtoV, const vector<optional<Degree>> &degrees) : 
    vertex_size_(vertex_size),
    degrees_(degrees) {

    for (int v = 0;v < vertex_size_; v++) {
        for (int u : VtoV[v]) {
            edges_.emplace_back(v, u);
        }
    }

    for (const auto &edge : edges_) {
        auto [v, u] = edge;
        for (int w : VtoV[v]) {
            if (VtoV[u].count(w)) {
                diagonal_vertices_[edge].push_back(w);
            }
        }
        spdlog::trace("diagonal vertices ({}, {}) : {}", v, u, fmt::join(diagonal_vertices_[edge], ", "));
        assert(diagonal_vertices_[edge].size() <= 2);
    }

}

int NearTriangulation::vertexSize(void) const {
    return vertex_size_;
}

const vector<optional<Degree>> &NearTriangulation::degrees(void) const {
    return degrees_;
}

const vector<pair<int, int>> &NearTriangulation::edges(void) const {
    return edges_;
}

const map<pair<int, int>, vector<int>> &NearTriangulation::diagonalVertices(void) const {
    return diagonal_vertices_;
}


void NearTriangulation::setDegree(int v, const optional<Degree> &degree) {
    degrees_[v] = degree;
    return;
}


string NearTriangulation::debug(void) const {
    string buf = "";
    vector<set<int>> VtoV(vertex_size_);
    for (const auto &e : edges_) {
        VtoV[e.first].insert(e.second);
        VtoV[e.second].insert(e.first);
    }
    for (int v = 0;v < vertex_size_; v++) {
        buf += fmt::format("{} {} {}\n", v, degrees_[v] ? degrees_[v].value().toString() : "?", fmt::join(VtoV[v], ", "));
    }
    return buf;
}

