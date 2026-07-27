// model.h
#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include <string>
#include "geometry.h"

class Model {
private:
    std::vector<vec3> verts; // 顶点坐标
    std::vector<int> facet_vrt; // 三角形顶点索引

public:
    Model(const std::string& filename);

    int nverts() const { return verts.size(); }
    int nfaces() const { return facet_vrt.size() / 3; }

    vec3 vert(int i) const { return verts[i]; }
    vec3 vert(int iface, int nthvert) const {
        return verts[facet_vrt[iface * 3 + nthvert]];
    }
};

#endif // MODEL_H