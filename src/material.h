#pragma once
#include "geometry.h"

struct Material {
    Vec2f albedo;
    Vec3f diffuse_color;
    float specular_exponent;

    Material(const Vec2f &a, const Vec3f color, const float &spec) : albedo(a), diffuse_color(color), specular_exponent(spec) {}

    Material() : albedo(1, 0), diffuse_color(), specular_exponent() {}
};