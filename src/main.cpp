#include "../include/color.h"
#include "../include/vec3.h"
#include "../include/ray.h"
#include "../include/sphere.h"
#include "../include/scene.h"
#include "../include/camera.h"
#include "../include/utils.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>  // For directory checking

color ray_color(const ray& r, const scene& world, int depth) {
    hit_record rec;

    if (depth <= 0)
        return color(0, 0, 0);

    if (world.hit(r, 0.001, infinity, rec)) {
        ray scattered;
        color attenuation;

        if (rec.is_dielectric) {
            double refraction_ratio = rec.front_face ? (1.0/rec.ir) : rec.ir;
            vec3 unit_direction = unit_vector(r.direction());
            double cos_theta = fmin(dot(-unit_direction, rec.normal), 1.0);
            double sin_theta = sqrt(1.0 - cos_theta*cos_theta);

            bool cannot_refract = refraction_ratio * sin_theta > 1.0;
            vec3 direction;

            if (cannot_refract || reflectance(cos_theta, refraction_ratio) > random_double())
                direction = reflect(unit_direction, rec.normal);
            else
                direction = refract(unit_direction, rec.normal, refraction_ratio);

            scattered = ray(rec.p, direction);
            attenuation = color(1.0, 1.0, 1.0);
            return attenuation * ray_color(scattered, world, depth-1);
        }
        else if (rec.is_metal) {
            vec3 reflected = reflect(unit_vector(r.direction()), rec.normal);
            scattered = ray(rec.p, reflected + rec.fuzz*random_in_unit_sphere());
            attenuation = rec.albedo;
            return attenuation * ray_color(scattered, world, depth-1);
        }
        else {
            vec3 scatter_direction = rec.normal + random_unit_vector();
            if (scatter_direction.near_zero())
                scatter_direction = rec.normal;
            scattered = ray(rec.p, scatter_direction);
            attenuation = rec.albedo;
            return attenuation * ray_color(scattered, world, depth-1);
        }
    }

    vec3 unit_direction = unit_vector(r.direction());
    auto t = 0.5*(unit_direction.y() + 1.0);
    return (1.0-t)*color(1.0, 1.0, 1.0) + t*color(0.5, 0.7, 1.0);
}

scene replicated_scene() {
    scene world;

    // Ground
    world.add(sphere(point3(0, -1000, 0), 1000, color(0.5, 0.5, 0.5)));

    // Random smaller spheres
    for (int a = -5; a < 5; a++) {
        for (int b = -5; b < 5; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9*random_double(), 0.2, b + 0.9*random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                if (choose_mat < 0.8) {
                    auto albedo = vec3::random() * vec3::random();
                    world.add(sphere(center, 0.2, albedo));
                } else if (choose_mat < 0.95) {
                    auto albedo = vec3::random(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    world.add(sphere(center, 0.2, albedo, true, false, fuzz));
                } else {
                    world.add(sphere(center, 0.2, color(1, 1, 1), false, true, 0.0, 1.5));
                }
            }
        }
    }

    // Central glass bubble. The negative radius flips normals to create a hollow air interior.
    world.add(sphere(point3(0, 1, 0), 1.0, color(1.0, 1.0, 1.0), false, true, 0.0, 1.5));
    world.add(sphere(point3(0, 1, 0), -0.95, color(1.0, 1.0, 1.0), false, true, 0.0, 1.0 / 1.5));

    // Large side spheres from the reference scene
    world.add(sphere(point3(-4, 1, 0), 1.0, color(0.4, 0.2, 0.1)));
    world.add(sphere(point3(4, 1, 0), 1.0, color(0.7, 0.6, 0.5), true, false, 0.0));

    return world;
}
int main() {
    // Initialize random seed
    std::srand(std::time(0));

    // Image
    const auto aspect_ratio = 16.0 / 9.0;
    const int image_width = 1080;
    const int image_height = static_cast<int>(image_width / aspect_ratio);
    const int samples_per_pixel = 100;
    const int max_depth = 40;

    // World
    auto world = replicated_scene();

    // Create frames directory 
    struct stat info;
    if (stat("frames", &info) != 0 || !(info.st_mode & S_IFDIR)) {
        #ifdef _WIN32
        system("mkdir frames 2> nul");
        #else
        system("mkdir -p frames");
        #endif
    }

    // Render frames
    for (int frame = 0; frame < 250; ++frame) {
        std::cerr << "\rRendering frame " << frame+1 << " of 250" << std::flush;

        // Camera
        double theta = (2 * pi * frame) / 250.0;
        point3 lookfrom(13.5*cos(theta), 2.0, 13.5*sin(theta));
        point3 lookat(0, 1, 0);
        vec3 vup(0, 1, 0);
        double vfov = 20.0;
        camera cam(lookfrom, lookat, vup, vfov, aspect_ratio);

        // Output file
        char filename[32];
        sprintf(filename, "frames/frame%03d.ppm", frame);
        std::ofstream out(filename);
        out << "P3\n" << image_width << ' ' << image_height << "\n255\n";

        // Render
        for (int j = image_height-1; j >= 0; --j) {
            for (int i = 0; i < image_width; ++i) {
                color pixel_color(0, 0, 0);
                for (int s = 0; s < samples_per_pixel; ++s) {
                    auto u = (i + random_double()) / (image_width-1);
                    auto v = (j + random_double()) / (image_height-1);
                    ray r = cam.get_ray(u, v);
                    pixel_color += ray_color(r, world, max_depth);
                }
                write_color(out, pixel_color, samples_per_pixel);
            }
        }
        out.close();
    }

    std::cerr << "\nDone rendering frames.\n";
    
    // Combine frames into video using FFmpeg
    std::cerr << "Creating video...\n";
    #ifdef _WIN32
    system("ffmpeg -framerate 25 -i frames/frame%03d.ppm -c:v libx264 -pix_fmt yuv420p output.mp4 -y");
    #else
    system("ffmpeg -framerate 25 -i frames/frame%03d.ppm -c:v libx264 -pix_fmt yuv420p output.mp4 -y");
    #endif
    std::cerr << "Done! Video saved as output.mp4\n";

    return 0;
}
