// model.cpp
#include "model.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include<algorithm>

Model::Model(const std::string& filename) {
    std::ifstream in(filename);
    if (in.fail()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        char prefix;
        iss >> prefix;

        if (prefix == 'v') {
            // 解析顶点: v x y z
            vec3 v;
            iss >> v.x >> v.y >> v.z;
            verts.push_back(v);
        }
        else if (prefix == 'f') {
            // 解析面: f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3
            std::string token;
            int cnt = 0;

            while (iss >> token) {
                // 提取顶点索引（忽略 / 后面的纹理和法线）
                size_t pos = token.find('/');
                int idx;
				if (pos != std::string::npos) {//npos超出范围，未找到
                    idx = std::stoi(token.substr(0, pos));
                }
                else {
                    idx = std::stoi(token);
                }
                facet_vrt.push_back(idx - 1);  // OBJ 索引从 1 开始
                cnt++;
            }

            if (cnt != 3) {
                std::cerr << "错误: OBJ 文件必须包含三角形（当前面有 " << cnt << " 个顶点）" << std::endl;
                return;
            }
        }
    }

    in.close();
    std::cerr << "加载完成: " << nverts() << " 个顶点, " << nfaces() << " 个三角形" << std::endl;

    //std::vector<int> idx(nfaces());
    //for (int i = 0;i < nfaces();i++) {
    //    idx[i] = i;
    //}

    //std::sort(idx.begin(), idx.end(),
    //    [&](const int& a, const int& b) {
    //        float aminz = std::min(vert(a, 0).z, std::min(vert(a, 1).z, vert(a, 2).z));
    //        float bminz = std::min(vert(b, 0).z, std::min(vert(b, 1).z, vert(b, 2).z));
    //        return aminz < bminz;
    //    });

    //std::vector<int> facet_vrt2(nfaces() * 3); // allocate an array to store permutated facets
    //for (int i = 0; i < nfaces(); i++)           // for each (new) facet
    //    for (int j = 0; j < 3; j++)              // copy its three vertices from the old array
    //        facet_vrt2[i * 3 + j] = facet_vrt[idx[i] * 3 + j];

    //facet_vrt = facet_vrt2;                  // store the sorted triangles
}