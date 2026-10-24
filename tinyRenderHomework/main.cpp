// main.cpp
#include <iostream>
#include <cmath>
#include <algorithm>
#include<limits>
#include<random>
#include<filesystem>
#include "model.h"
#include "tgaimage.h"
#include "geometry.h"
#include "our_gl.h"

constexpr double M_PI = 3.14159265358979323846;

const TGAColor white = {255, 255, 255, 255};
const TGAColor red = {0, 0, 255, 255};
const TGAColor green = {0, 255, 0, 255};
const TGAColor blue = {255, 0, 0, 255};

extern mat<4, 4> Viewport,ModelView, Perspective; // "OpenGL" 的状态矩阵：视口矩阵，观察矩阵，投影矩阵
extern std::vector<double> zbuffer;     // 摄像机视角 的 深度缓冲

struct BlankShader :IShader {//空着色器
    const Model& model;
    BlankShader(const Model& m) :model(m) {}

    virtual vec4 vertex(const int face, const int vert) {
        vec4 gl_Position = ModelView * model.vert(face, vert);
        return Perspective * gl_Position;
    }

    virtual std::pair<bool, TGAColor> fragment(const vec3 bar) const {
        TGAColor gl_FragColor = { 255,255,255,255 };
        return { false,gl_FragColor };
    }
};

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "用法: " << argv[0] << " <模型文件.obj>" << std::endl;
        return 1;
    }

    constexpr int width = 800;
    constexpr int height = 800;
    constexpr int shadoww = 2000;
    constexpr int shadowh = 2000;
    constexpr vec3 light = { 1,1,1 };//环境光
    constexpr vec3 eye{ -1,0,2 };//摄像机位置
    constexpr vec3 center{ 0,0,0 };//目标点
    constexpr vec3 up{ 0,1,0 };//向上方向，控制画面旋转

    
    //常规渲染通道：摄像机处 
    lookat(eye, center, up); //观察矩阵
    init_perspective(norm(eye - center)); //投影矩阵
    init_viewport(width / 16, height / 16, width * 7 / 8, height * 7 / 8); //视口矩阵
    init_zbuffer(width, height);
    TGAImage framebuffer(width, height, TGAImage::RGB, { 177, 195, 209, 255 });

    for (int m = 1; m < argc; m++) {                    
        Model model(argv[m]);
        BlankShader shader{ model };
        for (int f = 0; f < model.nfaces(); f++) {    
            Triangle clip = { shader.vertex(f, 0),
                              shader.vertex(f, 1),
                              shader.vertex(f, 2) };
            rasterize(clip, shader, framebuffer);  
        }
    }


    // 阴影渲染通道
    constexpr double ao_radius = .1;//AO采样半径
    constexpr int nsamples = 128;//采样点数量
    std::random_device rd;
    std::mt19937 gen(rd());//随机数
    std::uniform_real_distribution<double> dist(-ao_radius, ao_radius);

    auto smoothstep = [](double edge0, double edge1, double x) {         // 平滑插值函数
        double t = std::clamp((x - edge0) / (edge1 - edge0), 0., 1.);  
        return t * t * (3 - 2 * t);  // Hermite 插值：两边平滑，中间陡
        };
    
    // 屏幕空间环境光遮蔽
#pragma omp parallel for
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            double z = zbuffer[x + y * width];
            if (z < -100) continue;//背景跳过
            vec4 fragment = Viewport.invert() * vec4 { (double)x, (double)y, (double)z, 1. };//屏幕坐标->世界坐标
            double vote = 0;//被遮挡的采样点数量
            double voters = 0;//有效的采样点数量
            for (int i = 0;i < nsamples;i++) {
                vec4 p = Viewport * (fragment + vec4{ dist(gen),dist(gen),dist(gen),0. });//采样点
                if (p.x < 0 || p.x >= width || p.y < 0 || p.y >= height) continue;
                double d = zbuffer[int(p.x) + int(p.y) * width];//采样点投影到屏幕后的深度
                if (z + 5 * ao_radius < d) continue;//当前像素的深度远小于采样点投影后的深度，过
                voters++;
                vote += d > p.z;//投影到屏幕的采样点的深度 > 采样点的深度,不可见
            }
            double ssao = smoothstep(0, 1, 1 - vote / voters * .4);//计算AO强度
            //被遮挡严重(vote/voters)大,ssao小，变暗
            TGAColor c = framebuffer.get(x, y);
            framebuffer.set(x, y, { to_byte(c[0] * ssao),to_byte(c[1] * ssao),to_byte(c[2] * ssao),c[3] });
        }
    }
    // 确保输出目录存在（write_tga_file 内部用 fopen，不会自动建目录）
    std::filesystem::create_directories("output");
    framebuffer.write_tga_file("output/framebuffer.tga");

    return 0;
}