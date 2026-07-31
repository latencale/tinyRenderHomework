// main.cpp
#include <iostream>
#include <cmath>
#include <algorithm>
#include<limits>
#include<random>
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

//struct PhongShader :IShader {
//    const Model& model;
//    vec4 l;//光照方向
//    vec2 varying_uv[3];//纹理贴图坐标
//    vec4 varying_nrm[3];//三角形的法向量
//    vec4 tri[3];//三角形坐标点

//    PhongShader(const Model & m,const vec3 light):model(m){
//        l = normalized(ModelView * vec4{ light.x,light.y,light.z,0. });
//    }
//
//    virtual vec4 vertex(const int face, const int vert) {//第face个三角形的第vert个点
//        varying_uv[vert] = model.uv(face, vert);
//        varying_nrm[vert] = ModelView.invert_transpose() * model.normal(face, vert);
//        vec4 gl_Position = ModelView * model.vert(face, vert);
//        tri[vert] = gl_Position;
//        //分别得到uv,法线,三角形在观察空间的坐标
//        return Perspective * gl_Position;
//    }
//
//    virtual std::pair<bool, TGAColor>fragment(const vec3 bar) const {//bar=(alpha,beta,gamma)
//        //将法线贴图中的法线从切线空间转换到世界空间
//        mat<2, 4> E = { tri[1] - tri[0],tri[2] - tri[0] };//三角形的两条边（世界空间）
//        mat<2, 2> U = { varying_uv[1] - varying_uv[0],varying_uv[2] - varying_uv[0] };//三角形的两条边（uv空间）
//        mat<2, 4> T = U.invert() * E;
//        mat<4, 4> D = { normalized(T[0]),//切线向量(tangent)
//                    normalized(T[1]),//副切线向量(bitangent)
//                    normalized(varying_nrm[0] * bar[0] + varying_nrm[1] * bar[1] + varying_nrm[2] * bar[2]),//法线
//                    {0,0,0,1} };
//
//        vec2 uv = varying_uv[0] * bar[0] + varying_uv[1] * bar[1] + varying_uv[2] * bar[2];//这个像素的纹理坐标
//        vec4 n = normalized(D.transpose() * model.normal(uv));//法线
//        vec4 r = normalized(2 * n * (n * l) - l);//反射光
//
//        double ambient = .4;//环境光强度
//        double diffuse = 1.*std::max(0., n * l);//漫反射光强度
//        double specular = (3.*sample2D(model.specular(),uv)[0]/255.)*std::pow(std::max(r.z, 0.), 35);//镜面反射光强度
//        TGAColor gl_FragColor = sample2D(model.diffuse(), uv);//输出颜色：从漫反射贴图获取基础颜色
//
//        for (int channel : {0, 1, 2})
//            gl_FragColor[channel] = std::min<int>(255, gl_FragColor[channel] * (ambient + diffuse + specular));
//        return {false, gl_FragColor};
//    }
//};
struct EmptyShader :IShader {//空着色器
    const Model& model;
    EmptyShader(const Model& m) :model(m) {}

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
    constexpr int shadoww = 1000;
    constexpr int shadowh = 1000;
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
        EmptyShader shader{ model };
        for (int f = 0; f < model.nfaces(); f++) {    
            Triangle clip = { shader.vertex(f, 0),
                              shader.vertex(f, 1),
                              shader.vertex(f, 2) };
            rasterize(clip, shader, framebuffer);  
        }
    }
    framebuffer.write_tga_file("framebuffer.tga");


    // 阴影渲染通道
    std::vector<double> mask(width * height, 0);//遮罩图
    std::vector<double> zbuffer_copy = zbuffer;//赋值深度缓冲
    mat<4, 4> M = (Viewport * Perspective * ModelView).invert();//MVP逆矩阵：屏幕坐标到世界坐标
    mat<4, 4> N = Viewport * Perspective * ModelView;

    std::mt19937 gen(std::random_device{}());//随机数生成器
    std::uniform_real_distribution<double> dist(0.0, 1.0);//生成[0,1)的随机数
    constexpr int n = 10;

    auto smoothstep = [](double edge0, double edge1, double x) {         // 平滑插值函数
        double t = std::clamp((x - edge0) / (edge1 - edge0), 0., 1.);  
        return t * t * (3 - 2 * t);  // Hermite 插值：两边平滑，中间陡
        };

    for (int i = 0; i < n; ++i) {//OA环境光遮蔽
        std::cerr << i << std::endl;
        double y = dist(gen);//随机数，高度
        double theta = 2.0 * M_PI * dist(gen);//水平旋转角度
        double r = std::sqrt(1.0 - y * y);//水平半径
        vec3 light = vec3{ r * std::cos(theta), y, r * std::sin(theta) } * 1.5;//光源坐标

        std::cout << "v " << light << std::endl;//输出光照角度

        lookat(light, center, up);//相机放在light位置，朝向原点
        ModelView[3][3] = norm(light - center);//相机到物体距离
        init_perspective(norm(eye - center));
        init_viewport(shadoww / 16, shadowh / 16, shadoww * 7 / 8, shadowh * 7 / 8);
        init_zbuffer(shadoww, shadowh);
        TGAImage trash(shadoww, shadowh, TGAImage::RGB, { 177, 195, 209, 255 });

        for (int m = 1; m < argc; m++) {                    // iterate through all input objects
            Model model(argv[m]);                       // load the data
            EmptyShader shader{ model };
            for (int f = 0; f < model.nfaces(); f++) {      // iterate through all facets
                Triangle clip = { shader.vertex(f, 0),  // assemble the primitive
                    shader.vertex(f, 1),
                    shader.vertex(f, 2) };
                rasterize(clip, shader, trash);         // rasterize the primitive
            }
        }
        //trash.write_tga_file(std::string("shadowmap") + std::to_string(i) + std::string(".tga"));


        // 后处理1:添加阴影效果
    #pragma omp parallel for
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                vec4 fragment = M * vec4{ (double)x, (double)y, zbuffer_copy[x + y * width], 1. };
                //在当前像素从屏幕坐标反推回3D世界坐标，得到当前像素的世界坐标
                vec4 q = N * fragment;//在光源视角下的齐次坐标
                vec3 p = q.xyz() / q.w;//归一化得到 NDC坐标
                double lit = (fragment.z < -100 ||//当前光照下是否该被照亮        // 背景
                    (p.x >= 0 && p.x < shadoww && p.y >= 0 && p.y < shadowh &&    // 非边界
                        (p.z > zbuffer[int(p.x) + int(p.y) * shadoww] - .03)));   // 可视
                mask[x + y * width] += (lit - mask[x + y * width]) / (i + 1.);
                //“指数移动平均”滤波，实现多光源累计阴影效果，或渐进式平滑
                //新平均值 = 旧平均值 + (新样本 - 旧平均值) / (已处理样本数 + 1)
            }
        }
    }


//后处理1：应用阴影效果
#pragma omp parallel for
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            double m = smoothstep(-1,1.,mask[x + y * width]);
            TGAColor c = framebuffer.get(x, y);
            framebuffer.set(x, y, { to_byte(c[0] * m),to_byte(c[1] * m), to_byte(c[2] * m), c[3] });
        }
    }
    framebuffer.write_tga_file("shadow.tga");


    //后处理2：边缘检测 并画成黑色 ，使用Sobel算子 
    constexpr double threshold = .15;//敏感度阈值：像素深度变化>0.15，则为边缘
    for (int y = 1;y < framebuffer.height() - 1;++y) {
        for (int x = 1;x < framebuffer.width() - 1;++x) {//跳过边界：卷积需要3*3
            vec2 sum;
            for (int j = -1;j <= 1;j++) {
                for (int i = -1;i <= 1;i++) {
                    constexpr int Gx[3][3] = { {-1,0,1},{-2,0,2},{-1,0,1} };
                    constexpr int Gy[3][3] = { {-1,-2,-1},{0,0,0},{1,2,1} };
                    //定义Sobel卷积核，水平方向 和 垂直方向
                    sum = sum + vec2{ Gx[j + 1][i + 1] * zbuffer_copy[x + i + (y + j) * width],Gy[j + 1][i + 1] * zbuffer_copy[x + i + (y + j) * width] };
                }
            }
            if (norm(sum) > threshold)
                framebuffer.set(x, y, TGAColor{ 0, 0, 0, 255 });//涂黑
        }
    }
    framebuffer.write_tga_file("edges.tga");

    return 0;
}