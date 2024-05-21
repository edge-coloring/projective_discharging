#include <vector>
#include <fstream>
#include <set>
#include <sstream>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <boost/algorithm/string.hpp>
#include "rule.hpp"

using std::getline;
namespace fs = std::filesystem;

Rule::Rule(int from, int to, int amount, const NearTriangulation &rule) :
    rule_(rule),
    amount_(amount) {
    auto send_edge = std::make_pair(from, to);
    const auto &edges = rule.edges();
    send_edgeid_ = std::find(edges.begin(), edges.end(), send_edge) - edges.begin();
    assert(send_edgeid_ != (int)edges.size());
}

Rule Rule::readRuleFile(const string &filename) {
    std::ifstream ifs(filename);
    if (!ifs) {
        spdlog::critical("Failed to open {} ", filename);
        throw std::runtime_error("Failed to open" + filename);
    }
    string dummy;
    getline(ifs, dummy);
    int vertex_size, from, to, amount;
    ifs >> vertex_size >> from >> to >> amount;
    --from, --to;

    vector<set<int>> VtoV(vertex_size);
    vector<optional<Degree>> degrees(vertex_size, std::nullopt);

    for (int vi = 0;vi < vertex_size; vi++) {
        int v;
        string degv_str;
        ifs >> v >> degv_str;
        --v;
        assert(v == vi);
        degrees[v] = Degree::fromString(degv_str);

        string str;
        getline(ifs, str);
        boost::trim(str);
        std::stringstream ss(str);
        string u_str;
        while (getline(ss, u_str, ' ')) {
            int u = std::stoi(u_str);
            --u;
            assert(0 <= u && u < vertex_size);
            VtoV[v].insert(u);
            VtoV[u].insert(v);
        }
    }
    assert(VtoV[from].count(to));

    return Rule(from, to, amount, NearTriangulation(vertex_size, VtoV, degrees));
}

const NearTriangulation &Rule::nearTriangulation(void) const {
    return rule_;
}

int Rule::sendEdgeId(void) const {
    return send_edgeid_;
}

int Rule::amount(void) const {
    return amount_;
}

// ディレクトリに含まれる　rule ファイルの rule を返す。
vector<Rule> getRules(const std::string &dirname) {
    vector<Rule> rules;
    spdlog::info("reading rules from {} ...", dirname);
    for (const fs::directory_entry &file : fs::directory_iterator(dirname)) {
        if (file.is_regular_file() && file.path().extension().string<char>() == ".rule") {
            spdlog::trace("reading {}", file.path().string<char>());
            rules.push_back(Rule::readRuleFile(file.path().string<char>()));
        } 
    }
    return rules;
}