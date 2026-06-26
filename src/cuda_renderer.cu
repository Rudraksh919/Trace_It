#include "../include/cuda_renderer.h"
#include "../include/utils.h"

#include <cuda_runtime.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

struct GpuVec3 {
    double x;
    double y;
    double z;
};

struct GpuRay {
    GpuVec3 origin;
    GpuVec3 direction;
};

struct GpuSphere {
    GpuVec3 center;
    double radius;
    GpuVec3 albedo;
    int is_metal;
    int is_dielectric;
    double fuzz;
    double ir;
};

struct GpuHitRecord {
    GpuVec3 p;
    GpuVec3 normal;
    double t;
    int front_face;
    GpuVec3 albedo;
    double fuzz;
    double ir;
    int is_metal;
    int is_dielectric;
};

struct GpuCamera {
    GpuVec3 origin;
    GpuVec3 lower_left_corner;
    GpuVec3 horizontal;
    GpuVec3 vertical;
};

__host__ __device__ GpuVec3 make_vec(double x, double y, double z) {
    GpuVec3 v{x, y, z};
    return v;
}

__host__ __device__ GpuVec3 operator+(GpuVec3 a, GpuVec3 b) {
    return make_vec(a.x + b.x, a.y + b.y, a.z + b.z);
}

__host__ __device__ GpuVec3 operator-(GpuVec3 a, GpuVec3 b) {
    return make_vec(a.x - b.x, a.y - b.y, a.z - b.z);
}

__host__ __device__ GpuVec3 operator-(GpuVec3 a) {
    return make_vec(-a.x, -a.y, -a.z);
}

__host__ __device__ GpuVec3 operator*(GpuVec3 a, GpuVec3 b) {
    return make_vec(a.x * b.x, a.y * b.y, a.z * b.z);
}

__host__ __device__ GpuVec3 operator*(double t, GpuVec3 a) {
    return make_vec(t * a.x, t * a.y, t * a.z);
}

__host__ __device__ GpuVec3 operator*(GpuVec3 a, double t) {
    return t * a;
}

__host__ __device__ GpuVec3 operator/(GpuVec3 a, double t) {
    return (1.0 / t) * a;
}

__host__ __device__ double dot(GpuVec3 a, GpuVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

__host__ __device__ GpuVec3 cross(GpuVec3 a, GpuVec3 b) {
    return make_vec(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

__host__ __device__ double length_squared(GpuVec3 a) {
    return dot(a, a);
}

__host__ __device__ double length(GpuVec3 a) {
    return sqrt(length_squared(a));
}

__host__ __device__ GpuVec3 unit_vector(GpuVec3 a) {
    return a / length(a);
}

__device__ bool near_zero(GpuVec3 a) {
    const double s = 1e-8;
    return fabs(a.x) < s && fabs(a.y) < s && fabs(a.z) < s;
}

__device__ GpuVec3 ray_at(GpuRay r, double t) {
    return r.origin + t * r.direction;
}

__device__ unsigned int xorshift32(unsigned int& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

__device__ double random_double(unsigned int& state) {
    return (xorshift32(state) + 1.0) * 2.328306435454494e-10;
}

__device__ double random_double(unsigned int& state, double min_value, double max_value) {
    return min_value + (max_value - min_value) * random_double(state);
}

__device__ GpuVec3 random_vec(unsigned int& state, double min_value, double max_value) {
    return make_vec(
        random_double(state, min_value, max_value),
        random_double(state, min_value, max_value),
        random_double(state, min_value, max_value)
    );
}

__device__ GpuVec3 random_in_unit_sphere(unsigned int& state) {
    while (true) {
        GpuVec3 p = random_vec(state, -1.0, 1.0);
        if (length_squared(p) < 1.0) {
            return p;
        }
    }
}

__device__ GpuVec3 random_unit_vector(unsigned int& state) {
    return unit_vector(random_in_unit_sphere(state));
}

__device__ GpuVec3 reflect(GpuVec3 v, GpuVec3 n) {
    return v - 2.0 * dot(v, n) * n;
}

__device__ GpuVec3 refract(GpuVec3 uv, GpuVec3 n, double etai_over_etat) {
    double cos_theta = fmin(dot(-uv, n), 1.0);
    GpuVec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    GpuVec3 r_out_parallel = -sqrt(fabs(1.0 - length_squared(r_out_perp))) * n;
    return r_out_perp + r_out_parallel;
}

__device__ double reflectance(double cosine, double ref_idx) {
    double r0 = (1.0 - ref_idx) / (1.0 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cosine, 5.0);
}

__device__ bool hit_sphere(const GpuSphere& sphere, GpuRay r, double t_min, double t_max, GpuHitRecord& rec) {
    GpuVec3 oc = r.origin - sphere.center;
    double a = length_squared(r.direction);
    double half_b = dot(oc, r.direction);
    double c = length_squared(oc) - sphere.radius * sphere.radius;
    double discriminant = half_b * half_b - a * c;

    if (discriminant < 0.0) {
        return false;
    }

    double sqrtd = sqrt(discriminant);
    double root = (-half_b - sqrtd) / a;
    if (root < t_min || root > t_max) {
        root = (-half_b + sqrtd) / a;
        if (root < t_min || root > t_max) {
            return false;
        }
    }

    rec.t = root;
    rec.p = ray_at(r, rec.t);
    GpuVec3 outward_normal = (rec.p - sphere.center) / sphere.radius;
    rec.front_face = dot(r.direction, outward_normal) < 0.0;
    rec.normal = rec.front_face ? outward_normal : -outward_normal;
    rec.albedo = sphere.albedo;
    rec.is_metal = sphere.is_metal;
    rec.is_dielectric = sphere.is_dielectric;
    rec.fuzz = sphere.fuzz;
    rec.ir = sphere.ir;
    return true;
}

__device__ bool hit_world(const GpuSphere* spheres, int sphere_count, GpuRay r, double t_min, double t_max, GpuHitRecord& rec) {
    GpuHitRecord temp_rec;
    bool hit_anything = false;
    double closest = t_max;

    for (int i = 0; i < sphere_count; ++i) {
        if (hit_sphere(spheres[i], r, t_min, closest, temp_rec)) {
            hit_anything = true;
            closest = temp_rec.t;
            rec = temp_rec;
        }
    }

    return hit_anything;
}

__device__ GpuRay get_camera_ray(const GpuCamera& camera, double s, double t) {
    GpuRay ray;
    ray.origin = camera.origin;
    ray.direction = camera.lower_left_corner + s * camera.horizontal + t * camera.vertical - camera.origin;
    return ray;
}

__device__ GpuVec3 ray_color(GpuRay ray, const GpuSphere* spheres, int sphere_count, int max_depth, unsigned int& rng_state) {
    GpuVec3 attenuation = make_vec(1.0, 1.0, 1.0);

    for (int depth = 0; depth < max_depth; ++depth) {
        GpuHitRecord rec;
        if (hit_world(spheres, sphere_count, ray, 0.001, 1.0e30, rec)) {
            if (rec.is_dielectric) {
                double refraction_ratio = rec.front_face ? (1.0 / rec.ir) : rec.ir;
                GpuVec3 unit_direction = unit_vector(ray.direction);
                double cos_theta = fmin(dot(-unit_direction, rec.normal), 1.0);
                double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
                bool cannot_refract = refraction_ratio * sin_theta > 1.0;
                GpuVec3 direction;

                if (cannot_refract || reflectance(cos_theta, refraction_ratio) > random_double(rng_state)) {
                    direction = reflect(unit_direction, rec.normal);
                } else {
                    direction = refract(unit_direction, rec.normal, refraction_ratio);
                }

                ray.origin = rec.p;
                ray.direction = direction;
            } else if (rec.is_metal) {
                GpuVec3 reflected = reflect(unit_vector(ray.direction), rec.normal);
                ray.origin = rec.p;
                ray.direction = reflected + rec.fuzz * random_in_unit_sphere(rng_state);
                attenuation = attenuation * rec.albedo;
            } else {
                GpuVec3 scatter_direction = rec.normal + random_unit_vector(rng_state);
                if (near_zero(scatter_direction)) {
                    scatter_direction = rec.normal;
                }
                ray.origin = rec.p;
                ray.direction = scatter_direction;
                attenuation = attenuation * rec.albedo;
            }
        } else {
            GpuVec3 unit_direction = unit_vector(ray.direction);
            double t = 0.5 * (unit_direction.y + 1.0);
            GpuVec3 sky = (1.0 - t) * make_vec(1.0, 1.0, 1.0) + t * make_vec(0.5, 0.7, 1.0);
            return attenuation * sky;
        }
    }

    return make_vec(0.0, 0.0, 0.0);
}

__device__ unsigned char to_byte(double value, int samples_per_pixel) {
    double scale = 1.0 / samples_per_pixel;
    double corrected = sqrt(scale * value);
    corrected = fmin(fmax(corrected, 0.0), 0.999);
    return static_cast<unsigned char>(256.0 * corrected);
}

__global__ void render_kernel(
    unsigned char* pixels,
    int image_width,
    int image_height,
    int samples_per_pixel,
    int max_depth,
    GpuCamera camera,
    const GpuSphere* spheres,
    int sphere_count,
    unsigned int seed
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= image_width || y >= image_height) {
        return;
    }

    int output_y = image_height - 1 - y;
    int pixel_index = (output_y * image_width + x) * 3;
    unsigned int rng_state = seed ^ (x * 1973u) ^ (y * 9277u) ^ 0x9e3779b9u;
    GpuVec3 pixel_color = make_vec(0.0, 0.0, 0.0);

    for (int s = 0; s < samples_per_pixel; ++s) {
        double u = (x + random_double(rng_state)) / (image_width - 1);
        double v = (y + random_double(rng_state)) / (image_height - 1);
        GpuRay ray = get_camera_ray(camera, u, v);
        pixel_color = pixel_color + ray_color(ray, spheres, sphere_count, max_depth, rng_state);
    }

    pixels[pixel_index + 0] = to_byte(pixel_color.x, samples_per_pixel);
    pixels[pixel_index + 1] = to_byte(pixel_color.y, samples_per_pixel);
    pixels[pixel_index + 2] = to_byte(pixel_color.z, samples_per_pixel);
}

GpuVec3 to_gpu_vec(const vec3& value) {
    return make_vec(value.x(), value.y(), value.z());
}

GpuCamera make_camera(int frame, int total_frames, double aspect_ratio) {
    double theta = (2.0 * pi * frame) / static_cast<double>(total_frames);
    GpuVec3 lookfrom = make_vec(13.5 * cos(theta), 2.0, 13.5 * sin(theta));
    GpuVec3 lookat = make_vec(0.0, 1.0, 0.0);
    GpuVec3 vup = make_vec(0.0, 1.0, 0.0);
    double vfov = 20.0;
    double angle = degrees_to_radians(vfov);
    double h = tan(angle / 2.0);
    double viewport_height = 2.0 * h;
    double viewport_width = aspect_ratio * viewport_height;

    GpuVec3 w = unit_vector(lookfrom - lookat);
    GpuVec3 u = unit_vector(cross(vup, w));
    GpuVec3 v = cross(w, u);

    GpuCamera camera;
    camera.origin = lookfrom;
    camera.horizontal = viewport_width * u;
    camera.vertical = viewport_height * v;
    camera.lower_left_corner = camera.origin - camera.horizontal / 2.0 - camera.vertical / 2.0 - w;
    return camera;
}

std::vector<GpuSphere> copy_scene_to_gpu_format(const scene& world) {
    std::vector<GpuSphere> spheres;
    spheres.reserve(world.objects.size());

    for (const auto& object : world.objects) {
        GpuSphere sphere;
        sphere.center = to_gpu_vec(object.center);
        sphere.radius = object.radius;
        sphere.albedo = to_gpu_vec(object.albedo);
        sphere.is_metal = object.is_metal ? 1 : 0;
        sphere.is_dielectric = object.is_dielectric ? 1 : 0;
        sphere.fuzz = object.fuzz;
        sphere.ir = object.ir;
        spheres.push_back(sphere);
    }

    return spheres;
}

bool check_cuda(cudaError_t result, const char* operation) {
    if (result == cudaSuccess) {
        return true;
    }

    std::cerr << operation << " failed: " << cudaGetErrorString(result) << "\n";
    return false;
}

} // namespace

bool cuda_renderer_available() {
    int device_count = 0;
    cudaError_t result = cudaGetDeviceCount(&device_count);
    return result == cudaSuccess && device_count > 0;
}

bool render_frame_cuda(
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
    std::vector<GpuSphere> host_spheres = copy_scene_to_gpu_format(world);
    if (host_spheres.empty()) {
        std::cerr << "CUDA renderer needs at least one sphere in the scene.\n";
        return false;
    }

    GpuSphere* device_spheres = nullptr;
    unsigned char* device_pixels = nullptr;
    const size_t spheres_bytes = host_spheres.size() * sizeof(GpuSphere);
    const size_t pixels_bytes = static_cast<size_t>(image_width) * image_height * 3;

    if (!check_cuda(cudaMalloc(&device_spheres, spheres_bytes), "cudaMalloc(spheres)")) {
        return false;
    }
    if (!check_cuda(cudaMalloc(&device_pixels, pixels_bytes), "cudaMalloc(pixels)")) {
        cudaFree(device_spheres);
        return false;
    }
    if (!check_cuda(cudaMemcpy(device_spheres, host_spheres.data(), spheres_bytes, cudaMemcpyHostToDevice), "cudaMemcpy(spheres)")) {
        cudaFree(device_pixels);
        cudaFree(device_spheres);
        return false;
    }

    GpuCamera camera = make_camera(frame, total_frames, aspect_ratio);
    dim3 block(16, 16);
    dim3 grid((image_width + block.x - 1) / block.x, (image_height + block.y - 1) / block.y);
    unsigned int seed = 1337u + static_cast<unsigned int>(frame * 9781);

    render_kernel<<<grid, block>>>(
        device_pixels,
        image_width,
        image_height,
        samples_per_pixel,
        max_depth,
        camera,
        device_spheres,
        static_cast<int>(host_spheres.size()),
        seed
    );

    if (!check_cuda(cudaGetLastError(), "render_kernel launch")) {
        cudaFree(device_pixels);
        cudaFree(device_spheres);
        return false;
    }
    if (!check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize")) {
        cudaFree(device_pixels);
        cudaFree(device_spheres);
        return false;
    }

    std::vector<unsigned char> host_pixels(pixels_bytes);
    if (!check_cuda(cudaMemcpy(host_pixels.data(), device_pixels, pixels_bytes, cudaMemcpyDeviceToHost), "cudaMemcpy(pixels)")) {
        cudaFree(device_pixels);
        cudaFree(device_spheres);
        return false;
    }

    cudaFree(device_pixels);
    cudaFree(device_spheres);

    std::ofstream out(filename.c_str());
    if (!out) {
        std::cerr << "Failed to open " << filename << " for writing.\n";
        return false;
    }

    out << "P3\n" << image_width << ' ' << image_height << "\n255\n";
    for (size_t i = 0; i < host_pixels.size(); i += 3) {
        out << static_cast<int>(host_pixels[i]) << ' '
            << static_cast<int>(host_pixels[i + 1]) << ' '
            << static_cast<int>(host_pixels[i + 2]) << '\n';
    }

    return true;
}
