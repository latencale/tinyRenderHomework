// main.cpp
#include <iostream>
#include <cmath>
#include <algorithm>
#include<limits>
#include "model.h"
#include "tgaimage.h"
#include "geometry.h"

constexpr int width = 800;
constexpr int height = 800;
constexpr vec3 eye{ -1,0,2 };//摄像机位置
constexpr vec3 center{ 0,0,0 };//目标点
constexpr vec3 up{ 0,1,0 };//向上方向，控制画面旋转
constexpr double M_PI = 3.14159265358979323846;

const TGAColor white = {255, 255, 255, 255};
const TGAColor red = {0, 0, 255, 255};
const TGAColor green = {0, 255, 0, 255};
const TGAColor blue = {255, 0, 0, 255};

mat<4,4> ModelView,Viewport,Perspective;//观察矩阵，视口矩阵，投影矩阵


//旋转物体 局部坐标->世界坐标
void lookat(const vec3 eye, const vec3 center, const vec3 up) {
    vec3 n = normalized(eye - center);//前方向，-z
    vec3 l = normalized(cross(up, n));//右方向，x
    vec3 m = normalized(cross(n, l));//上方向，y
    //(l,m,n)形成 相机的本地坐标系
    ModelView = mat<4, 4>{ {{l.x,l.y,l.z,0}, {m.x,m.y,m.z,0}, {n.x,n.y,n.z,0}, {0,0,0,1}} } * //旋转：把世界轴对其到相机轴
        mat<4, 4>{{{1, 0, 0, -center.x}, { 0,1,0,-center.y }, { 0,0,1,-center.z }, { 0,0,0,1 }}};//平移到原点
}


//透视投影:实现近大远小的透视效果  观察坐标->裁剪坐标
void perspective(const double f) {//f相机的焦距：摄像机到摄影平面的距离
    Perspective = { {{1,0,0,0},
        {0,1,0,0},
        {0,0,1,0},
        {0,0, -1 / f,1}} };
}

// 视口变换（将 3D 坐标投影到屏幕） 裁剪坐标->屏幕坐标
void viewport(const int x, const int y, const int w, const int h) {
    Viewport = { {{w / 2.,0,0,x + w / 2.},//将x轴[-1,1]拉伸到宽度w，0，0，x轴平移到窗口中心
        {0,h / 2.,0,y + h / 2.},//同理，y轴
        {0,0,1,0},// 0，0，z轴不变，0
        {0,0,0,1}} };
}

//光栅化
void rasterize(const vec4 clip[3], std::vector<double>& zbuffer, TGAImage& framebuffer, const TGAColor color) {
    //裁剪空间坐标clip，
    vec4 ndc[3] = { clip[0] / clip[0].w, clip[1] / clip[1].w, clip[2] / clip[2].w };             
    //归一化设备坐标ndc
    vec2 screen[3] = { (Viewport * ndc[0]).xy(), (Viewport * ndc[1]).xy(), (Viewport * ndc[2]).xy() }; 
    //屏幕坐标

    mat<3, 3> ABC = { { {screen[0].x, screen[0].y, 1.}, {screen[1].x, screen[1].y, 1.}, {screen[2].x, screen[2].y, 1.} } };
    //记录ABC的坐标
    if (ABC.det() < 1) return; 
    //背面剔除+面积小于 1 像素的微小三角形跳过

    auto [bbminx, bbmaxx] = std::minmax({ screen[0].x, screen[1].x, screen[2].x }); //包围盒
    auto [bbminy, bbmaxy] = std::minmax({ screen[0].y, screen[1].y, screen[2].y }); // 由 最小/最大 x 和 y 决定盒子长宽

#pragma omp parallel for
    for (int x = std::max<int>(bbminx, 0); x <= std::min<int>(bbmaxx, framebuffer.width() - 1); x++) { // 裁剪，不在视野里的像素不进行遍历
        for (int y = std::max<int>(bbminy, 0); y <= std::min<int>(bbmaxy, framebuffer.height() - 1); y++) {
            vec3 bc = ABC.invert_transpose() * vec3 { static_cast<double>(x), static_cast<double>(y), 1. }; // 计算重心坐标
            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue; // 在 包围盒 外
            double z = bc * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };//深度
            if (z <= zbuffer[x + y * framebuffer.width()]) continue;//深度大于当前深度，跳过
            zbuffer[x + y * framebuffer.width()] = z;
            framebuffer.set(x, y, color);
        }
    }
}
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "用法: " << argv[0] << " <模型文件.obj>" << std::endl;
        return 1;
    }
    
    lookat(eye, center, up); //观察矩阵
    perspective(norm(eye - center)); //投影矩阵
    viewport(width / 16, height / 16, width * 7 / 8, height * 7 / 8); //视口矩阵

    TGAImage framebuffer(width, height, TGAImage::RGB);
    std::vector<double> zbuffer(width * height, -std::numeric_limits<double>::max());

    for (int m = 1; m < argc; m++) { // 遍历每个输入的对象
        Model model(argv[m]);
        for (int i = 0; i < model.nfaces(); i++) { // 遍历所有三角形
            vec4 clip[3];
            for (int d : {0, 1, 2}) {            // assemble the primitive 组装图元
                vec3 v = model.vert(i, d);
                clip[d] = Perspective * ModelView * vec4{ v.x, v.y, v.z, 1. };
            }
            TGAColor rnd;
            for (int c = 0; c < 3; c++) rnd[c] = std::rand() % 255;
            rasterize(clip, zbuffer, framebuffer, rnd); // rasterize the primitive 光栅化
        }
    }
    
    
    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}