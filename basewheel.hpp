#pragma once
#include <vector>
#include <set>
#include "configuration.hpp"
#include "near_triangulation.hpp"
#include "cartwheel.hpp"
#include "rule.hpp"
using std::vector;

enum class Contain {
    // 含む
    Yes, 
    // 現時点ではわからないが、今後の次数の定め方次第で含む可能性がある。
    Possible,
    // 含まない
    No
};

class ContainResult {
public:
    Contain contain;
    // contain が Yes や Possible だったときに、
    // occupied[v] := v (v は含む側の頂点の番号) に対応する頂点番号 w　(w は含まれる側の頂点の番号)
    vector<int> occupied;
    ContainResult(Contain contain, const vector<int> &occupied = vector<int>()): contain(contain), occupied(occupied) {};
};

// Wheel グラフ全般に共通して使う関数
class BaseWheel {
public:
    static vector<ContainResult> containSubgraphWithCorrespondingEdge(
        const NearTriangulation &wheelgraph, const NearTriangulation &subgraph, 
        int edgeid_wheelgraph, int edgeid_subgraph, 
        const set<int> &except_vertices, bool detect_possible);

    static int numOfSubgraphWithCorrespondingEdge(
        const NearTriangulation &wheelgraph, const NearTriangulation &subgraph,
        int edgeid_wheelgraph, int edgeid_subgraph, 
        const set<int> &except_vertices = set<int>());

    static bool containConf(const NearTriangulation &wheelgraph, const Configuration &conf);

    template <class WheelLike>
    static bool containOneofConfs(const WheelLike &wheelgraph, const vector<Configuration> &confs);

    template <class WheelLike>
    static vector<WheelLike> decideDegreeBySendCases(
        const WheelLike &wheelgraph, const vector<Rule> &rules, const vector<Configuration> &confs, int max_degree, int threshold, bool charge_bound = false);

    template <class WheelLike>
    static vector<WheelLike> searchNoConfGraphs(
        const WheelLike &wheelgraph, int index, const vector<Degree> &possible_degrees, const vector<Configuration> &confs);

    template <class WheelLike> static bool isIsomorphic(const WheelLike &wheel1, const WheelLike &wheel2);
    template <class WheelLike> static void makeUnique(vector<WheelLike> &wheels);
    
    template <class WheelLike>
    static tuple<int, int, vector<bool>> amountChargeToSend(const WheelLike &wheel, int from, int to, const Rule &rule);
};