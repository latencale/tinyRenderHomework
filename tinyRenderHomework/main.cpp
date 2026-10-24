// main.cpp
#include <iostream>
#include <cmath>
#include <algorithm>
#include<limits>
#include<filesystem>
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

struct BlankShader :IShader {//空着色器：生成阴影贴图
    const Model& model;
    BlankShader(const Model& m) :model(m) {}

    virtual vec4 vertex(const int face, const int vert) {
        vec4 gl_Position = ModelView * model.vert(face, vert);
        return Perspective * gl_Position;
    }

    virtual std::pair<bool, TGAColor> fragment(const vec3 bar) const {
        return { false, { 255,255,255,255 } };
    }
};


struct PhongShader :IShader {
    const Model& model;
    vec4 l;//光照方向
    vec2 varying_uv[3];//纹理贴图坐标
    vec4 varying_nrm[3];//三角形的法向量
    vec4 tri[3];//三角形坐标点

    PhongShader(const Model & m,const vec3 light):model(m){
        l = normalized(ModelView * vec4{ light.x,light.y,light.z,0. });
    }

    virtual vec4 vertex(const int face, const int vert) {//第face个三角形的第vert个点
        varying_uv[vert] = model.uv(face, vert);
        varying_nrm[vert] = ModelView.invert_transpose() * model.normal(face, vert);
        vec4 gl_Position = ModelView * model.vert(face, vert);
        tri[vert] = gl_Position;
        //分别得到uv,法线,三角形在观察空间的坐标
        return Perspective * gl_Position;
    }

    virtual std::pair<bool, TGAColor>fragment(const vec3 bar) const {//bar=(alpha,beta,gamma)
        //将法线贴图中的法线从切线空间转换到世界空间
        mat<2, 4> E = { tri[1] - tri[0],tri[2] - tri[0] };//三角形的两条边（世界空间）
        mat<2, 2> U = { varying_uv[1] - varying_uv[0],varying_uv[2] - varying_uv[0] };//三角形的两条边（uv空间）
        mat<2, 4> T = U.invert() * E;
        mat<4, 4> D = { normalized(T[0]),//切线向量(tangent)
                    normalized(T[1]),//副切线向量(bitangent)
                    normalized(varying_nrm[0] * bar[0] + varying_nrm[1] * bar[1] + varying_nrm[2] * bar[2]),//法线
                    {0,0,0,1} };

        vec2 uv = varying_uv[0] * bar[0] + varying_uv[1] * bar[1] + varying_uv[2] * bar[2];//这个像素的纹理坐标
        vec4 n = normalized(D.transpose() * model.normal(uv));//法线
        vec4 r = normalized(2 * n * (n * l) - l);//反射光

        double ambient = .4;//环境光强度
        double diffuse = 1.*std::max(0., n * l);//漫反射光强度
        double specular = (3.*sample2D(model.specular(),uv)[0]/255.)*std::pow(std::max(r.z, 0.), 35);//镜面反射光强度
        TGAColor gl_FragColor = sample2D(model.diffuse(), uv);//输出颜色：从漫反射贴图获取基础颜色

        for (int channel : {0, 1, 2})
            gl_FragColor[channel] = std::min<int>(255, gl_FragColor[channel] * (ambient + diffuse + specular));
        return {false, gl_FragColor};
    }
};

void drop_zbuffer(std::string filename, std::vector<double>& zbuffer, int width, int height) {
    TGAImage zimg(width, height, TGAImage::GRAYSCALE, { 0,0,0,0 });
    double minz = +1000;
    double maxz = -1000;
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            double z = zbuffer[x + y * width];
            if (z < -100) continue;
            minz = std::min(z, minz);
            maxz = std::max(z, maxz);
        }
    }
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            double z = zbuffer[x + y * width];
            if (z < -100) continue;
            z = (z - minz) / (maxz - minz) * 255;
            zimg.set(x, y, { to_byte(z), 255, 255, 255 });
        }
    }
    zimg.write_tga_file(filename);
}


uint8_t A(int x) {
    return (uint8_t)x;
}
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

    std::vector<double>zshadow;//光源的深度缓冲
    mat<4, 4> ShadowMatrix;

    //常规渲染通道：摄像机处
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
    std::filesystem::create_directories("output");
    framebuffer.write_tga_file("output/framebuffer.tga");
    drop_zbuffer("output/zbuffer1.tga", zbuffer, width, height);

    std::vector<bool> mask(width * height, false);
    std::vector<double> zbuffer_copy = zbuffer;
    mat<4, 4> M = (Viewport * Perspective * ModelView).invert();

    { // 阴影渲染路径
        lookat(light, center, up);
        init_perspective(norm(eye - center));
        init_viewport(shadoww / 16, shadowh / 16, shadoww * 7 / 8, shadowh * 7 / 8);
        init_zbuffer(shadoww, shadowh);
        TGAImage trash(shadoww, shadowh, TGAImage::RGB, { 177, 195, 209, 255 });

        for (int m = 1; m < argc; m++) {                    // iterate through all input objects
            Model model(argv[m]);                       // load the data
            BlankShader shader{ model };
            for (int f = 0; f < model.nfaces(); f++) {      // iterate through all facets
                Triangle clip = { shader.vertex(f, 0),  // assemble the primitive
                                  shader.vertex(f, 1),
                                  shader.vertex(f, 2) };
                rasterize(clip, shader, trash);         // rasterize the primitive
            }
        }
        trash.write_tga_file("output/shadowmap.tga");
    }
    drop_zbuffer("output/zbuffer2.tga", zbuffer, shadoww, shadowh);

    mat<4, 4> N = Viewport * Perspective * ModelView;

    // 后处理1:添加阴影效果
#pragma omp parallel for
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            vec4 fragment = M*vec4{ (double)x, (double)y, zbuffer_copy[x + y * width], 1. };
            //在当前像素从屏幕坐标反推回3D世界坐标，得到当前像素的世界坐标
            vec4 q =N * fragment;//在光源视角下的齐次坐标
            vec3 p = q.xyz() / q.w;//归一化得到 NDC坐标
            bool lit = (fragment.z < -100 ||                                   // 背景or出界or可视
                (p.x < 0 || p.x >= shadoww || p.y < 0 || p.y >= shadowh) ||   
                (p.z > zbuffer[int(p.x) + int(p.y) * shadoww] - .03));  
            mask[x + y * width] = lit;
        }
    }

    TGAImage maskimg(width, height, TGAImage::GRAYSCALE);
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            if (mask[x + y * width]) continue;
            maskimg.set(x, y, { 255, 255, 255, 255 });
        }
    }
    maskimg.write_tga_file("output/mask.tga");

    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            if (mask[x + y * width]) continue;
            TGAColor c = framebuffer.get(x, y);
            vec3 a = { c[0], c[1], c[2] };
            if (norm(a) < 80) continue;
            a = normalized(a) * 80;
            framebuffer.set(x, y, { to_byte(a[0]), to_byte(a[1]), to_byte(a[2]), 255 });
        }
    }
    framebuffer.write_tga_file("output/shadow.tga");

    // 后处理2：屏幕空间环境光遮蔽 SSAO
    // 注意：必须读 zbuffer_copy（相机视角 800x800），不能读 zbuffer
    //       —— zbuffer 已经被上面的阴影通道 init_zbuffer(shadoww, shadowh) 换成了 2000x2000
    constexpr double ao_radius = .1;        // 采样半径（NDC 空间单位）
    constexpr int nsamples = 128;           // 每个像素的采样点数量
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(-ao_radius, ao_radius);

    auto smoothstep = [](double edge0, double edge1, double x) {  // 平滑插值：两端缓、中间陡
        double t = std::clamp((x - edge0) / (edge1 - edge0), 0., 1.);
        return t * t * (3 - 2 * t);
        };

    const mat<4, 4> ViewportInv = Viewport.invert();  // 提到循环外，避免每个像素都求一次逆

    // TODO: 这里的循环尚未并行化。若要加 #pragma omp parallel for，
    //       必须为每个线程单独创建 mt19937，否则多个线程共用同一个引擎会产生数据竞争。
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            double z = zbuffer_copy[x + y * width];
            if (z < -100) continue;  // 背景跳过
            vec4 fragment = ViewportInv * vec4{ (double)x, (double)y, z, 1. };  // 屏幕坐标 -> NDC
            double vote = 0;    // 被遮挡的采样点数量
            double voters = 0;  // 有效采样点数量
            for (int i = 0; i < nsamples; i++) {
                vec4 p = Viewport * (fragment + vec4{ dist(gen), dist(gen), dist(gen), 0. });  // 采样点投影回屏幕
                if (p.x < 0 || p.x >= width || p.y < 0 || p.y >= height) continue;  // 出界的采样点不计入
                double d = zbuffer_copy[int(p.x) + int(p.y) * width];
                if (z + 5 * ao_radius < d) continue;  // 深度差过大，不算同一片遮蔽体
                voters++;
                vote += d > p.z;  // 该处深度更靠近相机 -> 采样点被遮挡
            }
            double ratio = voters > 0 ? vote / voters : 0.;  // 防除零
            double ssao = smoothstep(0, 1, 1 - ratio * .4);  // 遮挡比例越大 -> 越暗
            TGAColor c = framebuffer.get(x, y);
            framebuffer.set(x, y, { to_byte(c[0] * ssao), to_byte(c[1] * ssao), to_byte(c[2] * ssao), c[3] });
        }
    }
    framebuffer.write_tga_file("output/ssao.tga");

    return 0;
}