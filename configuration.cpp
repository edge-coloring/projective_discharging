#include <fstream>
#include <filesystem>
#include <spdlog/spdlog.h>
#include "configuration.hpp"

namespace fs = std::filesystem;

bool confHasCutVertex(int vertex_size, int ring_size, const vector<set<int>> &VtoV) {
    bool has_cutvertex = false;
    int ord = 0;
    vector<int> num(vertex_size, -1);
    vector<int> low(vertex_size, -1);
    auto dfs = [&](auto &&dfs, int v, int par) -> void {
        num[v] = ord++;
        low[v] = num[v];
        int n_child = 0;
        for (int u : VtoV[v]) {
            if (u == par) continue;
            if (u < ring_size) continue;
            if (num[u] != -1) {
                low[v] = std::min(low[v], num[u]);
                continue;
            }
            n_child++;
            dfs(dfs, u, v);
            low[v] = std::min(low[v], low[u]);
            if (par != -1 && num[v] <= low[u]) has_cutvertex = true;
        }
        if (par == -1 && n_child >= 2) has_cutvertex = true;
        return;
    };
    dfs(dfs, ring_size, -1);
    spdlog::trace("num : {}", fmt::join(num, ", "));
    spdlog::trace("low :{}", fmt::join(low, ", "));
    return has_cutvertex;
}


Configuration::Configuration(int ring_size, bool has_cutvertex, const string &filename, const NearTriangulation &conf) :
    conf_(conf), ring_size_(ring_size), has_cutvertex_(has_cutvertex), filename_(filename) {
    inside_edge_id_ = 0;
    if (has_cutvertex) {
        // カット点を持っているときは ring の頂点も含むから、inside_edge_id を計算する必要がある。
        // カット点を持っていない時は ring の頂点を含まないから、全ての辺が ring ではない頂点を結ぶ辺だから inside_edge_id = 0 で良い。
        for (const auto &edge : conf_.edges()) {
            auto [u, v] = edge;
            if (u >= ring_size_ && v >= ring_size_) {
                break;
            }
            inside_edge_id_++;
        }
    }
    assert(inside_edge_id_ < (int)conf_.edges().size());
};

Configuration Configuration::readConfFile(const string &filename) {
    std::ifstream ifs(filename);
    if (!ifs) {
        spdlog::critical("Failed to open {} ", filename);
        throw std::runtime_error("Failed to open" + filename);
    }
    string dummy;
    std::getline(ifs, dummy);
    int vertex_size, ring_size;
    ifs >> vertex_size >> ring_size;

    vector<set<int>> VtoV(vertex_size);
    vector<optional<Degree>> degrees(vertex_size, std::nullopt);

    for (int vi = 0;vi < ring_size; vi++) {
        int vip = (vi + 1) % ring_size;
        VtoV[vi].insert(vip);
        VtoV[vip].insert(vi);
    }
    for (int vi = ring_size;vi < vertex_size; vi++) {
        int v, degv;
        ifs >> v >> degv;
        --v;
        assert(v == vi);
        degrees[v] = Degree(degv);
        for (int i = 0;i < degv; i++) {
            int nv;
            ifs >> nv;
            --nv;
            assert(0 <= nv && nv < vertex_size);
            VtoV[v].insert(nv);
            VtoV[nv].insert(v);
        }
    }

    if (confHasCutVertex(vertex_size, ring_size, VtoV)) {
        spdlog::trace("has cut vertex");
        return Configuration(ring_size, true, filename, NearTriangulation(vertex_size, VtoV, degrees));
    }
    spdlog::trace("has no cut vertex");

    // delete ring and add - to degree of ring incident vertex
    vector<set<int>> VtoV2(vertex_size - ring_size);
    for (int v = ring_size;v < vertex_size; v++) {
        bool is_incident_ring = false;
        for (auto u : VtoV[v]) {
            if (u < ring_size) {
                is_incident_ring = true;
                continue;
            }
            VtoV2[v - ring_size].insert(u - ring_size);
        }
        int n_adj = (int)VtoV2[v - ring_size].size();
        // primal で
        // (i) 3 本の辺が ring に出ている頂点が configuration に含まれているなら、その頂点の次数を -1 しても reducible である。
        // を使う。
        int deg = degrees[v].value().upper();
        if (is_incident_ring && deg - n_adj == 3) {
            degrees[v] = Degree(std::max(deg - 1, MIN_DEGREE), deg);
        }
    }
    degrees.erase(degrees.begin(), degrees.begin() + ring_size);
    vertex_size -= ring_size;
    
    return Configuration(ring_size, false, filename, NearTriangulation(vertex_size, VtoV2, degrees));
}

const NearTriangulation &Configuration::nearTriangulation(void) const {
    return conf_;
}

int Configuration::ringSize(void) const {
    return ring_size_;
}

// 両端点がどちらも ring にない辺を一つ返す。
int Configuration::getInsideEdgeId(void) const {
    return inside_edge_id_;
}

bool Configuration::hasCutVertex(void) const {
    return has_cutvertex_;
}

const string &Configuration::fileName(void) const {
    return filename_;
}


// configuration の直径を計算する。
// 注意: ring の頂点を通るパスは考えない。
int Configuration::diameter(void) const {
    int vertex_size = conf_.vertexSize();
    int offset = has_cutvertex_ ? ring_size_ : 0;
    vector<vector<int>> dist(vertex_size - offset, vector<int>(vertex_size - offset, 10000));
    for (int v = offset;v < vertex_size; v++) {
        dist[v - offset][v - offset] = 0;
    }
    for (const auto &e: conf_.edges()) {
        if (e.first - offset < 0 || e.second - offset < 0) continue;
        dist[e.first - offset][e.second - offset] = 1;
    }
    for (int k = 0;k < vertex_size - offset; k++) {
        for (int i = 0;i < vertex_size - offset; i++) {
            for (int j = 0;j < vertex_size - offset; j++) {
                dist[i][j] = std::min(dist[i][j], dist[i][k] + dist[k][j]);
            }
        }
    }
    int diam = 0;
    for (int i = 0;i < vertex_size - offset; i++) {
        for (int j = 0;j < vertex_size - offset; j++) {
            diam = std::max(diam, dist[i][j]);
        }
    }
    return diam;
}

// ディレクトリに含まれる　conf ファイルの configuration を返す。
vector<Configuration> getConfs(const std::string &dirname) {
    vector<Configuration> confs;
    spdlog::info("reading confs from {} ...", dirname);
    for (const fs::directory_entry &file : fs::directory_iterator(dirname)) {
        if (file.is_regular_file() && file.path().extension().string<char>() == ".conf") {
            spdlog::trace("reading {}", file.path().string<char>());
            confs.push_back(Configuration::readConfFile(file.path().string<char>()));
        } 
    }
    return confs;
}