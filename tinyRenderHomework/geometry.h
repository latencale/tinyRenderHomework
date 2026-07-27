// geometry.h
#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <cmath>
#include <iostream>

struct vec3 { //坐标
    float x, y, z;

    vec3() : x(0), y(0), z(0) {}
    vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    float& operator[](int i) {
        return i == 0 ? x : (i == 1 ? y : z);
    }

    const float& operator[](int i) const {
        return i == 0 ? x : (i == 1 ? y : z);
    }

    vec3 operator+(const vec3& other) const {
        return { x + other.x, y + other.y, z + other.z };
    }

    vec3 operator-(const vec3& other) const {
        return { x - other.x, y - other.y, z - other.z };
    }

    vec3 operator*(float scalar) const {
        return { x * scalar, y * scalar, z * scalar };
    }
};

#endif // GEOMETRY_H