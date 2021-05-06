#pragma once
#include "geometry.h"

struct Material {
    float refractive_index;
    Vec4f albedo;
    Vec3f diffuse_color;
    float specular_exponent;

    Material(const float &r, const Vec4f &a, const Vec3f color, const float &spec) : refractive_index(r), albedo(a), diffuse_color(color), specular_exponent(spec) {}

    Material() : albedo(1, 0, 0, 0), diffuse_color(), specular_exponent() {}
};