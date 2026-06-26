#include "../include/color.h"
#include "../include/vec3.h"
#include "../include/ray.h"
#include "../include/sphere.h"
#include "../include/scene.h"
#include "../include/camera.h"
#include "../include/utils.h"
#ifdef TRACE_IT_ENABLE_CUDA
#include "../include/cuda_renderer.h"
#endif

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <sys/stat.h>
#include <thread>

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

    world.add(sphere(point3(0, -1000, 0), 1000, color(0.5, 0.5, 0.5)));

    for (int a = -5; a < 5; a++) {
        for (int b = -5; b < 5; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9*random_double(), 0.2, b + 0.9*random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9
                && (center - point3(0, 0.2, 0)).length() > 0.9
                && (center - point3(-4, 0.2, 0)).length() > 0.9) {
                if (choose_mat < 0.8) {
                    auto albedo = vec3::random() * vec3::random();
                    world.add(sphere(center, 0.2, albedo));
                } else if (choose_mat < 0.95) {
                    auto albedo = vec3::random(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    world.add(sphere(center, 0.2, albedo, true, false, fuzz));
                } else {
                    world.add(sphere(center, 0.2, color(1, 1, 1), false, true, 0.0, 1.5));
                    world.add(sphere(center, -0.19, color(1, 1, 1), false, true, 0.0, 1.5));
                }
            }
        }
    }

    world.add(sphere(point3(0, 1, 0), 1.0, color(1.0, 1.0, 1.0), false, true, 0.0, 1.5));
    world.add(sphere(point3(0, 1, 0), -0.95, color(1.0, 1.0, 1.0), false, true, 0.0, 1.5));
    world.add(sphere(point3(-4, 1, 0), 1.0, color(0.4, 0.2, 0.1)));
    world.add(sphere(point3(4, 1, 0), 1.0, color(0.7, 0.6, 0.5), true, false, 0.0));

    return world;
}

struct render_settings {
    double aspect_ratio = 16.0 / 9.0;
    int image_width = 1080;
    int samples_per_pixel = 100;
    int max_depth = 40;
    int total_frames = 250;
    std::string renderer = "auto";
    bool make_video = true;
};

void print_usage(const char* program) {
    std::cerr
        << "Usage: " << program << " [--renderer auto|cpu|cpu_multithreaded] [--width N]\n"
        << "       [--samples N] [--depth N] [--frames N] [--no-video]\n";
}

bool parse_args(int argc, char** argv, render_settings& settings) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        auto require_value = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << "\n";
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--renderer") {
            const char* value = require_value("--renderer");
            if (!value) return false;
            settings.renderer = value;
            if (settings.renderer != "auto" && settings.renderer != "cpu" && settings.renderer != "cpu_multithreaded") {
                std::cerr << "--renderer must be auto, cpu, or cpu_multithreaded\n";
                return false;
            }
        } else if (arg == "--width") {
            const char* value = require_value("--width");
            if (!value) return false;
            settings.image_width = std::atoi(value);
        } else if (arg == "--samples") {
            const char* value = require_value("--samples");
            if (!value) return false;
            settings.samples_per_pixel = std::atoi(value);
        } else if (arg == "--depth") {
            const char* value = require_value("--depth");
            if (!value) return false;
            settings.max_depth = std::atoi(value);
        } else if (arg == "--frames") {
            const char* value = require_value("--frames");
            if (!value) return false;
            settings.total_frames = std::atoi(value);
        } else if (arg == "--no-video") {
            settings.make_video = false;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }

    if (settings.image_width <= 0 || settings.samples_per_pixel <= 0 ||
        settings.max_depth <= 0 || settings.total_frames <= 0) {
        std::cerr << "Width, samples, depth, and frames must be positive integers.\n";
        return false;
    }

    return true;
}

void ensure_frames_directory() {
    struct stat info;
    if (stat("frames", &info) != 0 || !(info.st_mode & S_IFDIR)) {
        #ifdef _WIN32
        system("mkdir frames 2> nul");
        #else
        system("mkdir -p frames");
        #endif
    }
}

void render_frame_cpu(
    const scene& world,
    int frame,
    int total_frames,
    int image_width,
    int image_height,
    int samples_per_pixel,
    int max_depth,
    double aspect_ratio,
    const std::string& filename
) {
    double theta = (2 * pi * frame) / static_cast<double>(total_frames);
    point3 lookfrom(13.5*cos(theta), 2.0, 13.5*sin(theta));
    point3 lookat(0, 1, 0);
    vec3 vup(0, 1, 0);
    double vfov = 20.0;
    camera cam(lookfrom, lookat, vup, vfov, aspect_ratio);

    std::ofstream out(filename.c_str());
    out << "P3\n" << image_width << ' ' << image_height << "\n255\n";

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
}

void render_frame_cpu_multithreaded(
    const scene& world,
    int frame,
    int total_frames,
    int image_width,
    int image_height,
    int samples_per_pixel,
    int max_depth,
    double aspect_ratio,
    const std::string& filename
) {
    double theta = (2 * pi * frame) / static_cast<double>(total_frames);
    point3 lookfrom(13.5*cos(theta), 2.0, 13.5*sin(theta));
    point3 lookat(0, 1, 0);
    vec3 vup(0, 1, 0);
    double vfov = 20.0;
    camera cam(lookfrom, lookat, vup, vfov, aspect_ratio);

    std::vector<color> buffer(static_cast<size_t>(image_width) * image_height);

    int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    std::vector<std::thread> threads;
    auto worker = [&](int thread_id) {
        for (int j = image_height - 1 - thread_id; j >= 0; j -= num_threads) {
            for (int i = 0; i < image_width; ++i) {
                color pixel_color(0, 0, 0);
                for (int s = 0; s < samples_per_pixel; ++s) {
                    auto u = (i + random_double()) / (image_width-1);
                    auto v = (j + random_double()) / (image_height-1);
                    ray r = cam.get_ray(u, v);
                    pixel_color += ray_color(r, world, max_depth);
                }
                buffer[static_cast<size_t>(image_height - 1 - j) * image_width + i] = pixel_color;
            }
        }
    };

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(worker, t);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::ofstream out(filename.c_str());
    out << "P3\n" << image_width << ' ' << image_height << "\n255\n";
    for (int j = image_height - 1; j >= 0; --j) {
        for (int i = 0; i < image_width; ++i) {
            color pixel_color = buffer[static_cast<size_t>(image_height - 1 - j) * image_width + i];
            write_color(out, pixel_color, samples_per_pixel);
        }
    }
}

int main(int argc, char** argv) {
    render_settings settings;
    if (!parse_args(argc, argv, settings)) {
        print_usage(argv[0]);
        return 1;
    }

    std::srand(std::time(0));

    const int image_height = static_cast<int>(settings.image_width / settings.aspect_ratio);
    auto world = replicated_scene();

    ensure_frames_directory();

    std::string renderer_desc = (settings.renderer == "cpu") ? "CPU (Single-threaded)" : "CPU (Multithreaded)";
    std::cerr << "Renderer: " << renderer_desc << "\n";

    for (int frame = 0; frame < settings.total_frames; ++frame) {
        std::cerr << "\rRendering frame " << frame+1 << " of " << settings.total_frames << std::flush;

        char filename[32];
        sprintf(filename, "frames/frame%03d.ppm", frame);

        if (settings.renderer == "cpu") {
            render_frame_cpu(
                world,
                frame,
                settings.total_frames,
                settings.image_width,
                image_height,
                settings.samples_per_pixel,
                settings.max_depth,
                settings.aspect_ratio,
                filename
            );
        } else {
            render_frame_cpu_multithreaded(
                world,
                frame,
                settings.total_frames,
                settings.image_width,
                image_height,
                settings.samples_per_pixel,
                settings.max_depth,
                settings.aspect_ratio,
                filename
            );
        }
    }

    std::cerr << "\nDone rendering frames.\n";

    if (settings.make_video) {
        std::cerr << "Creating video...\n";
        system("ffmpeg -framerate 25 -i frames/frame%03d.ppm -vf \"scale=trunc(iw/2)*2:trunc(ih/2)*2\" -c:v libx264 -pix_fmt yuv420p output.mp4 -y");
        std::cerr << "Done! Video saved as output.mp4\n";
    }

    return 0;
}
