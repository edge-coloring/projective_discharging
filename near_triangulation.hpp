#pragma once
#include <vector>
#include <utility>
#include <map>
#include <string>
#include <fstream>
#include <set>
#include <optional>

using std::vector;
using std::pair;
using std::map;
using std::tuple;
using std::string;
using std::set;
using std::optional;

// 次数は高々12くらいまでしか考えないので inf として1000を設定
const int MAX_DEGREE = 1000;
const int MIN_DEGREE = 5;

class Degree {
private:
    int lower_deg_, upper_deg_;

public:
    Degree(int lower_deg, int upper_deg);
    Degree(int deg);
    static Degree fromString(const string &str);

    int lower(void) const;
    int upper(void) const;

    string toString(void) const;
    bool include(const Degree &degree) const;
    static bool disjoint(const Degree &degree0, const Degree &degree1);
    bool fixed(void) const;
};

vector<Degree> divideDegree(const Degree &degree, int max_degree);

class NearTriangulation {
private:
    int vertex_size_;
    // 頂点の次数
    // std::nullopt はまだ次数が定まっていない状態を表す。
    vector<optional<Degree>> degrees_;
    // 辺集合
    vector<pair<int, int>> edges_;
    // diagonal_vertices_[e] := 辺 e を含む三角形の頂点であって、 e の端点でない頂点
    map<pair<int, int>, vector<int>> diagonal_vertices_;

public:
    NearTriangulation(int vertex_size, const vector<set<int>> &VtoV, const vector<optional<Degree>> &degrees);

    int vertexSize(void) const;
    const vector<optional<Degree>> &degrees(void) const;
    const vector<pair<int, int>> &edges(void) const;
    const map<pair<int, int>, vector<int>> &diagonalVertices(void) const;

    void setDegree(int v, const optional<Degree> &degree);
    string debug(void) const;
};

