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
    vec4 l;//光照方向
    vec2 varying_uv[3];//纹理贴图坐标

    PhongShader(const Model & m,const vec3 light):model(m){
        l = normalized(ModelView * vec4{ light.x,light.y,light.z,0. });
    }

    virtual vec4 vertex(const int face, const int vert) {//第face个三角形的第vert个点
        varying_uv[vert] = model.uv(face, vert);
        vec4 gl_Position = ModelView * model.vert(face, vert);
        return Perspective * gl_Position;
    }

    virtual std::pair<bool, TGAColor>fragment(const vec3 bar) const {//bar=(alpha,beta,gamma)
        vec2 uv = varying_uv[0] * bar[0] + varying_uv[1] * bar[1] + varying_uv[2] * bar[2];//这个像素的纹理
        vec4 n = normalized(ModelView.invert_transpose() * model.normal(uv));
        vec4 r = normalized(2 * n * (n * l) - l);

        double ambient = .4;//环境光强度
        double diffuse = 1.*std::max(0., n * l);//漫反射光强度
        double specular = (3.*sample2D(model.specular(),uv)[0]/255.)*std::pow(std::max(r.z, 0.), 35);//镜面反射光强度
        TGAColor gl_FragColor = sample2D(model.diffuse(), uv);//输出颜色：从漫反射贴图获取基础颜色

        for (int channel : {0, 1, 2})
            gl_FragColor[channel] = std::min<int>(255, gl_FragColor[channel] * (ambient + diffuse + specular));
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

    for (int m = 1; m < argc; m++) { // 遍历每个输入的对象
        Model model(argv[m]);
        PhongShader shader(model,light);
        for (int f = 0; f < model.nfaces(); f++) { // 遍历所有三角形
            Triangle clip = { shader.vertex(f,0),
                shader.vertex(f,1),
                shader.vertex(f,2) };
            rasterize(clip, shader, framebuffer); // rasterize the primitive 光栅化
        }
    }
    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}