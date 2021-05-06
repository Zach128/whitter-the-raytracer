#pragma once
#include "geometry.h"

struct Material {
    Vec3f diffuse_color;
    
    Material(const Vec3f color) : diffuse_color(color) {}
    Material() : diffuse_color() {}
};