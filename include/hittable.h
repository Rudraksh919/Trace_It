#ifndef HITTABLE_H
#define HITTABLE_H

#include "vec3.h"
#include "ray.h"

struct hit_record {
    point3 p;
    vec3 normal;
    double t;
    bool front_face;
    color albedo;
    double fuzz;
    double ir; // index of refraction
    bool is_metal;
    bool is_dielectric;

    inline void set_face_normal(const ray& r, const vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

#endif
