#define _USE_MATH_DEFINES
#include <limits>
#include <cmath>
#include <iostream>
#include <fstream>
#include <vector>
#include "geometry.h"
#include "sphere.h"
#include "light.h"
#include "model.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define MAX_REFLECTION_DEPTH 4
#define MAX_DISTANCE 1000
#define PPM_WIDTH 1064
#define PPM_HEIGHT 768

int envmap_width, envmap_height;
std::vector<Vec3f> envmap;
Model duck("./duck.obj");

Vec3f reflect(const Vec3f &I, const Vec3f &N) {
    return I - N * 2.f * (I * N);
}

Vec3f refract(const Vec3f &I, const Vec3f &N, const float eta_t, const float eta_i = 1.f) {
    //Snell's law
    float cosi = -std::max(-1.f, std::min(1.f, I*N));
    if (cosi < 0) return refract(I, -N, eta_i, eta_t); // If the ray is coming from inside the object, swap the order of the media being passed through (such as from the media to air or vice versa).

    float eta = eta_i / eta_t;
    float k = 1 - eta * eta * (1 - cosi * cosi);

    return k < 0 ? Vec3f(1, 0, 0) : I * eta + N * (eta * cosi - sqrtf(k));
}

bool scene_intersect(const Vec3f &orig, const Vec3f &dir, const std::vector<Sphere> &spheres, Vec3f &hit, Vec3f &N, Material &material) {
    float spheres_dist = std::numeric_limits<float>::max();
    float checkerboard_dist = std::numeric_limits<float>::max();

    // Perform testing on each sphere.
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

    // Perform testing on the checkboard.
    if (fabs(dir.y) > 1e-3) {
        float d = -(orig.y + 4) / dir.y;
        Vec3f pt = orig + dir * d;

        // Check if we hit the checkerboard.
        if (d > 0 && fabs(pt.x) < 10 && pt.z < -10 && pt.z > -30 && d < spheres_dist) {
            checkerboard_dist = d;
            hit = pt;
            N = Vec3f(0, 1, 0);

            // Choose a checkerboard pattern.
            material.diffuse_color = (int(.5 * hit.x + 1000) + int(.5 * hit.z)) & 1 ? Vec3f(1, 1, 1) : Vec3f(1, .7, .3);
            material.diffuse_color = material.diffuse_color * .3;
        }
    }

    // If the sphere is closer than the max draw distance, return true.
    return std::min(spheres_dist, checkerboard_dist) < MAX_DISTANCE;
}

Vec3f cast_ray(const Vec3f &orig, const Vec3f &dir, const std::vector<Sphere> &spheres, const std::vector<Light> &lights, size_t depth = 0) {
    Vec3f point, N;
    Material material;
    float diffuse_light_intensity = 0, specular_light_intensity = 0;

    // Check this ray isn't 5 or more reflections deep into being cast, or hasn't hit an object in the scene.
    // If so, return a background color obtained from the environment map.
    if (depth > MAX_REFLECTION_DEPTH || !scene_intersect(orig, dir, spheres, point, N, material)) {
        // Convert the ray direction to a UV coordinate.
        float u = .5 + atan2(dir.x, dir.z) / (2 * M_PI);
        float v = .5 - asin(dir.y) / M_PI;

        // Return the colour from the envmap at the given coordinates.
        return envmap[int(u * envmap_width) + int(v * envmap_height) * envmap_width];
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
            float dir_x = (i + 0.5) - width / 2.;
            float dir_y = -(j + 0.5) + height / 2.;
            float dir_z = -height / (2. * tan(fov / 2.));

            framebuffer[i + j * width] = cast_ray(Vec3f(0, 0, 0), Vec3f(dir_x, dir_y, dir_z).normalize(), spheres, lights);
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
    int n = -1;
    unsigned char *pixmap = stbi_load("./envmap.jpg", &envmap_width, &envmap_height, &n, 0);

    if (!pixmap || 3 != n) {
        std::cerr << "Error: can not load the environment map" << std::endl;

        return -1;
    }

    envmap = std::vector<Vec3f>(envmap_width * envmap_height);

    // Load the environment map into memory.
    for (int j = envmap_height - 1; j >= 0; j--) {
        for (int i = 0; i < envmap_width; i++) {
            envmap[i + j * envmap_width] = Vec3f(pixmap[(i + j * envmap_width) * 3 + 0], pixmap[(i + j * envmap_width) * 3 + 1], pixmap[(i + j * envmap_width) * 3 + 2]) * (1/255.);
        }
    }

    stbi_image_free(pixmap);

    // Initialise materials
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
