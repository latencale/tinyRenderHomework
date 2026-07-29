// main.cpp
#include <iostream>
#include <cmath>
#include <algorithm>
#include<limits>
#include "model.h"
#include "tgaimage.h"
#include "geometry.h"
#include "our_gl.h"

constexpr int width = 800;
constexpr int height = 800;
constexpr vec3 light = { 1,1,1 };//环境光
constexpr vec3 eye{ -1,0,2 };//摄像机位置
constexpr vec3 center{ 0,0,0 };//目标点
constexpr vec3 up{ 0,1,0 };//向上方向，控制画面旋转
constexpr double M_PI = 3.14159265358979323846;

const TGAColor white = {255, 255, 255, 255};
const TGAColor red = {0, 0, 255, 255};
const TGAColor green = {0, 255, 0, 255};
const TGAColor blue = {255, 0, 0, 255};

extern mat<4, 4> ModelView, Perspective; // "OpenGL" state matrices and
extern std::vector<double> zbuffer;     // the depth buffer

struct PhongShader :IShader {
    const Model& model;
    TGAColor color = {  };
    vec3 l;//光照方向
    vec3 tri[3];

    PhongShader(const Model & m,const vec3 light):model(m){
        l = normalized((ModelView * vec4{ light.x,light.y,light.z,0. }).xyz());
    }

    virtual vec4 vertex(const int face, const int vert) {
        vec3 v = model.vert(face, vert);
        vec4 gl_Position = ModelView * vec4(v.x, v.y,v.z, 1.);
        tri[vert] = gl_Position.xyz();
        return Perspective * gl_Position;
    }

    virtual std::pair<bool, TGAColor>fragment(const vec3 bar) const {
        TGAColor gl_FragColor = { 255,255,255,255 };
        
        vec3 n = normalized(cross(tri[1] - tri[0], tri[2] - tri[0]));//法线
        vec3 r = normalized(n * (n * l) * 2 - l);//反射向量
        
        double ambient = .3;//环境光强度
        double diff = std::max(0., n * l);//漫反射光强度
        double spec = std::pow(std::max(r.z, 0.), 35);//镜面反射光强度

        for (int channel : {0, 1, 2})
            gl_FragColor[channel] *= std::min(1.,ambient + .4 * diff + .9 * spec);
        return {false, gl_FragColor};
    }
};

uint8_t random_byte() {
    return static_cast<uint8_t>(std::rand() % 256);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "用法: " << argv[0] << " <模型文件.obj>" << std::endl;
        return 1;
    }
    
    lookat(eye, center, up); //观察矩阵
    init_perspective(norm(eye - center)); //投影矩阵
    init_viewport(width / 16, height / 16, width * 7 / 8, height * 7 / 8); //视口矩阵
    init_zbuffer(width, height);
    TGAImage framebuffer(width, height, TGAImage::RGB,{177,195,109,255});
    //std::vector<double> zbuffer(width * height, -std::numeric_limits<double>::max());

    for (int m = 1; m < argc; m++) { // 遍历每个输入的对象
        Model model(argv[m]);
        PhongShader shader(model,light);
        for (int f = 0; f < model.nfaces(); f++) { // 遍历所有三角形
            //shader.color = { random_byte() ,random_byte(),random_byte(),255};
            Triangle clip = { shader.vertex(f,0),shader.vertex(f,1),shader.vertex(f,2) };
            rasterize(clip, shader, framebuffer); // rasterize the primitive 光栅化
        }
    }
    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}