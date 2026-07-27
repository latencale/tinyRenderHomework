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

void triangle(int ax, int ay, int bx, int by, int cx, int cy, TGAImage& framebuffer, TGAColor color) {
    int bbminx = std::min(std::min(ax, bx), cx); // bounding box for the triangle
    int bbminy = std::min(std::min(ay, by), cy); // defined by its top left and bottom right corners
    int bbmaxx = std::max(std::max(ax, bx), cx);
    int bbmaxy = std::max(std::max(ay, by), cy);
    double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
    if (total_area < 1) return; // 背面剔除 + 丢弃覆盖面积小于一个像素的三角形

#pragma omp parallel for
    for (int x = bbminx; x <= bbmaxx; x++) {
        for (int y = bbminy; y <= bbmaxy; y++) {
            double alpha = signed_triangle_area(x, y, bx, by, cx, cy) / total_area;
            double beta = signed_triangle_area(x, y, cx, cy, ax, ay) / total_area;
            double gamma = signed_triangle_area(x, y, ax, ay, bx, by) / total_area;
            if (alpha < 0 || beta < 0 || gamma < 0) continue; // negative barycentric coordinate => the pixel is outside the triangle
            framebuffer.set(x, y, color);
        }
    }
}

// 将 3D 坐标投影到屏幕
std::pair<int, int> project(const vec3& v) {
    int x = static_cast<int>((v.x + 1.0f) * width / 2.0f);
    int y = static_cast<int>((v.y + 1.0f) * height / 2.0f);
    // 注意：TGA 的 y 轴向下，所以需要翻转
    y = height - 1 - y;
    return {x, y};
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "用法: " << argv[0] << " <模型文件.obj>" << std::endl;
        return 1;
    }
    
    Model model(argv[1]);
    if (model.nverts() == 0 || model.nfaces() == 0) {
        std::cerr << "错误: 模型没有顶点或三角形数据" << std::endl;
        return 1;
    }
    
    TGAImage framebuffer(width, height, TGAImage::RGB);
    
	// 绘制三角形并填充颜色
    for (int i = 0; i < model.nfaces(); i++) {
        auto [ax, ay] = project(model.vert(i, 0));
        auto [bx, by] = project(model.vert(i, 1));
        auto [cx, cy] = project(model.vert(i, 2));
        
        TGAColor rnd;
        for (int c = 0; c < 3; c++) rnd[c] = std::rand() % 255;
        triangle(ax, ay, bx, by, cx, cy, framebuffer, rnd);
    }
    
    // 绘制顶点（用小点标记）
    //for (int i = 0; i < model.nverts(); i++) {
    //    auto [x, y] = project(model.vert(i));
    //    // 画一个点
    //    framebuffer.set(x, y, white);
    //}
        
    
    framebuffer.write_tga_file("framebuffer.tga");
    std::cout << "渲染完成！保存为 framebuffer.tga" << std::endl;
    
    return 0;
}