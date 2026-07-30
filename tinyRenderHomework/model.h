// model.h
#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include <string>
#include "geometry.h"
#include "tgaimage.h"

class Model {
private:
    std::vector<vec4> verts; // 顶点坐标
    std::vector<vec4> norms;//顶点处法线坐标
    std::vector<vec2> tex;// 纹理坐标数组
    std::vector<int> facet_vrt; // 三角形顶点索引
    std::vector<int> facet_nrm;//三角形顶点法线索引
    std::vector<int> facet_tex;//三角形纹理坐标索引
    TGAImage normalmap; //法线贴图
public:
    Model(const std::string filename);

    int nverts() const;
    int nfaces() const;

    vec4 vert(int i) const; 
    vec4 vert(int iface, int nthvert) const;
    vec4 normal(int iface, int nthvert) const; //vn x,y,z
    vec4 normal(const vec2& uv) const;  //法相贴图纹理得到法相向量
    vec2 uv(const int iface, const int nrhvert) const;//三角形uv坐标
};

#endif // MODEL_H