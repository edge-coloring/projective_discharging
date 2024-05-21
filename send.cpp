#include <string>
#include <iostream>
#include <cassert>
#include <utility>
#include <set>
#include <filesystem>
#include <boost/program_options.hpp>
#include <spdlog/spdlog.h>
#include <fmt/core.h>
#include "configuration.hpp"
#include "rule.hpp"
#include "near_triangulation.hpp"
#include "cartwheel.hpp"

namespace fs = std::filesystem;
using std::string;
using std::set;

template <class WheelLike>
vector<WheelLike> makeUnique(const vector<WheelLike> &wheels, int edgeid) {
    vector<WheelLike> unique_wheels;
    for (auto &wheel : wheels) {
        // edgeid の辺を対応させたときに unique_wheels の中に wheel と同型なものが含まれていたら wheel は unique_wheel に追加しない。
        bool add_wheel = true;
        for (auto &unique_wheel : unique_wheels) {
            if (BaseWheel::numOfSubgraphWithCorrespondingEdge(wheel.nearTriangulation(), unique_wheel.nearTriangulation(), edgeid, edgeid) > 0 &&
                BaseWheel::numOfSubgraphWithCorrespondingEdge(unique_wheel.nearTriangulation(), wheel.nearTriangulation(), edgeid, edgeid) > 0) {
                add_wheel = false;
                break;
            }
        }
        if (add_wheel) {
            unique_wheels.push_back(wheel);
        }
    }
    return unique_wheels;
}

// bidirectional = true のとき
// + 与えられた rules に従って send_vertex から receive_vertex に charge を送る時の周辺の頂点の次数の状況を列挙する。
// + ただし、 confs に含まれている configuration が現れている場合は除く。
// bidirectional = false のとき
// + 与えられた rules で send_vertex と receive_vertex の間で charge を送り合う次数の状況を列挙する。
// + ただし、 confs に含まれている configuration が現れている場合は除く。
vector<CartWheel> decideDegree(const CartWheel &cartwheel, const vector<Degree> &degrees, 
    const vector<Configuration> &confs, const vector<Rule> &rules, int send_vertex, int receive_vertex, int max_degree, bool bidirectional) {
    vector<CartWheel> res;
    set<string> res_strs;

    const auto &edges = cartwheel.nearTriangulation().edges();
    vector<int> edgeids;

    // send_vertex から receive_vertex への辺を加える。
    int edgeid = std::find(edges.begin(), edges.end(), std::make_pair(send_vertex, receive_vertex)) - edges.begin();
    assert(edgeid != (int)edges.size());
    edgeids.push_back(edgeid);

    // receive_vertex から send_vertex への辺を加える。
    if (bidirectional) {
        int revedgeid = std::find(edges.begin(), edges.end(), std::make_pair(receive_vertex, send_vertex)) - edges.begin();
        assert(revedgeid != (int)edges.size());
        edgeids.push_back(revedgeid);
    }

    // rules に従って次数を決める。
    auto decide_degree_by_rules = [&](const CartWheel& wheel) -> vector<CartWheel> {
        const auto &wheel_degrees = wheel.nearTriangulation().degrees();
        vector<CartWheel> next_wheels;
        for (const auto &rule : rules) {
            for (int edgeid : edgeids) {
                auto result_list = BaseWheel::containSubgraphWithCorrespondingEdge(wheel.nearTriangulation(), rule.nearTriangulation(), edgeid, rule.sendEdgeId(), {}, true);
                const auto &rule_degrees = rule.nearTriangulation().degrees();
                for (const auto &result : result_list) {
                    if (result.contain != Contain::Possible) continue;
                    // Possible (含まれる可能性がある) が Yes になるように次数を決める。
                    // ただし、third neighbor の頂点を付け足さないと Yes にならないときがあるので、
                    // 次数を1つも決めなかった時は新しく探索を行わない。
                    bool decide_degree = false;
                    vector<CartWheel> wheels = {wheel};
                    for (int v = 0;v < wheel.nearTriangulation().vertexSize(); v++) {
                        if (result.occupied[v] != -1 && !wheel_degrees[v].has_value()) {
                            decide_degree = true;
                            vector<Degree> degrees = divideDegree(rule_degrees[result.occupied[v]].value(), max_degree);

                            for (CartWheel &w : wheels) {
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
                    if (!decide_degree) continue;
                    std::copy(wheels.begin(), wheels.end(), std::back_inserter(next_wheels));
                }
            }
        }
        return next_wheels;
    };

    auto decide_degree = [&](auto &&decide_degree, const CartWheel &wheel) -> void {
        string wheel_str = wheel.toString();
        if (res_strs.count(wheel_str)) return;
        res.push_back(wheel);
        res_strs.insert(wheel_str);

        // rule に従って次数を決める。
        vector<CartWheel> next_wheels = decide_degree_by_rules(wheel);
        spdlog::trace("candidate next_wheel.size : {}", next_wheels.size());

        // unique にする。
        makeUnique(next_wheels, edgeids[0]);
        spdlog::trace("unique_wheel.size : {}", next_wheels.size());

        // configuration を含んでいる cartwheel を除く。
        vector<CartWheel> temp;
        std::copy_if(next_wheels.begin(), next_wheels.end(), std::back_inserter(temp), [&](const CartWheel &w) {
            return !BaseWheel::containOneofConfs(w, confs);
        });
        std::swap(temp, next_wheels);
        
        spdlog::trace("next_wheel.size : {}", next_wheels.size());
        for (const auto& next_wheel : next_wheels) {
            decide_degree(decide_degree, next_wheel);
        }
        return;
    };
    decide_degree(decide_degree, cartwheel);

    return res;
}

// cartwheel の send_vertex と receive_vertex を繋ぐ辺に沿って rules に従って charge を送るときに、
// 1. send_vertex から receive_vertex に送る charge の量。
// 2. + bidirectional = true なら receive_vertex から send_vertex に送る charge の量。
//    + bidirectional = false なら 0。
// 3. 適用される rule に関連しているかどうかを表す頂点ごとの bool 値。
// をまとめて計算する。
std::tuple<int, int, vector<bool>> getRelatedVertices(CartWheel &cw, int send_vertex, int receive_vertex, const vector<Rule> &rules, bool bidirectional) {
    int send_charge = 0;
    int receive_charge = 0;
    vector<bool> is_related(cw.nearTriangulation().vertexSize(), false);
    for (const Rule &rule : rules) {
        auto [send_l, send_u, send_related] = BaseWheel::amountChargeToSend(cw, send_vertex, receive_vertex, rule);
        send_charge += send_l;
        for (auto v = 0u;v < is_related.size(); v++) {
            is_related[v] = is_related[v] || send_related[v];
        }
        
        if (bidirectional) {
            auto [receive_l, receive_u, receive_related] = BaseWheel::amountChargeToSend(cw, receive_vertex, send_vertex, rule);
            receive_charge += receive_l;
            for (auto v = 0u;v < is_related.size(); v++) {
                is_related[v] = is_related[v] || receive_related[v];
            }
        }
    }

    return std::make_tuple(send_charge, receive_charge, is_related);
}


// is_related = true であるような cartwheel の頂点からなる nearTriangulation の (vertexSize, VtoV, degrees) を計算する。
// つまり、ルールに関係ない頂点を取り除いた nearTriangulation を計算する。
std::tuple<int, vector<set<int>>, vector<optional<Degree>>> generateNearTriangulation(const CartWheel &cartwheel, int send_vertex, int receive_vertex, const vector<bool> &is_related) {
    const auto &original_degrees = cartwheel.nearTriangulation().degrees();

    vector<int> new_vid(cartwheel.nearTriangulation().vertexSize(), -1);
    int vertex_size = 0;
    vector<optional<Degree>> degrees;
    for (int v = 0;v < cartwheel.nearTriangulation().vertexSize(); v++) {
        if (is_related[v]) {
            new_vid[v] = vertex_size++;
            degrees.push_back(original_degrees[v]);
        }
    }
    // send_vertex = 0, receive_vertex = 1 で頂点番号を付け替えても変わらない。
    assert(send_vertex == 0 && new_vid[send_vertex] == 0 && 
        receive_vertex == 1 && new_vid[receive_vertex] == 1);

    vector<set<int>> VtoV(vertex_size);
    for (const auto &edge : cartwheel.nearTriangulation().edges()) {
        if (new_vid[edge.first] != -1 && new_vid[edge.second] != -1) {
            VtoV[new_vid[edge.first]].insert(new_vid[edge.second]);
            VtoV[new_vid[edge.second]].insert(new_vid[edge.first]);
        }
    }

    return std::make_tuple(vertex_size, VtoV, degrees);
}

// cartwheel の情報 (頂点の次数、 charge の量など) を出力する。
// outdir が指定されているのなら、cartwheel の情報を rule ファイルにして、出力する。
void output(const NearTriangulation &cw_neartriangulation, int send_vertex, int receive_vertex, const Degree &send_degree, const Degree &receive_degree,
    int send_charge, int receive_charge, bool bidirectional, int &count, const string &outdir) {
    if (bidirectional) {
        // 互いに送りあうケースを表示する。
        if (send_charge > 0 && receive_charge > 0) {
            spdlog::info("send_charge : {}, receive_charge : {}", send_charge, receive_charge);
            spdlog::info("rule (for machine) :\n{}", cw_neartriangulation.debug());
        }
    } else {
        if (send_charge > 0) {
            spdlog::info("charge : {}", send_charge);
            spdlog::info("rule (for machine) :\n{}", cw_neartriangulation.debug());

            // output
            if (outdir != "") {
                int vertex_size = cw_neartriangulation.vertexSize();
                // VtoV
                vector<set<int>> VtoV(vertex_size);
                for (const auto &e : cw_neartriangulation.edges()) {
                    VtoV[e.first].insert(e.second);
                    VtoV[e.second].insert(e.first);
                }
                vector<optional<Degree>> degrees = cw_neartriangulation.degrees();

                string res = fmt::format("from {} to {} amount {}\n", send_degree.toString(), receive_degree.toString(), send_charge);
                res += fmt::format("{} {} {} {}\n", vertex_size, send_vertex+1, receive_vertex+1, send_charge);
                for (int v = 0;v < vertex_size; v++) {
                    res += fmt::format("{} {} ", v+1, degrees[v].value().toString());
                    for (int u : VtoV[v]) {
                        res += fmt::format("{} ", u+1);
                    }
                    res += "\n";
                }
                string filename = fmt::format("{}/from{}to{}_{:05d}.rule", outdir, send_degree.toString(), receive_degree.toString(), count);
                std::ofstream ofs(filename);
                if (!ofs) {
                    spdlog::warn("The directory {} does not exist", outdir);
                    exit(1);
                }
                ofs << res;
            }
            count++;
        }
    }
    return;
}

void enumerate(const Degree &send_degree, const Degree &receive_degree, 
    const string &confs_dirname, const string &rules_dirname, int max_degree, bool bidirectional, const string &outdir) {
    auto confs = getConfs(confs_dirname);
    auto rules = getRules(rules_dirname);

    vector<Degree> possible_degrees;
    for (int deg = 5;deg < max_degree; deg++) possible_degrees.push_back(Degree(deg));
    possible_degrees.push_back(Degree(max_degree, MAX_DEGREE));

    // wheel の生成
    Wheel wheel = Wheel::fromHubDegree(send_degree.lower());
    int send_vertex = 0, receive_vertex = 1;
    wheel.setDegree(receive_vertex, receive_degree);
    spdlog::info("calculating wheel which does not contain conf...");
    auto wheels = BaseWheel::searchNoConfGraphs(wheel, 2, possible_degrees, confs);
    
    // wheel を unique にする。
    spdlog::info("calculating unique wheel...");
    auto edge = std::make_pair(send_vertex, receive_vertex);
    int edgeid = -1;
    {
        const auto &wheel_edges = wheel.nearTriangulation().edges();
        edgeid = std::find(wheel_edges.begin(), wheel_edges.end(), edge) - wheel_edges.begin();
        assert(edgeid != (int)wheel_edges.size());
    }
    spdlog::info("take only unique wheel");
    auto unique_wheels = makeUnique(wheels, edgeid);

    // 第 2 近傍までの次数を決める。
    spdlog::info("deciding degree...");
    vector<CartWheel> cartwheels;
    for (const Wheel &w : unique_wheels) {
        auto cartwheels_from_w = decideDegree(CartWheel::fromWheel(w), possible_degrees, confs, rules, send_vertex, receive_vertex, max_degree, bidirectional);
        cartwheels.insert(cartwheels.end(), cartwheels_from_w.begin(), cartwheels_from_w.end());
    }

    // 第 3 近傍まで拡張する。
    spdlog::info("extending third neighbor...");
    std::transform(cartwheels.begin(), cartwheels.end(), cartwheels.begin(), [&max_degree](CartWheel &cartwheel){
        const auto &degrees = cartwheel.nearTriangulation().degrees();
        // second-neighbor で次数の定まっていない頂点は次数を max_degree+ にする。
        for (int v = 0;v < cartwheel.nearTriangulation().vertexSize(); v++) {
            if (!degrees[v].has_value()) cartwheel.setDegree(v, Degree(max_degree, MAX_DEGREE));
        }
        cartwheel.extendThirdNeighbor();
        return cartwheel;
    });
    spdlog::info("deciding degree of third neighbor...");

    // 第 3 近傍の次数を決める。
    vector<CartWheel> thirdneighbor_cartwheels;
    for (const auto& cartwheel : cartwheels) {
        auto cartwheels = decideDegree(cartwheel, possible_degrees, confs, rules, send_vertex, receive_vertex, max_degree, bidirectional);
        thirdneighbor_cartwheels.insert(thirdneighbor_cartwheels.end(), cartwheels.begin(), cartwheels.end());
    }

    vector<NearTriangulation> unique_cartwheels;
    vector<int> edgeids;
    int count = 0;
    for (CartWheel &cw : thirdneighbor_cartwheels) {
        // ルールの適用に関係がある頂点を特定し、
        // 同時に charge の計算もする。
        auto [send_charge, receive_charge, is_related] = getRelatedVertices(cw, send_vertex, receive_vertex, rules, bidirectional);
        // charge が全く送られていないなら continue
        if (send_charge == 0 && receive_charge == 0) continue;
        auto [vertex_size, VtoV, degrees] = generateNearTriangulation(cw, send_vertex, receive_vertex, is_related);
        NearTriangulation cw_neartriangulation = NearTriangulation(vertex_size, VtoV, degrees);

        // unique 判定
        const auto &cw_edges = cw_neartriangulation.edges();
        int cw_edgeid = std::find(cw_edges.begin(), cw_edges.end(), std::make_pair(send_vertex, receive_vertex)) - cw_edges.begin();
        assert(cw_edgeid != (int)cw_edges.size());
        bool unique = true;
        for (auto i = 0u;i < unique_cartwheels.size(); i++) {
            if (BaseWheel::numOfSubgraphWithCorrespondingEdge(unique_cartwheels[i], cw_neartriangulation, edgeids[i], cw_edgeid) > 0 
             && BaseWheel::numOfSubgraphWithCorrespondingEdge(cw_neartriangulation, unique_cartwheels[i], cw_edgeid, edgeids[i]) > 0) {
                unique = false;
                break;
            }
        }
        if (!unique) {
            continue;
        }
        unique_cartwheels.push_back(cw_neartriangulation);
        edgeids.push_back(cw_edgeid);
        assert(edgeid == cw_edgeid);

        // print
        output(cw_neartriangulation, send_vertex, receive_vertex, send_degree, receive_degree, send_charge, receive_charge, bidirectional, count, outdir);
    }
    spdlog::info("There are {} case that degree {} sends charge to degree {}", count, send_degree.toString(), receive_degree.toString());

    return;
}


int main(const int ac, const char* const* const av) {
    using namespace boost::program_options;
    options_description description("Options");
    description.add_options()
        ("from,f", value<string>(), "degree of vertex that sends charge")
        ("to,t", value<string>(), "degree of vertex that receives charge")
        ("conf,c", value<string>(), "The directory which includes configuration files")
        ("rule,r", value<string>(), "The directory which includes rule files")
        ("max_degree,m", value<int>(), "Maximum degree to check (if you choose degree from {5, 6, 7, 8+}), set max_degree 8")
        ("bidirectional,b", "Detect cases that we apply both \"to -> from\", \"from -> to\" rules")
        ("outdir,o", value<string>()->default_value(""), "The directory which outputs rule file that represents vertex sends charge. if you do not specify thie parameter, output is nothing")
        ("help,H", "Display options")
        ("verbosity,v", value<int>()->default_value(0), "1 for debug, 2 for trace");

    variables_map vm;
    store(parse_command_line(ac, av, description), vm);
    notify(vm);

    if (vm.count("help")) {
        description.print(std::cout);
        return 0;
    }
    if (vm.count("verbosity")) {
        auto v = vm["verbosity"].as<int>();
        if (v == 1) {
            spdlog::set_level(spdlog::level::debug);
        }
        if (v == 2) {
            spdlog::set_level(spdlog::level::trace);
        }
    }
    if (vm.count("from") && vm.count("to")) {
        Degree send_degree = Degree::fromString(vm["from"].as<string>());
        Degree receive_degree = Degree::fromString(vm["to"].as<string>());
        if (!send_degree.fixed()) {
            spdlog::warn("degree of vertex that sends charge must be fixed value");
            exit(1);
        }
        if (!vm.count("rule")) {
            spdlog::warn("Specify directory which includes rule files");
            exit(1);
        }
        if (!vm.count("conf")) {
            spdlog::warn("Specify directory which includes configuration files");
            exit(1);
        }
        if (!vm.count("max_degree")) {
            spdlog::warn("Specify max_degree");
            exit(1);
        }
        string confdir = vm["conf"].as<string>();
        string ruledir = vm["rule"].as<string>();
        int max_degree = vm["max_degree"].as<int>();
        bool bidirectional = vm.count("bidirectional");
        string outdir = vm["outdir"].as<string>();
        if (outdir != "" && !fs::exists(outdir)) {
            spdlog::warn("The directory {} does not exist", outdir);
            exit(1);
        }
        enumerate(send_degree, receive_degree, confdir, ruledir, max_degree, bidirectional, outdir);
    } else {
        spdlog::warn("Please specify degree of vertex");
    }
    

    return 0;
}