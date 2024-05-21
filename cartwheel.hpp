#pragma once
#include <string>
#include <vector>
#include "basewheel.hpp"
#include "near_triangulation.hpp"
#include "configuration.hpp"
#include "rule.hpp"

using std::vector;
using std::string;
using std::pair;
using std::optional;

enum class Contain;
class ContainResult;
class BaseWheel;

// Wheel 
// hub とその neighbor からなるグラフ
class Wheel {
private:
    NearTriangulation wheel_;
public:
    Wheel(const NearTriangulation &wheel);
    static Wheel readWheelFile(const string &filename);
    static Wheel fromHubDegree(int hub_degree);
    void writeWheelFile(const string &filename) const;

    string toString(void) const;
    void setDegree(int v, const optional<Degree> &degree);
    const NearTriangulation &nearTriangulation(void) const;
    int numNeighbor(void) const;
};

// CartWheel
// hub とその neighbor, second neighbor, third neighbor からなるグラフ
class CartWheel {
private:
    NearTriangulation cartwheel_;
    int num_neighbor_;
    // hub の neighbor の　neighbor のうち、hub の second-neighbor を時計回りに並べたもの。
    vector<vector<int>> hub_neighbors_neighbors_;
    // hub の次数7の neighbor v について second-neighbor ( hub からは third-neighbor ) を格納しておく。
    // 具体的には u \in hub_neighbors_neighbors_[v] としたとき
    // third_neighbors[u] := u の neighbor で hub の third-neighbr を時計回りに並べたもの。
    vector<vector<int>> third_neighbors_;
public:
    CartWheel(int num_neighbor, const vector<vector<int>> &hub_neighbors_neighbors, const NearTriangulation &cartwheel);
    static CartWheel fromWheel(const Wheel &wheel);

    string toString(const vector<bool> &show_degree) const;
    string toString(void) const;
    void setDegree(int v, const optional<Degree> &degree);
    const NearTriangulation &nearTriangulation(void) const;
    int numNeighbor(void) const;
    const vector<vector<int>> &hubNeighborsNeighbors(void) const;
    const vector<vector<int>> &thirdNeighbors(void) const;

    void extendThirdNeighbor(void);
    pair<bool, vector<bool>> isOvercharged(const vector<Rule> &rules) const;
};

int chargeInitial(int degree);
void evaluateWheel(const string &wheel_filename, const string &rules_dirname, const string &send_cases_dirname, const string &confs_dirname, int max_degree);
void generateWheels(int hub_degree, const string &confs_dirname, const string &send_cases_dirname, int max_degree, const string &output_dirname);
