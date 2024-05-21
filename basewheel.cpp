#include <vector>
#include <spdlog/spdlog.h>
#include <fmt/ranges.h>
#include "basewheel.hpp"

using std::make_pair;
using std::swap;

// wheelgraph (nearTriangulation) の辺番号 edgeid_wheelgraph を持つ辺 e と 
// subgraph   (nearTriangulation) の辺番号 edgeid_subgraph   を持つ辺 f を向きまで含めて対応させる。
// ただし、 except_vertices に入っている subgraph の頂点の対応は考えない。
// このとき、対応させる辺について鏡映を考えると、0-2 通りの対応がある。 0-2 通りの結果を vector に入れて返す。 
//
// detect_possible = true のとき
// + Yes: 全ての頂点の次数が適合している。 
// + Possible: すでに次数が定まっている頂点の次数は適合している。(ある subgraph の頂点に対応する wheelgraph の頂点がないとき Possible を返す。) 
// + No: ある頂点の次数が適合していない。
//
// detect_possilbe = false のとき
// + Yes: 全ての頂点の次数が適合している。
// + No: ある頂点の次数が適合していない。またはまだ次数が定まっていない頂点がある。(ある subgraph の頂点に対応する wheelgraph の頂点がないとき No を返す。) 
//
vector<ContainResult> BaseWheel::containSubgraphWithCorrespondingEdge(
    const NearTriangulation &wheelgraph, const NearTriangulation &subgraph, 
    int edgeid_wheelgraph, int edgeid_subgraph, 
    const set<int> &except_vertices, bool detect_possible) {
    // 返り値
    vector<ContainResult> res;

    // subgraph の頂点 vs と wheelgraph の頂点 vw の次数が適合しているか判定する。
    // vs が except_vertices に入っていたら true 。
    // detect_possible = true のとき、
    // + vw の次数が定まっていない時: true
    // + vs の次数が定まっていない時: true
    // + vs と vw の次数が既に定まっている時: vs の次数の範囲が vw の次数を含む <-> true
    // detect_possible = false のとき、
    // + vs と vw の次数が定まっていない時: true
    // + vw の次数が定まっていない かつ vs の次数が定まっている時: false
    // + vs の次数が定まっていない時: true
    // + vs と vw の次数が既に定まっている時: vs の次数の範囲が vw の次数を含む <-> true
    // 例 ) (vs, vw) = (5+, 5) -> ok, (6+, 5) -> ng
    auto match_degree = [&wheelgraph, &subgraph, &except_vertices](int vs, int vw, bool detect_possible) {
        if (except_vertices.count(vs)) return true;
        optional<Degree> deg_vw = wheelgraph.degrees()[vw];
        optional<Degree> deg_vs = subgraph.degrees()[vs];
        if (detect_possible) {
            if (!deg_vs.has_value()) return true;
            if (!deg_vw.has_value()) return true;
        }
        else {
            if (!deg_vs.has_value()) return true;
            if (!deg_vw.has_value()) return false;
        }
        Degree degw = deg_vw.value();
        Degree degs = deg_vs.value();
        return degs.include(degw);
    };

    // located[vs]: subgraph の頂点 vs に対応している wheelgraph の頂点
    // occupied[vw]: wheelgraph の頂点 vw に対応している subgraph の頂点
    // correspond 関数で対応させる。
    vector<int> occupied(wheelgraph.vertexSize(), -1);
    vector<int> located(subgraph.vertexSize(), -1);
    auto correspond = [&](int vs, int vw) -> void {
        occupied[vw] = vs;
        located[vs] = vw;
        return;
    };

    // wheelgraph の辺 edge_w と subgraph の辺 edge_s を向きまで含めて対応させたことによって決まった
    // それぞれの辺の diagonal_vertex (辺 e について e とある頂点 v が三角形を誘導する時 v を e の diagonal_vertex とする。) を対応させる。
    // diagonal_vertex を対応させた結果決まる辺の対応を再帰的に決める。
    auto set_edge_recursive = [&](auto &&set_edge_recursive, const pair<int, int> &edge_w, const pair<int, int> &edge_s, set<pair<int, int>> &visited_edges_w) -> bool {
        if (visited_edges_w.count(edge_w)) return true;
        visited_edges_w.insert(edge_w);
        spdlog::trace("edge_w, edge_s : {}, {}", edge_w, edge_s);
        const auto &diagonal_vertices_w = wheelgraph.diagonalVertices().at(edge_w);
        const auto &diagonal_vertices_s = subgraph.diagonalVertices().at(edge_s);
        bool match_deg = true;
        int new_match_case = 0;
        for (auto vs : diagonal_vertices_s) {
            int vs_match_case = 0;
            for (auto vw :  diagonal_vertices_w) {
                // vs と vw のどちらかまたは両方がすでに他の頂点とマッチしているとときは、これ以上対応を考えない (continueする。)
                if (!(located[vs] == -1 && occupied[vw] == -1) && !(located[vs] == vw && occupied[vw] == vs)) continue;
                if (located[vs] == -1 && occupied[vw] == -1) new_match_case++;
                ++vs_match_case;
                if (!match_degree(vs, vw, detect_possible)) {
                    match_deg = false;
                    continue;
                }
                correspond(vs, vw);
                match_deg = match_deg && set_edge_recursive(set_edge_recursive, make_pair(edge_w.first, vw), make_pair(edge_s.first, vs), visited_edges_w);
                match_deg = match_deg && set_edge_recursive(set_edge_recursive, make_pair(edge_w.second, vw), make_pair(edge_s.second, vs), visited_edges_w);
            }
            // diagonal_vertex の対応のさせ方は 1通りしかない(minimal counterexample は 4cut を持たないから1つの辺について diagoal_vertex は 2個以下 そのうち 1個は既に前の段階で対応づけられているはずだから)ので n_case <= 1
            assert(vs_match_case <= 1);
        }
        assert(new_match_case <= 1);
        return match_deg;
    };

    // res を 更新する。
    auto update_res = [&](bool match_deg) {
        if (!match_deg) {
            // 次数がマッチしない時 No
            res.emplace_back(Contain::No);
            return;
        }
        bool is_possible = false;
        for (int v = 0;v < subgraph.vertexSize(); v++) {
            // except_vertices に入っているときは、なんでもよい。
            if (except_vertices.count(v)) continue;
            // 1. 対応する頂点がない ( subgraph がはみでているとき)
            // 2. 対応する頂点があるが、次数がマッチしていないとき
            if ((located[v] == -1)
             || (located[v] != -1 && !match_degree(v, located[v], false))) {
                is_possible = true;
                break;
            }
        }
        if (is_possible) {
            if (detect_possible) res.emplace_back(Contain::Possible, occupied);
            else res.emplace_back(Contain::No);
        } else {
            // Yes
            res.emplace_back(Contain::Yes, occupied);
        }
        return;
    };

    auto edge_wheelgraph = wheelgraph.edges()[edgeid_wheelgraph];
    auto edge_subgraph = subgraph.edges()[edgeid_subgraph];
    spdlog::trace("edge_cartwheel, edge_subgraph : {}, {}", edge_wheelgraph, edge_subgraph);
    // そもそも対応させる辺で次数がマッチしていなかったら {} を返して終了
    if (!match_degree(edge_subgraph.first, edge_wheelgraph.first, detect_possible) 
     || !match_degree(edge_subgraph.second, edge_wheelgraph.second, detect_possible)) {
        return {};
    }
    correspond(edge_subgraph.first, edge_wheelgraph.first);
    correspond(edge_subgraph.second, edge_wheelgraph.second);

    const auto &diagonal_vertices_wheelgraph = wheelgraph.diagonalVertices().at(edge_wheelgraph);
    const auto &diagonal_vertices_subgraph = subgraph.diagonalVertices().at(edge_subgraph);

    // subgraph の辺 e について diagonal な位置にある頂点が 1 点, wheelgraph は 2 点だったとき、
    // subgraph の diagonal な頂点を wheelgraph の 2 点のうちどちらを固定するかで 2 通り考える。
    // それぞれの場合で、追加で 2 本の辺を固定する。
    if (diagonal_vertices_subgraph.size() == 1 && diagonal_vertices_wheelgraph.size() == 2) {
        int vs = diagonal_vertices_subgraph[0];
        auto located_tmp = located, occupied_tmp = occupied;
        for (auto vw : diagonal_vertices_wheelgraph) {
            if (!match_degree(vs, vw, detect_possible)) continue;
            correspond(vs, vw);
            spdlog::trace("vs, vw : {}, {}", vs, vw);
            set<pair<int, int>> edge_set;
            bool match_deg = true;
            match_deg = match_deg && set_edge_recursive(set_edge_recursive, make_pair(edge_wheelgraph.first, vw), make_pair(edge_subgraph.first, vs), edge_set);
            match_deg = match_deg && set_edge_recursive(set_edge_recursive, make_pair(edge_wheelgraph.second, vw), make_pair(edge_subgraph.second, vs), edge_set);
            update_res(match_deg);
            swap(occupied, occupied_tmp);
            swap(located, located_tmp);
        }
        return res;
    } 
    // subgraph の辺 e について diagonal な位置にある頂点が2点, wheelgraph は 1 点だったとき、
    // wheelgraph の diagonal な頂点を subgraph の 1 点のうちどちらを固定するかで 2 通り考える。
    // それぞれの場合で、追加で 2 本の辺を固定する。
    if (diagonal_vertices_subgraph.size() == 2 && diagonal_vertices_wheelgraph.size() == 1) {
        int vw = diagonal_vertices_wheelgraph[0];
        auto located_tmp = located, occupied_tmp = occupied;
        for (auto vs : diagonal_vertices_subgraph) {
            if (!match_degree(vs, vw, detect_possible)) continue;
            correspond(vs, vw);
            spdlog::trace("vs, vw : {}, {}", vs, vw);
            set<pair<int, int>> edge_set;
            bool match_deg = true;
            match_deg = match_deg && set_edge_recursive(set_edge_recursive, make_pair(edge_wheelgraph.first, vw), make_pair(edge_subgraph.first, vs), edge_set);
            match_deg = match_deg && set_edge_recursive(set_edge_recursive, make_pair(edge_wheelgraph.second, vw), make_pair(edge_subgraph.second, vs), edge_set);
            update_res(match_deg);
            swap(occupied, occupied_tmp);
            swap(located, located_tmp);
        }
        return res;
    } 
    // subgraph の辺 e について diagonal な位置にある頂点が 2 点, wheelgraph は 2 点だったとき、
    // subgraph の diagonal な頂点を wheelgraph の 2 点のうちどちらを固定するかで 2 通り考える。 (片方を決めるともう一方は自動的に対応が決まる。)
    // それぞれの場合で、追加で 4 本の辺を固定する。
    if (diagonal_vertices_subgraph.size() == 2 && diagonal_vertices_wheelgraph.size() == 2) {
        auto located_tmp = located, occupied_tmp = occupied;
        for (int i = 0;i < 2; i++) {
            int vs0 = diagonal_vertices_subgraph[i];
            int vs1 = diagonal_vertices_subgraph[1 - i];
            int vw0 = diagonal_vertices_wheelgraph[0];
            int vw1 = diagonal_vertices_wheelgraph[1];
            if (!match_degree(vs0, vw0, detect_possible) || !match_degree(vs1, vw1, detect_possible)) continue;
            correspond(vs0, vw0);
            correspond(vs1, vw1);
            spdlog::trace("vs0, vw0 : {}, {}", vs0, vw0);
            spdlog::trace("vs1, vw1 : {}, {}", vs1, vw1);
            set<pair<int, int>> edge_set;
            bool match_deg = true;
            match_deg = match_deg && set_edge_recursive(set_edge_recursive, make_pair(edge_wheelgraph.first, vw0), make_pair(edge_subgraph.first, vs0), edge_set);
            match_deg = match_deg && set_edge_recursive(set_edge_recursive, make_pair(edge_wheelgraph.second, vw0), make_pair(edge_subgraph.second, vs0), edge_set);
            match_deg = match_deg && set_edge_recursive(set_edge_recursive, make_pair(edge_wheelgraph.first, vw1), make_pair(edge_subgraph.first, vs1), edge_set);
            match_deg = match_deg && set_edge_recursive(set_edge_recursive, make_pair(edge_wheelgraph.second, vw1), make_pair(edge_subgraph.second, vs1), edge_set);
            update_res(match_deg);
            swap(occupied, occupied_tmp);
            swap(located, located_tmp);
        }
        return res;
    }
    // それ以外のケース (0, 0), (0, 1), (0, 2), (1, 0), (1, 1), (2, 0)
    // では1つ辺の対応を決めれば あとの対応は一意に決める。
    if (diagonal_vertices_subgraph.size() <= 2 && diagonal_vertices_wheelgraph.size() <= 2) {
        set<pair<int, int>> edge_set;
        bool match_deg = set_edge_recursive(set_edge_recursive, edge_wheelgraph, edge_subgraph, edge_set);
        update_res(match_deg);
        return res;
    }
    assert(false);
}


// wheelgraph の辺 edgeid_wheelgraph と subgraph の辺 edgeid_subgraph を向きまで含めて対応させた時に wheelgraph が含む subgraph の個数(n)を返す。(0 <= n <= 2)
// ただし、except_vertices に入っている subgraph の頂点は含まれていなくて良い。
int BaseWheel::numOfSubgraphWithCorrespondingEdge(
    const NearTriangulation &wheelgraph, const NearTriangulation &subgraph, 
    int edgeid_wheelgraph, int edgeid_subgraph, 
    const set<int> &except_vertices) {
    auto result_list = BaseWheel::containSubgraphWithCorrespondingEdge(wheelgraph, subgraph, edgeid_wheelgraph, edgeid_subgraph, except_vertices, false);
    return std::count_if(result_list.begin(), result_list.end(), [](const ContainResult &res) {
        return res.contain == Contain::Yes;
    });
}

// ring の頂点を除いて conf が　wheelgraph に含まれているか判定する。
bool BaseWheel::containConf(const NearTriangulation &wheelgraph, const Configuration &conf) {
    int edgeid_conf = conf.getInsideEdgeId();
    set<int> ring_vertices;
    if (conf.hasCutVertex()) {
        for (int v = 0;v < conf.ringSize(); v++) ring_vertices.insert(v);
    }
    for (int edgeid_wheelgraph = 0;edgeid_wheelgraph < (int)wheelgraph.edges().size(); edgeid_wheelgraph++) {
        if (BaseWheel::numOfSubgraphWithCorrespondingEdge(wheelgraph, conf.nearTriangulation(), edgeid_wheelgraph, edgeid_conf, ring_vertices) > 0) {
            return true;
        }
    }
    return false;
}

// wheelgraph が　confs に含まれる conf を含んでいるかどうか。
template <class WheelLike>
bool BaseWheel::containOneofConfs(const WheelLike &wheelgraph, const vector<Configuration> &confs) {
    spdlog::trace("wheellike graph to check : {}", wheelgraph.toString());
    int conf_idx = 0;
    for (const auto &conf : confs) {
        spdlog::trace("conf_idx : {}", conf_idx++);
        if (BaseWheel::containConf(wheelgraph.nearTriangulation(), conf)) return true;
    }
    return false;
}

// hub のチャージに影響を与える rule (指定された次数が送ってくる場合のケース) に基づいて頂点の次数を探索し、 
// 1. confs を含まない
// 2. rule による charge の授与の結果 threhold より大きい charge が hub に送られる
// ような WheelLike (CartWheel, SubCartWheel) を返す。
// 次数の候補として、 5, 6, 7, ..., max_degree+ を採用する。(例えば、max_degree = 8 のとき 5, 6, 7, 8+)
template <class WheelLike>
vector<WheelLike> BaseWheel::decideDegreeBySendCases(
    const WheelLike &wheelgraph, const vector<Rule> &rules, const vector<Configuration> &confs,
    int max_degree, int threshold, bool charge_bound) {
    int hub = 0;
    int hubdegree = wheelgraph.numNeighbor();
    vector<WheelLike> res;

    const auto &edges = wheelgraph.nearTriangulation().edges();
    vector<int> edgeids;
    edgeids.reserve(2 * hubdegree);
    // hub のチャージに影響を与える辺番号を列挙しておく。
    // 1. neighbor -> hub の辺が hubdegree 本
    for (int v = 1;v <= hubdegree; v++) {
        auto edge_receive = std::make_pair(v, hub);
        int edge_receive_id = std::find(edges.begin(), edges.end(), edge_receive) - edges.begin();
        assert(edge_receive_id != (int)edges.size());
        edgeids.push_back(edge_receive_id);
    }
    // 2. hub -> neighbor の辺が hubdegree 本
    for (int v = 1;v <= hubdegree; v++) {
        auto edge_send = std::make_pair(hub, v);
        int edge_send_id = std::find(edges.begin(), edges.end(), edge_send) - edges.begin();
        assert(edge_send_id != (int)edges.size());
        edgeids.push_back(edge_send_id);
    }

    // wheel の辺番号 edgeids[edgeids_idx] に対応する辺に沿って rule を適用することを考えたとき、
    // 新しく次数を決めて、その候補を vector に詰めて返す。
    auto decide_degree_by_rules = [&](const WheelLike &wheel, int edgeids_idx) -> pair<vector<WheelLike>, vector<int>> {
        const auto &wheel_degrees = wheel.nearTriangulation().degrees();
        vector<WheelLike> next_wheels = {wheel};
        vector<int> next_charges = {0};
        int edgeid = edgeids[edgeids_idx];
        // rule に従って次数を新しく決める。
        for (const auto &rule : rules) {
            auto result_list = BaseWheel::containSubgraphWithCorrespondingEdge(wheel.nearTriangulation(), rule.nearTriangulation(), edgeid, rule.sendEdgeId(), {}, true);
            const auto &rule_degrees = rule.nearTriangulation().degrees();
            for (const auto &result : result_list) {
                if (result.contain == Contain::No) continue;
                // result が No ではないとき、試してみた rule に従って次数を決める。
                vector<WheelLike> wheels = {wheel};
                for (int v = 0;v < wheel.nearTriangulation().vertexSize(); v++) {
                    if (result.occupied[v] != -1 && !wheel_degrees[v].has_value()) {
                        vector<Degree> degrees = divideDegree(rule_degrees[result.occupied[v]].value(), max_degree);
                        for (WheelLike &w : wheels) {
                            w.setDegree(v, degrees[0]);
                        }
                        int wheel_size = (int)wheels.size();
                        for (int di = 1;di < (int)degrees.size(); di++) {
                            for (int wi = 0;wi < wheel_size; wi++) {
                                auto w = wheels[wi];
                                w.setDegree(v, degrees[di]);
                                wheels.push_back(w);
                            }
                        }
                    }
                }
                std::copy(wheels.begin(), wheels.end(), std::back_inserter(next_wheels));
                for (int i = 0;i < (int)wheels.size(); i++) next_charges.push_back(rule.amount());
            }
        }
        return std::make_pair(next_wheels, next_charges);
    };

    // 探索する候補の cartwheel を unique にする。
    // もし unique な 2 つの cartwheel があるときは、送るチャージが大きい方を選ぶ。
    auto unique = [&](const vector<WheelLike> &next_wheels, const vector<int> &next_charges) -> pair<vector<WheelLike>, vector<int>> {
        vector<WheelLike> unique_wheels;
        vector<int> unique_charges;
        for (auto i = 0u;i < next_wheels.size(); i++) {
            // unique_wheels の中に同型なものが含まれていたら unique_wheels に追加しない。
            bool add_wheel = true;
            for (auto j = 0u;j < unique_wheels.size(); j++) {
                if (BaseWheel::isIsomorphic(next_wheels[i], unique_wheels[j])) {
                    unique_charges[j] = std::max(next_charges[i], unique_charges[j]);
                    add_wheel = false;
                    break;
                }
            }
            if (add_wheel) {
                unique_wheels.push_back(next_wheels[i]);
                unique_charges.push_back(next_charges[i]);
            }
        }
        return std::make_pair(unique_wheels, unique_charges);
    };

    // 探索を速くするために 
    // 1. 既に reducible configuration を含んでいる。
    // 2. charge_bound が true であり、かつ現時点で決まっている次数の情報から送られる charge の量が threshold 以下である。
    // のどちらかの条件を満たす cartwheel を既に探索しない。
    auto prune = [&](const vector<WheelLike> &next_wheels, const vector<int> &next_charges, int edgeids_idx, const vector<int> &decided_charges) -> pair<vector<WheelLike>, vector<int>> {
        vector<WheelLike> pruned_wheels;
        vector<int> pruned_charges;
        for (int i = 0;i < (int)next_wheels.size(); i++) {
            const WheelLike &w = next_wheels[i];
            if (charge_bound) {
                // この先の探索でどんな次数の組み合わせであったとしても charge が閾値を超えない時に探索をやめる。
                int send_lower = 0, receive_upper = 0;
                vector<int> expected_charge(edgeids.size(), 0);
                bool stop_search = false;
                for (int ei = 0;ei < (int)edgeids.size(); ei++) {
                    int max_send_l = 0;
                    int max_send_u = 0;
                    for (const auto &rule : rules) {
                        int s = edges[edgeids[ei]].first;
                        int t = edges[edgeids[ei]].second;
                        auto [send_l, send_u, send_related] = BaseWheel::amountChargeToSend(w, s, t, rule);
                        max_send_l = std::max(max_send_l, send_l > 0 ? rule.amount() : 0); // rule が2回適用されるときでも、1回の適用しか考えない。2回の適用は別の rule で見ているのと max をとっているので大丈夫。
                        max_send_u = std::max(max_send_u, send_u > 0 ? rule.amount() : 0);
                    }
                    if (ei < hubdegree) {
                        // 1. neighbor -> hub
                        if (ei == edgeids_idx) {
                            if (max_send_l > next_charges[i]) {
                                stop_search = true; // 指定されたチャージよりも多く送っている場合は、他のケースで探索が行われているので探索をしなくてよい。
                                break;
                            }
                            expected_charge[ei] = next_charges[i];
                        } else if (ei < edgeids_idx) {
                            if (max_send_l > decided_charges[ei]) {
                                stop_search = true; // 指定されたチャージよりも多く送っている場合は、他のケースで探索が行われているので探索をしなくてよい。
                                break;
                            }
                            expected_charge[ei] = decided_charges[ei];
                        } else {
                            expected_charge[ei] = max_send_u;
                        }
                        receive_upper += expected_charge[ei];
                    } else {
                        // 2. hub -> neighbor
                        // こちらでは枝刈りを、行わない。
                        expected_charge[ei] = max_send_l;
                        send_lower += expected_charge[ei];
                    }
                }
                if (stop_search) continue;
                spdlog::trace("cartwheel : {}", w.toString());
                spdlog::trace("expected_charges : {}", fmt::join(expected_charge, ", "));
                int charge = receive_upper - send_lower;
                if (charge <= threshold) continue;
            }
            if (BaseWheel::containOneofConfs(w, confs)) continue; // conf を含んでいたらその時点で探索をやめる。
            pruned_wheels.push_back(next_wheels[i]);
            pruned_charges.push_back(next_charges[i]);
        }
        return std::make_pair(pruned_wheels, pruned_charges);
    };
    
    // decide_degree_by_rule, unique,  prune を順に適用することで、
    // cartwheel の次数を決めていき、すべて次数を決めたら、配列 res に詰めて返す。
    auto decide_degree = [&](auto &&decide_degree, const WheelLike &wheel, int edgeids_idx, vector<int> &decided_charges) -> void {
        if (edgeids_idx == (int)edgeids.size()) {
            res.push_back(wheel);
            return;
        }
        spdlog::trace("cartwheel : {}", wheel.toString());
        spdlog::trace("decided_charges : {}", fmt::join(decided_charges, ", "));

        // _wheels は cartwheel
        // _charges は辺番号 edgeids[edgeids_idx] を持つ辺に従って送られる charge の量を表す。
        auto [next_wheels, next_charges] = decide_degree_by_rules(wheel, edgeids_idx);
        auto [unique_wheels, unique_charges] = unique(next_wheels, next_charges);
        auto [pruned_wheels, pruned_charges] = prune(unique_wheels, unique_charges, edgeids_idx, decided_charges);
       
        spdlog::trace("next_wheels.size : {}", pruned_wheels.size());
        spdlog::trace("next_charges : {}", fmt::join(pruned_charges, ", "));
        assert(pruned_wheels.size() == pruned_charges.size());
        for (int i = 0;i < (int)pruned_wheels.size(); i++) {
            if (edgeids_idx < hubdegree) decided_charges.push_back(pruned_charges[i]);
            decide_degree(decide_degree, pruned_wheels[i], edgeids_idx + 1, decided_charges);
            if (edgeids_idx < hubdegree) decided_charges.pop_back();
        }
        return;
    };
    vector<int> decided_charges;
    decided_charges.reserve(hubdegree);
    decide_degree(decide_degree, wheelgraph, 0, decided_charges); 
    return res;
}

// 頂点番号 index 以上の頂点の　degree を possible_degrees の中から選んで決めた wheelgraph のうち、 
// confs に含まれる　conf を含まない　wheelgraph の集合を返す。
template <class WheelLike>
vector<WheelLike> BaseWheel::searchNoConfGraphs(
    const WheelLike &wheelgraph, int index, const vector<Degree> &possible_degrees, 
    const vector<Configuration> &confs) {
    vector<WheelLike> wheelgraphs;
    auto base_wheelgraph = wheelgraph;
    if (BaseWheel::containOneofConfs(base_wheelgraph, confs)) return {};
    int vertex_size = base_wheelgraph.nearTriangulation().vertexSize();
    
    // 次数を決めて、 conf を含まない cartwheel を探索する。
    auto set_degree_recursive = [&](auto &&set_degree_recursive, int v, WheelLike &temp_wheelgraph) -> void {
        if (v % 5 == 0) {
            // 5つ次数を決めるごとに conf を含んでいるか確認
            if (BaseWheel::containOneofConfs(temp_wheelgraph, confs)) return;
        }
        if (v == vertex_size) {
            if (!BaseWheel::containOneofConfs(temp_wheelgraph, confs)) {
                wheelgraphs.push_back(temp_wheelgraph);
            }
            return;
        }
        for (const auto &degree : possible_degrees) {
            temp_wheelgraph.setDegree(v, degree);
            set_degree_recursive(set_degree_recursive, v + 1, temp_wheelgraph);
        }
        temp_wheelgraph.setDegree(v, std::nullopt);
        return;
    };
    set_degree_recursive(set_degree_recursive, index, base_wheelgraph);
    return wheelgraphs;
}

// wheel1 と wheel2 が同型かどうか判定する。
template <class WheelLike>
bool BaseWheel::isIsomorphic(const WheelLike &wheel1, const WheelLike &wheel2) {
    for (int ei = 0;ei < (int)wheel2.nearTriangulation().edges().size(); ei++) {
        if (BaseWheel::numOfSubgraphWithCorrespondingEdge(wheel1.nearTriangulation(), wheel2.nearTriangulation(), 0, ei) > 0
         && BaseWheel::numOfSubgraphWithCorrespondingEdge(wheel2.nearTriangulation(), wheel1.nearTriangulation(), ei, 0) > 0) {
            return true;
        }
    }
    return false;
}

template <class WheelLike>
void BaseWheel::makeUnique(vector<WheelLike> &wheels) {
    vector<WheelLike> unique_wheels;
    vector<int> unique_wheel_idxes;
    for (auto i = 0u;i < wheels.size(); i++) {
        // unique_subwheels の中に　subwheel と同型なものが含まれていたら subwheel は unique_subwheels に追加しない。
        bool add_wheel = true;
        for (auto j = 0u;j < unique_wheels.size(); j++) {
            if (BaseWheel::isIsomorphic(wheels[i], unique_wheels[j])) {
                add_wheel = false;
                break;
            }
        }
        if (add_wheel) {
            unique_wheels.push_back(wheels[i]);
            unique_wheel_idxes.push_back(i);
        }
    }
    wheels = unique_wheels;
    return;
}

// (i) wheel の頂点 from から to へ rule を適用した時にどれだけ charge が流れるかの下限
// (ii) wheel の頂点 from から to へ rule を適用した時にどれだけ charge が流れるかの上限
// (iii) wheel の頂点でルールを送るのに関係しているかどうかを表す bool 配列。
// を返す。
template <class WheelLike>
tuple<int, int, vector<bool>> BaseWheel::amountChargeToSend(const WheelLike &wheel, int from, int to, const Rule &rule) {
    auto is_symmetric = [](const vector<ContainResult> &result_list) {
        // No のときはsymmetricであることを検出する必要はないことと
        // symmetric であるとき結果は一致していることから、片方でも No なら false を返す。
        if (result_list.size() == 2 && result_list[0].contain != Contain::No && result_list[1].contain != Contain::No) {
            int vertex_size = result_list[0].occupied.size();
            // どの頂点についても、「subgraph の頂点に対応しているか、していないかのフラグ」が一致していれば symmetric である。
            for (int i = 0;i < vertex_size; i++) {
                if ((result_list[0].occupied[i] != -1) ^ (result_list[1].occupied[i] != -1)) {
                    return false;
                }
            }
            // symmetric であるとき結果は一致している。
            assert(result_list[0].contain == result_list[1].contain);
            return true;
        } else {
            return false;
        }
    };
    auto edge = std::make_pair(from, to);
    const auto &edges = wheel.nearTriangulation().edges();
    int edgeid = std::find(edges.begin(), edges.end(), edge) - edges.begin();
    assert(edgeid != (int)edges.size());
    auto result_list = BaseWheel::containSubgraphWithCorrespondingEdge(wheel.nearTriangulation(), rule.nearTriangulation(), edgeid, rule.sendEdgeId(), {}, true);
    int lower = 0, upper = 0;
    vector<bool> is_related(wheel.nearTriangulation().vertexSize(), false);
    assert(result_list.size() <= 2);
    if (is_symmetric(result_list)) {
        result_list.pop_back();
    }
    for (const auto &res : result_list) {
        if (res.contain == Contain::Yes) {
            lower ++;
            upper ++;
            for (int v = 0;v < wheel.nearTriangulation().vertexSize(); v++) {
                is_related[v] = is_related[v] || (res.occupied[v] != -1);
            }
        } else if (res.contain == Contain::Possible) {
            upper++;
        }
    }
    return make_tuple(lower * rule.amount(), upper * rule.amount(), is_related);
}

// WheelLike には Wheel, CartWheel, SubWheel, SubCartWheel などの型を代入しうる
template bool BaseWheel::containOneofConfs<Wheel>(
    const Wheel &wheelgraph, const vector<Configuration> &confs);
template bool BaseWheel::containOneofConfs<CartWheel>(
    const CartWheel &wheelgraph, const vector<Configuration> &confs);

template vector<Wheel> BaseWheel::searchNoConfGraphs<Wheel>(
    const Wheel &wheelgraph, int index, const vector<Degree> &possible_degrees, 
    const vector<Configuration> &confs);
template vector<CartWheel> BaseWheel::searchNoConfGraphs<CartWheel>(
    const CartWheel &wheelgraph, int index, const vector<Degree> &possible_degrees, 
    const vector<Configuration> &confs);

template bool BaseWheel::isIsomorphic(const Wheel &wheel1, const Wheel &wheel2);
template bool BaseWheel::isIsomorphic(const CartWheel &wheel1, const CartWheel &wheel2);

template void BaseWheel::makeUnique(vector<Wheel> &wheels);
template void BaseWheel::makeUnique(vector<CartWheel> &wheels);

template tuple<int, int, vector<bool>> BaseWheel::amountChargeToSend(const Wheel &wheel, int from, int to, const Rule &rule);
template tuple<int, int, vector<bool>> BaseWheel::amountChargeToSend(const CartWheel &wheel, int from, int to, const Rule &rule);

template vector<CartWheel> BaseWheel::decideDegreeBySendCases(const CartWheel &wheel, const vector<Rule> &rules, const vector<Configuration> &confs, int max_degree, int threshold, bool charge_bound);
