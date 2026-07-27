// main.cpp
#include <iostream>
#include <cmath>
#include <algorithm>
#include "model.h"
#include "tgaimage.h"

constexpr int width = 800;
constexpr int height = 800;

const TGAColor white = {255, 255, 255, 255};
const TGAColor red = {0, 0, 255, 255};
const TGAColor green = {0, 255, 0, 255};
const TGAColor blue = {255, 0, 0, 255};

// Bresenham 画线算法（整数版本）
void line(int ax, int ay, int bx, int by, TGAImage& framebuffer, TGAColor color) {
    bool steep = std::abs(ax - bx) < std::abs(ay - by);
    if (steep) {
        std::swap(ax, ay);
        std::swap(bx, by);
    }
    if (ax > bx) {
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    
    int dx = bx - ax;
    int dy = std::abs(by - ay);
    
    if (dx == 0) {
        // 垂直线
        for (int y = std::min(ay, by); y <= std::max(ay, by); y++) {
            if (steep)
                framebuffer.set(y, ax, color);
            else
                framebuffer.set(ax, y, color);
        }
        return;
    }
    
    int y = ay;
    int error = 0;
    int y_step = (by > ay) ? 1 : -1;
    
    for (int x = ax; x <= bx; x++) {
        if (steep)
            framebuffer.set(y, x, color);
        else
            framebuffer.set(x, y, color);
        
        error += 2 * dy;
        if (error > dx) {
            y += y_step;
            error -= 2 * dx;
        }
    }
}

double signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy) {
    return .5 * ((by - ay) * (bx + ax) + (cy - by) * (cx + bx) + (ay - cy) * (ax + cx));
}

void triangle(int ax, int ay,int az, int bx, int by,int bz ,int cx, int cy,int cz, TGAImage& framebuffer,TGAImage& zbuffer,TGAColor color) {
    int bbminx = std::min(std::min(ax, bx), cx); // bounding box for the triangle
    int bbminy = std::min(std::min(ay, by), cy); // defined by its top left and bottom right corners
    int bbmaxx = std::max(std::max(ax, bx), cx);
    int bbmaxy = std::max(std::max(ay, by), cy);
    double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
    //if (total_area < 1) return; // 背面剔除 + 丢弃覆盖面积小于一个像素的三角形

#pragma omp parallel for
    for (int x = bbminx; x <= bbmaxx; x++) {
        for (int y = bbminy; y <= bbmaxy; y++) {
            double alpha = signed_triangle_area(x, y, bx, by, cx, cy) / total_area;
            double beta = signed_triangle_area(x, y, cx, cy, ax, ay) / total_area;
            double gamma = signed_triangle_area(x, y, ax, ay, bx, by) / total_area;
            if (alpha < 0 || beta < 0 || gamma < 0) continue; // negative barycentric coordinate => the pixel is outside the triangle
            unsigned char z = static_cast<unsigned char>(alpha * az + beta * bz + gamma * cz);
            if (z <= zbuffer.get(x, y)[0]) continue;//把深度存在B通道中，深度大于目前深度，跳过
            framebuffer.set(x, y,color);
            zbuffer.set(x, y, { z,z,z,255 });
        }
    }
}

// 将 3D 坐标投影到屏幕
std::tuple<int, int, int> project(const vec3& v) {//(x,y)是向量(x,y,z)的正交投影
    return { (v.x + 1.) * width / 2, 
        (v.y + 1.) * height / 2,
        (v.z + 1.) * 255. / 2 };//将深度值z，映射到[0,255]，存储在 灰度图像 中
}


int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "用法: " << argv[0] << " <模型文件.obj>" << std::endl;
        return 1;
    }
    
    Model model(argv[1]);
    TGAImage framebuffer(width, height, TGAImage::RGB);
    TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);

    if (model.nverts() == 0 || model.nfaces() == 0) {
        std::cerr << "错误: 模型没有顶点或三角形数据" << std::endl;
        return 1;
    }
    
	// 绘制三角形并填充颜色
    for (int i = 0; i < model.nfaces(); i++) {
        auto [ax, ay,az] = project(model.vert(i, 0));
        auto [bx, by,bz] = project(model.vert(i, 1));
        auto [cx, cy,cz] = project(model.vert(i, 2));
        
        TGAColor rnd;
        for (int c = 0; c < 3; c++) rnd[c] = std::rand() % 255;
        triangle(ax, ay, az, bx, by, bz, cx, cy, cz, framebuffer,zbuffer,rnd);
    }
    
    // 绘制顶点（用小点标记）
    //for (int i = 0; i < model.nverts(); i++) {
    //    auto [x, y] = project(model.vert(i));
    //    // 画一个点
    //    framebuffer.set(x, y, white);
    //}
        
    
    framebuffer.write_tga_file("framebuffer.tga");
    zbuffer.write_tga_file("zbuffer.tga");
    
    return 0;
}