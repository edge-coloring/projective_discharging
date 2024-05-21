#pragma once
#include <string>
#include "near_triangulation.hpp"

using std::string;

class Configuration {
private:
    NearTriangulation conf_;
    int ring_size_;
    int inside_edge_id_;
    bool has_cutvertex_;
    string filename_;
    
public:
    Configuration(int ring_size, bool has_cutvertex, const string &filename, const NearTriangulation &conf);
    static Configuration readConfFile(const string &filename);

    const NearTriangulation &nearTriangulation(void) const;
    int ringSize(void) const;
    bool hasCutVertex(void) const;
    const string &fileName(void) const;
    int diameter(void) const;
    
    int getInsideEdgeId(void) const;
};

vector<Configuration> getConfs(const std::string &dirname);

