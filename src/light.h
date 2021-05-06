#pragma once
#include "geometry.h"

struct Light {
    Vec3f position;
    float intensity;

    Light(const Vec3f &p, const float i) : position(p), intensity(i) {}
};
