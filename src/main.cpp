#include <limits>
#include <cmath>
#include <iostream>
#include <fstream>
#include <vector>
#include "geometry.h"
#include "sphere.h"

Vec3f cast_ray(const Vec3f &orig, const Vec3f &dir, const Sphere &sphere) {
    float sphere_dist = std::numeric_limits<float>::max();

    if (!sphere.ray_intersect(orig, dir, sphere_dist)) {
        return Vec3f(0.2, 0.7, 0.8); // Background colour.
    }

    return Vec3f(0.4, 0.4, 0.3); // Global sphere colour.
}

void render(const Sphere &sphere) {
    const int fov = M_PI/2.;
    const int width = 1064;
    const int height = 768;
    std::vector<Vec3f> framebuffer(width * height);

    // Render each pixel.
    for (size_t j = 0; j < height; j++) {
        for (size_t i = 0; i < width; i++) {
            // Determine direction of ray according to pixel location and fov.
            float x = (2 * (i + 0.5) / (float) width - 1) * tan(fov/2.) * width / (float) height;
            float y = -(2 * (j + 0.5) / (float) height - 1) * tan(fov/2.);
            Vec3f dir = Vec3f(x, y, -1).normalize();

            framebuffer[i + j * width] = cast_ray(Vec3f(0, 0, 0), dir, sphere);
        }
    }

    // Output to file.

    std::ofstream ofs;
    
    ofs.open("./out.ppm");
    ofs << "P6\n" << width << " " << height << "\n255\n";

    for(size_t i = 0; i < height * width; i++) {
        for (size_t j = 0; j < 3; j++) {
            ofs << (char) (255 * std::max(0.f, std::min(1.f, framebuffer[i][j])));
        }
    }

    ofs.close();
}

int main() {
    Sphere sphere(Vec3f(-3, 0, -16), 2);
    
    render(sphere);

    return 0;
}
