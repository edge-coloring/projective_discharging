#include <fstream>
#include <numeric>
#include <cassert>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <fmt/ranges.h>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include "cartwheel.hpp"

using std::make_pair;
using std::swap;
using boost::algorithm::split;
namespace fs = std::filesystem;

Wheel::Wheel(const NearTriangulation &wheel) : wheel_(wheel) {}

Wheel Wheel::readWheelFile(const string &filename) {
    std::ifstream ifs(filename);
    if (!ifs) {
        spdlog::critical("Failed to open {}", filename);
        throw std::runtime_error("Failed to open" + filename);
    }
    int hub = 0;
    int hub_degree;
    ifs >> hub_degree;

    vector<set<int>> VtoV(hub_degree + 1);
    vector<optional<Degree>> degrees(hub_degree + 1, std::nullopt);
    degrees[hub] = Degree(hub_degree);
    for (int v = 1;v <= hub_degree; v++) {
        string vdeg_str;
        ifs >> vdeg_str;
        degrees[v] = Degree::fromString(vdeg_str);

        int u = (v == hub_degree ? 1 : v + 1);
        VtoV[v].insert(u);
        VtoV[u].insert(v);
        VtoV[hub].insert(v);
        VtoV[v].insert(hub);
    }

    return Wheel(NearTriangulation(hub_degree + 1, VtoV, degrees));
}

// hub_degree を指定してその他の次数はまだ決まっていない Wheel を返す。
Wheel Wheel::fromHubDegree(int hub_degree) {
    int hub = 0;
    vector<set<int>> VtoV(hub_degree + 1);
    vector<optional<Degree>> degrees(hub_degree + 1, std::nullopt);
    degrees[hub] = Degree(hub_degree);
    for (int v = 1;v <= hub_degree; v++) {
        int u = (v == hub_degree ? 1 : v + 1);
        VtoV[v].insert(u);
        VtoV[u].insert(v);
        VtoV[hub].insert(v);
        VtoV[v].insert(hub);
    }
    return Wheel(NearTriangulation(hub_degree + 1, VtoV, degrees));
}

void Wheel::writeWheelFile(const string &filename) const {
    std::ofstream ofs(filename);
    ofs << toString() << std::endl;
    return;
}

string Wheel::toString(void) const {
    int hub_degree = numNeighbor();
    const auto &degrees = wheel_.degrees();
    string res = std::to_string(hub_degree);

    auto deg_str = [&degrees](int u) -> string {
        return degrees[u].has_value() ? degrees[u].value().toString() : "?";
    };

    for (int v = 1;v <= hub_degree; v++) {
        res += " " + deg_str(v);
    }

    return res;
}

void Wheel::setDegree(int v, const optional<Degree> &degree) {
    wheel_.setDegree(v, degree);
    return;
}

const NearTriangulation &Wheel::nearTriangulation(void) const {
    return wheel_;
}

int Wheel::numNeighbor(void) const {
    int hub = 0;
    const auto &degrees = wheel_.degrees();
    assert(degrees[hub].has_value() && degrees[hub].value().fixed());
    return degrees[hub].value().lower();
}

CartWheel::CartWheel(int num_neighbor, const vector<vector<int>> &hub_neighbors_neighbors, const NearTriangulation &cartwheel) : 
    cartwheel_(cartwheel),
    num_neighbor_(num_neighbor),
    hub_neighbors_neighbors_(hub_neighbors_neighbors),
    third_neighbors_(vector<vector<int>>(cartwheel.vertexSize())) {}

// wheel を受け取って hub の second neighbor の次数はまだ決まっていない CartWheel を返す。
CartWheel CartWheel::fromWheel(const Wheel &wheel) {
    int hub = 0;
    int hub_degree = wheel.numNeighbor();
    vector<set<int>> VtoV(hub_degree + 1);
    vector<optional<Degree>> degrees = wheel.nearTriangulation().degrees();
    vector<vector<int>> hub_neighbors_neighbors(hub_degree + 1);

    auto add_edge = [&VtoV](int v, int u) -> void {
        VtoV[v].insert(u);
        VtoV[u].insert(v);
        return;
    };

    auto new_vertex = [&VtoV, &degrees]() -> int {
        int v = (int)VtoV.size();
        VtoV.push_back({});
        degrees.push_back(std::nullopt);
        return v;
    };

    vector<int> second_neighbors(hub_degree + 1, 0);
    // second_neighbors[v] := v (v は hub の neighbor) と v+1 どちらにも隣接している頂点の番号
    for (int v = 1;v <= hub_degree; v++) {
        int u = (v == hub_degree ? 1 : v + 1);
        add_edge(v, u);
        add_edge(hub, v);
        if (!degrees[v].value().fixed() && !degrees[u].value().fixed()) {
            // u も v も次数が固定されていない(8+など)のとき、
            // second_neighbor を作る必要がない。
            continue;
        }
        int w = new_vertex();
        add_edge(v, w);
        add_edge(u, w);
        second_neighbors[v] = w;
    }

    for (int v = 1;v <= hub_degree; v++) {
        auto degv = degrees[v].value();
        if (!degv.fixed()) continue;
        int u = (v == 1 ? hub_degree : v - 1);
        int first = second_neighbors[u];
        int last = second_neighbors[v];
        hub_neighbors_neighbors[v].push_back(first);
        // v (v は hub の neighbor) の次数に合わせて新しい頂点を追加する。
        for (int count = 0;count < degv.lower() - 5; count++) {
            int w = new_vertex();
            add_edge(v, w);
            add_edge(first, w);
            first = w;
            hub_neighbors_neighbors[v].push_back(w);
        }
        hub_neighbors_neighbors[v].push_back(last);
        add_edge(first, last);
    }
    
    int vertex_size = (int)VtoV.size();
    return CartWheel(hub_degree, hub_neighbors_neighbors, NearTriangulation(vertex_size, VtoV, degrees));
}


// machine_readable な形で cartwheel を str にする。
// show_degree[v] = true であるような頂点のみ次数を表示する。
string CartWheel::toString(const vector<bool> &show_degree) const {
    // int hub_degree = numNeighbor();
    const auto &degrees = cartwheel_.degrees();
    auto deg_str = [&degrees, &show_degree](int u) -> string {
        return degrees[u].has_value() && show_degree[u] ? degrees[u].value().toString() : "?";
    };

    // N E deg0 deg1 .. deg{N-1} u0 v0 u1 v1 .. u{N-1} v{N-1}
    const auto &edges = nearTriangulation().edges();
    int N = nearTriangulation().vertexSize();
    string res = std::to_string(N) + " " + std::to_string(edges.size()) + " ";
    for (int v = 0;v < N; v++) {
        res += deg_str(v) + " ";
    }
    for (auto [u, v]: nearTriangulation().edges()) {
        res += std::to_string(u) + " " + std::to_string(v) + " ";
    }
    return res;
}


// machine_readable な形で cartwheel を str にする。
string CartWheel::toString(void) const {
    vector<bool> show_degree(cartwheel_.vertexSize(), true);
    return toString(show_degree);
}

void CartWheel::setDegree(int v, const optional<Degree> &degree) {
    cartwheel_.setDegree(v, degree);
    return;
}

const NearTriangulation &CartWheel::nearTriangulation(void) const {
    return cartwheel_;
}

// hub の neighbor の数 (cartwheel の場合は、hubの次数と同じ)
int CartWheel::numNeighbor(void) const {
    return num_neighbor_;
}

const vector<vector<int>> &CartWheel::hubNeighborsNeighbors(void) const {
    return hub_neighbors_neighbors_;
}

const vector<vector<int>> &CartWheel::thirdNeighbors(void) const {
    return third_neighbors_;
}

// hub の third-neighbor を構築する。
void CartWheel::extendThirdNeighbor(void) {
    int vertex_size = nearTriangulation().vertexSize();
    vector<set<int>> VtoV(vertex_size);
    vector<optional<Degree>> degrees = nearTriangulation().degrees();
    vector<vector<int>> third_neighbors(vertex_size);

    auto add_edge = [&VtoV](int v, int u) -> void {
        VtoV[v].insert(u);
        VtoV[u].insert(v);
        return;
    };

    auto new_vertex = [&VtoV, &degrees, &third_neighbors]() -> int {
        int v = (int)VtoV.size();
        VtoV.push_back({});
        degrees.push_back(std::nullopt);
        third_neighbors.push_back({});
        return v;
    };

    auto get_degree = [degrees](int v) -> Degree {
        assert(degrees[v].has_value());
        return degrees[v].value();
    };

    // second-neighbor までの VtoV を計算する。
    const auto &edges = nearTriangulation().edges();
    const auto &diagonal_vertices = nearTriangulation().diagonalVertices();
    for (const auto& edge : edges) {
        for (auto v : diagonal_vertices.at(edge)) {
            add_edge(v, edge.first);
            add_edge(v, edge.second);
            add_edge(edge.first, edge.second);
        }
    }

    int hubdegree = numNeighbor();
    vector<int> circuit; // nearTriangulation の circuit を構築
    for (int v = 1;v <= hubdegree; v++) {
        Degree degv = get_degree(v);
        if (!degv.fixed()) {
            circuit.push_back(v);
        } else {
            int v_neighbor_size = (int)hub_neighbors_neighbors_[v].size();
            for (int v_neighbor_idx = 0;v_neighbor_idx < v_neighbor_size - 1; v_neighbor_idx++) {
                circuit.push_back(hub_neighbors_neighbors_[v][v_neighbor_idx]);
            }
            int v_after = (v == hubdegree ? 1 : v + 1);
            Degree degv_after = get_degree(v_after);
            if (!degv_after.fixed()) {
                circuit.push_back(hub_neighbors_neighbors_[v].back());
            }
        }
    }

    vector<int> circuit_neighbor(vertex_size, -1);
    Degree deg0 = get_degree(circuit[0]);
    // circuit_neighbor[i] := circuit[i], circuit[i+1] に隣接する頂点
    for (int cidx = 0;cidx < (int)circuit.size(); cidx++) {
        int v = circuit[cidx];
        int u = (cidx == (int)circuit.size() - 1 ? circuit[0] : circuit[cidx + 1]);
        Degree degv = get_degree(v);
        Degree degu = get_degree(u);
        // コーナーケース
        if (cidx == (int)circuit.size() - 2
            && degu.fixed() && (int)VtoV[u].size() == degu.lower() - 1
            && deg0.fixed() && (int)VtoV[circuit[0]].size() == deg0.lower()) {
            circuit_neighbor[v] = circuit_neighbor[circuit[0]];
            add_edge(v, circuit_neighbor[v]);
            add_edge(u, circuit_neighbor[v]);
            continue;
        }
        if (degv.fixed() && (int)VtoV[v].size() == degv.lower()) {
            assert(cidx > 0);
            circuit_neighbor[v] = circuit_neighbor[circuit[cidx - 1]];
            add_edge(u, circuit_neighbor[v]);
            continue;
        }
        if (degu.fixed() && (int)VtoV[u].size() == degu.lower()) {
            assert(cidx == (int)circuit.size() - 1);
            circuit_neighbor[v] = circuit_neighbor[circuit[0]];
            add_edge(v, circuit_neighbor[v]);
            continue;
        }
        if (!degv.fixed() && !degu.fixed()) {
            // u も v も次数が固定されていない(8+など)のとき、
            // circiut_neighbor を作る必要がない。
            continue;
        }
        int w = new_vertex();
        circuit_neighbor[v] = w;
        add_edge(u, circuit_neighbor[v]);
        add_edge(v, circuit_neighbor[v]);
    }

    for (int cidx = 0;cidx < (int)circuit.size(); cidx++) {
        int v = circuit[cidx];
        Degree degv = get_degree(v);
        if (!degv.fixed()) continue;
        int u = (cidx == 0 ? circuit.back() : circuit[cidx - 1]);
        int first = circuit_neighbor[u];
        int last = circuit_neighbor[v];
        third_neighbors[v].push_back(first);
        if (first == last) continue;
        int num_vertex = degv.lower() - (int)VtoV[v].size();
        assert(num_vertex >= 0);
        for (int count = 0;count < num_vertex; count++) {
            int w = new_vertex();
            add_edge(first, w);
            add_edge(v, w);
            third_neighbors[v].push_back(w);
            first = w;
        }
        third_neighbors[v].push_back(last);
        add_edge(first, last);
    }

    // メンバを更新
    cartwheel_ = NearTriangulation(VtoV.size(), VtoV, degrees);
    third_neighbors_ = third_neighbors;
    return;
}

// (i) hub が rules に従って近傍から charge を送受した結果 0 を超えたかどうかの判定結果
// (ii) cartwheel の頂点でルールを送るのに関係しているかどうかを表す bool 配列。
//　を返す。
pair<bool, vector<bool>> CartWheel::isOvercharged(const vector<Rule> &rules) const {
    int hub = 0;
    int hub_degree = numNeighbor();
    int charge_receive = 0, charge_send = 0;
    const auto &degrees = cartwheel_.degrees();
    vector<bool> is_rule_related(cartwheel_.vertexSize(), false);
    vector<pair<string, int>> degree_charge_of_neighbors(hub_degree, make_pair("", 0));
    for (int hub_neighbor = 1;hub_neighbor <= hub_degree; hub_neighbor++) {
        for (const auto &rule : rules) {
            // 受け取る量
            auto [receive_lower, receive_upper, receive_related] = BaseWheel::amountChargeToSend(*this, hub_neighbor, hub, rule);
            // 送る量
            auto [send_lower, send_upper, send_related] = BaseWheel::amountChargeToSend(*this, hub, hub_neighbor, rule);
            assert(receive_lower == receive_upper && send_lower == send_upper); 
            charge_receive += receive_lower;
            charge_send += send_lower;
            for (int i = 0;i < cartwheel_.vertexSize(); i++) {
                is_rule_related[i] = is_rule_related[i] || receive_related[i] || send_related[i];
            }
            degree_charge_of_neighbors[hub_neighbor - 1].second += receive_lower;
        }
        assert(degrees[hub_neighbor - 1].has_value());
        degree_charge_of_neighbors[hub_neighbor - 1].first = degrees[hub_neighbor].value().toString();
    }
    spdlog::debug("charges receive: {}", fmt::join(degree_charge_of_neighbors, ", "));
    int charge_initial = chargeInitial(hub_degree);
    int charge = charge_initial + charge_receive - charge_send;
    spdlog::debug("cartwheel : {}", toString());
    spdlog::debug("charge (initial, receive, send, result) : {}, {}, {}, {}", charge_initial, charge_receive, charge_send, charge);
    return make_pair(charge > 0, is_rule_related);
}

int chargeInitial(int degree) {
    return 10 * (6 - degree);
}

// degree がまだ定まっていない頂点の　degree を 5, 6, ..., max_degree+ の中から選んで決めた cartwheel のうち、 
// (i) reducible_confs に含まれる conf を含まない 
// (ii) rule による charge の授与の結果 hub が 0 より大きい charge を持つようになる。
// ような cartwheel を出力する。
void searchOverChargedCartWheel(
    const Wheel &wheel, const vector<Rule> &rules, const vector<Rule> &send_cases,
    const vector<Configuration> &reducible_confs, int max_degree) {
    auto base_cartwheel = CartWheel::fromWheel(wheel);
    int threshold = -chargeInitial(base_cartwheel.numNeighbor());

    auto possible_cartwheels_within_secondneighbor = BaseWheel::decideDegreeBySendCases(base_cartwheel, send_cases, reducible_confs, max_degree, threshold, true);
    spdlog::info("extending third neighbors...");
    std::transform(possible_cartwheels_within_secondneighbor.begin(), possible_cartwheels_within_secondneighbor.end(), possible_cartwheels_within_secondneighbor.begin(), [&max_degree](CartWheel &cartwheel){
        const auto &degrees = cartwheel.nearTriangulation().degrees();
        // second-neighbor で次数の定まっていない頂点は次数を max_degree+ にする。
        for (int v = 0;v < cartwheel.nearTriangulation().vertexSize(); v++) {
            if (!degrees[v].has_value()) cartwheel.setDegree(v, Degree(max_degree, MAX_DEGREE));
        }
        cartwheel.extendThirdNeighbor();
        return cartwheel;
    });
    // decideThirdNeighborDegreeByRulesでルールに影響のある third-neighbor の次数を決める(そのような頂点の次数の組み合わせしか探索する必要がない)。
    vector<CartWheel> possible_cartwheels;
    for (const auto& cartwheel : possible_cartwheels_within_secondneighbor) {
        auto cartwheels = BaseWheel::decideDegreeBySendCases(cartwheel, send_cases, reducible_confs, max_degree, threshold, true);
        possible_cartwheels.insert(possible_cartwheels.end(), cartwheels.begin(), cartwheels.end());
    }
    std::transform(possible_cartwheels.begin(), possible_cartwheels.end(), possible_cartwheels.begin(), [&max_degree](CartWheel &cartwheel){
        const auto &degrees = cartwheel.nearTriangulation().degrees();
        // third-neighbor で次数の定まっていない頂点は次数を max_degree+ にする。
        for (int v = 0;v < cartwheel.nearTriangulation().vertexSize(); v++) {
            if (!degrees[v].has_value()) cartwheel.setDegree(v, Degree(max_degree, MAX_DEGREE));
        }
        return cartwheel;
    });
    BaseWheel::makeUnique(possible_cartwheels);
    spdlog::info("number of cartwheel to check : {}", possible_cartwheels.size());
    int num_overcharged = 0;
    int idx_cartwheel = 0;
    for (const auto &cartwheel : possible_cartwheels) {
        spdlog::debug("checking cartwheel [{}/{}]", idx_cartwheel, possible_cartwheels.size());
        auto [is_ovecharged, is_related] = cartwheel.isOvercharged(rules);
        if (is_ovecharged) {
            spdlog::info("overcharged cartwheel (for machine) : {}", cartwheel.toString(is_related));
            num_overcharged ++;
        }
        idx_cartwheel ++;
    }
    spdlog::info("the ratio of overcharged cartwheel {}/{}", num_overcharged, possible_cartwheels.size());
    return;
}

void evaluateWheel(const string &wheel_filename, const string &rules_dirname, const string &send_cases_dirname, const string &confs_dirname, int max_degree) {
    spdlog::debug("reading {}", wheel_filename);
    Wheel wheel = Wheel::readWheelFile(wheel_filename);
    vector<Rule> rules = getRules(rules_dirname);
    vector<Rule> send_cases = getRules(send_cases_dirname);
    vector<Configuration> confs = getConfs(confs_dirname);
    spdlog::info("start evaluating {}", wheel_filename);
    searchOverChargedCartWheel(wheel, rules, send_cases, confs, max_degree);
    return;
}

vector<Wheel> searchPossibleOverChargedWheels(int hubdegree, const vector<Degree> &possible_degrees, 
    const vector<Configuration> &confs, const vector<Rule> &send_cases) {
    Wheel base_wheel = Wheel::fromHubDegree(hubdegree);
    vector<Wheel> res;
    // decide degree and generate wheel that is unique up to rotationaly symmetry
    vector<int> temp_degree_idx(hubdegree, -1);
    auto decide_degree = [&](auto &&decide_degree, int v, int lowerst_deg_idx) -> void {
        // decide v-th neighbor's degree
        if (v == hubdegree) {
            // lexicographical order
            auto original_order = temp_degree_idx;
            bool original_is_min = true;
            for (int i = 0;i < hubdegree; i++) {
                rotate(temp_degree_idx.begin(), temp_degree_idx.begin() + 1, temp_degree_idx.end());
                if (temp_degree_idx < original_order) {
                    original_is_min = false;
                    break;
                }
            }
            swap(original_order, temp_degree_idx);
            if (!original_is_min) {
                return;
            }
            for (int i = 0;i < hubdegree; i++) {
                base_wheel.setDegree(i + 1, possible_degrees[temp_degree_idx[i]]);
            }
            if (BaseWheel::containOneofConfs(base_wheel, confs)) return;
            // remove clearly not overcharged wheel
            int recv = 0;
            for (int neighbor = 1;neighbor <= hubdegree; neighbor++) {
                int max_recv_u = 0;
                for (const auto &send_case : send_cases) {
                    auto [_tmp0, recv_u, _tmp1] =  BaseWheel::amountChargeToSend(base_wheel, neighbor, 0, send_case);
                    max_recv_u = std::max(max_recv_u, recv_u > 0 ? send_case.amount() : 0); // rule が2回適用されるときでも、1回の適用しか考えない。2回の適用は別の rule で見ているのと max をとっているので大丈夫。
                }
                recv += max_recv_u;
            }
            if (chargeInitial(hubdegree) + recv <= 0) return;
            res.push_back(base_wheel);
            return;
        }
        for (int i = lowerst_deg_idx;i < (int)possible_degrees.size(); i++) {
            temp_degree_idx[v] = i;
            decide_degree(decide_degree, v + 1, lowerst_deg_idx);
            temp_degree_idx[v] = -1;
        }
    };
    for (int deg_idx = 0;deg_idx < (int)possible_degrees.size();deg_idx++ ) {
        // degree of 0-th neighbor
        temp_degree_idx[0] = deg_idx;
        decide_degree(decide_degree, 1, deg_idx);
        temp_degree_idx[0] = -1;
    }
    return res;
}

// hub の次数が hub_degree で confs を含まない wheel のファイルを output_dirname　ディレクトリに出力する。
void generateWheels(int hub_degree, const string &confs_dirname, const string &send_cases_dirname, int max_degree, const string &output_dirname) {
    vector<Degree> possible_degrees;
    for (int deg = 5; deg < max_degree; deg++) possible_degrees.push_back(Degree(deg));
    possible_degrees.push_back(Degree(max_degree, MAX_DEGREE));

    vector<Configuration> confs = getConfs(confs_dirname);
    vector<Rule> send_cases = getRules(send_cases_dirname);

    // 高速化のため confs の中で直径が 2 以下のもののみ考える。
    vector<Configuration> confs_filetered;
    std::copy_if(confs.begin(), confs.end(), std::back_inserter(confs_filetered), [](const Configuration &conf){
        return conf.diameter() <= 2;
    });

    spdlog::info("calculating wheel which does not contain conf...");
    auto wheels = searchPossibleOverChargedWheels(hub_degree, possible_degrees, confs, send_cases);
    
    spdlog::info("output wheel file into wheel directory");
    bool madedir = fs::create_directory(output_dirname);
    if (madedir) spdlog::info("made {} directory", output_dirname);
    int count = 0;
    for (auto &wheel : wheels) {
        wheel.writeWheelFile(fmt::format("{}/{}_{}.wheel", output_dirname, hub_degree, count++));
    }
    return;
}


