#include <limits>
#include <cmath>
#include <iostream>
#include <fstream>
#include <vector>
#include "geometry.h"
#include "sphere.h"
#include "light.h"

#define MAX_REFLECTION_DEPTH 4
#define MAX_DISTANCE 1000
#define PPM_WIDTH 1064
#define PPM_HEIGHT 768

Vec3f reflect(const Vec3f &I, const Vec3f &N) {
    return I - N * 2.f * (I * N);
}

Vec3f refract(const Vec3f &I, const Vec3f &N, const float &refractive_index) {
    //Snell's law
    float cosi = -std::max(-1.f, std::min(1.f, I*N));
    float etai = 1, etat = refractive_index;
    Vec3f n = N;

    // Ensure the ray is outside the object, swapping the indices and inverting the normal if it is inside the object.
    if (cosi < 0) {
        cosi = -cosi;
        std::swap(etai, etat);
        n = -N;
    }

    float eta = etai / etat;
    float k = 1 - eta * eta * (1 - cosi * cosi);

    return k < 0 ? Vec3f(1, 0, 0) : I * eta + n * (eta * cosi - sqrtf(k));
}

bool scene_intersect(const Vec3f &orig, const Vec3f &dir, const std::vector<Sphere> &spheres, Vec3f &hit, Vec3f &N, Material &material) {
    float spheres_dist = std::numeric_limits<float>::max();

    // Iterate over each sphere.
    for (size_t i = 0; i < spheres.size(); i++) {
        float dist_i;

        // If the sphere is hit, record it.
        if (spheres[i].ray_intersect(orig, dir, dist_i) && dist_i < spheres_dist) {
            spheres_dist = dist_i;
            hit = orig + dir * dist_i;
            N = (hit - spheres[i].center).normalize();
            material = spheres[i].material;
        }
    }

    // If the sphere is closer than the max draw distance, return true.
    return spheres_dist < MAX_DISTANCE;
}

Vec3f cast_ray(const Vec3f &orig, const Vec3f &dir, const std::vector<Sphere> &spheres, const std::vector<Light> &lights, size_t depth = 0) {
    Vec3f point, N;
    Material material;
    float diffuse_light_intensity = 0, specular_light_intensity = 0;

    // Check this ray isn't 5 or more reflections deep into being cast.
    if (depth > MAX_REFLECTION_DEPTH || !scene_intersect(orig, dir, spheres, point, N, material)) {
        return Vec3f(0.2, 0.7, 0.8); // Background color.
    }

    // Calculate a reflected ray, and get the color from casting it.
    Vec3f reflect_dir = reflect(dir, N).normalize();
    Vec3f reflect_orig = reflect_dir * N < 0 ? point - N * 1e-3 : point + N * 1e-3;
    Vec3f reflect_color = cast_ray(reflect_orig, reflect_dir, spheres, lights, depth + 1);

    Vec3f refract_dir = refract(dir, N, material.refractive_index).normalize();
    Vec3f refract_orig = refract_dir * N < 0 ? point - N * 1e-3 : point + N * 1e-3;
    Vec3f refract_color = cast_ray(refract_orig, refract_dir, spheres, lights, depth + 1);

    // Calculate diffuse lighting
    for (size_t i = 0; i < lights.size(); i++) {
        Vec3f light_dir = (lights[i].position - point).normalize();
        float light_distance = (lights[i].position - point).norm();
        
        Vec3f shadow_orig = light_dir * N < 0 ? point - N * 1e-3 : point + N * 1e-3; // Offset the point to ensure it doesn't accidentally hit the same sphere.
        Vec3f shadow_pt, shadow_N;
        Material tmpmaterial;

        // Check if a ray from this point to the current light is obscured by another object. If so, skip this light.
        if (scene_intersect(shadow_orig, light_dir, spheres, shadow_pt, shadow_N, tmpmaterial) && (shadow_pt - shadow_orig).norm() < light_distance)
            continue;

        diffuse_light_intensity += lights[i].intensity * std::max(0.f, light_dir * N);

        specular_light_intensity += powf(std::max(0.f, reflect(light_dir, N) * dir), material.specular_exponent) * lights[i].intensity;
    }

    // Calculate final pixel color.
    // Diffuse shading
    // Specular highlights
    // Reflections
    // Refractions
    return material.diffuse_color * diffuse_light_intensity * material.albedo[0] + Vec3f(1., 1., 1.) * specular_light_intensity * material.albedo[1] + reflect_color * material.albedo[2] + refract_color * material.albedo[3];
}

void render(const std::vector<Sphere> &spheres, const std::vector<Light> &lights) {
    const int fov = M_PI/2.;
    const int width = PPM_WIDTH;
    const int height = PPM_HEIGHT;
    std::vector<Vec3f> framebuffer(width * height);

    // Render each pixel.
    for (size_t j = 0; j < height; j++) {
        for (size_t i = 0; i < width; i++) {
            // Determine direction of ray according to pixel location and fov.
            float x = (2 * (i + 0.5) / (float) width - 1) * tan(fov/2.) * width / (float) height;
            float y = -(2 * (j + 0.5) / (float) height - 1) * tan(fov/2.);
            Vec3f dir = Vec3f(x, y, -1).normalize();

            framebuffer[i + j * width] = cast_ray(Vec3f(0, 0, 0), dir, spheres, lights);
        }
    }

    // Output to file.

    std::ofstream ofs;
    
    ofs.open("./out.ppm");
    ofs << "P6\n" << width << " " << height << "\n255\n";

    for(size_t i = 0; i < height * width; i++) {
        Vec3f &c = framebuffer[i];
        float max = std::max(c[0], std::max(c[1], c[2]));

        if (max > 1) c = c * (1. / max);
        for (size_t j = 0; j < 3; j++) {
            ofs << (char) (255 * std::max(0.f, std::min(1.f, framebuffer[i][j])));
        }
    }

    ofs.close();
}

int main() {
    Material ivory(1.0, Vec4f(0.6, 0.3, 0.1, 0.0), Vec3f(0.4, 0.4, 0.3), 50.);
    Material glass(1.5, Vec4f(0.0, 0.5, 0.1, 0.8), Vec3f(0.6, 0.7, 0.8), 125.);
    Material red_rubber(1.0, Vec4f(0.9, 0.1, 0.0, 0.0), Vec3f(0.3, 0.1, 0.1), 10.);
    Material mirror(1.0, Vec4f(0.0, 10.0, 0.8, 0.0), Vec3f(1.0, 1.0, 1.0), 1425.);
    
    std::vector<Sphere> spheres;
    spheres.push_back(Sphere(Vec3f(-3, 0, -16), 2, ivory));
    spheres.push_back(Sphere(Vec3f(-1.0, -1.5, -12), 2, glass));
    spheres.push_back(Sphere(Vec3f( 1.5, -0.5, -18), 3, red_rubber));
    spheres.push_back(Sphere(Vec3f( 7, 5, -18), 4, mirror));

    std::vector<Light> lights;
    lights.push_back(Light(Vec3f(-20, 20, 20), 1.5));
    lights.push_back(Light(Vec3f( 30, 50, -25), 1.8));
    lights.push_back(Light(Vec3f( 30, 20,  30), 1.7));

    render(spheres, lights);

    return 0;
}
