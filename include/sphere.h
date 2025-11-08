#ifndef SPHERE_H
#define SPHERE_H

#include "hittable.h"
#include "vec3.h"
#include "ray.h"

class sphere {
public:
    point3 center;
    double radius;
    color albedo;
    bool is_metal;
    bool is_dielectric;
    double fuzz;
    double ir; // index of refraction

    sphere(point3 _center, double _radius, color _albedo, 
           bool _is_metal = false, bool _is_dielectric = false, 
           double _fuzz = 0.0, double _ir = 1.5)
        : center(_center), radius(_radius), albedo(_albedo),
          is_metal(_is_metal), is_dielectric(_is_dielectric),
          fuzz(_fuzz), ir(_ir) {}

    bool hit(const ray& r, double t_min, double t_max, hit_record& rec) const {
        vec3 oc = r.origin() - center;
        auto a = r.direction().length_squared();
        auto half_b = dot(oc, r.direction());
        auto c = oc.length_squared() - radius * radius;
        auto discriminant = half_b * half_b - a * c;

        if (discriminant < 0) return false;
        auto sqrtd = sqrt(discriminant);

        auto root = (-half_b - sqrtd) / a;
        if (root < t_min || root > t_max) {
            root = (-half_b + sqrtd) / a;
            if (root < t_min || root > t_max)
                return false;
        }

        rec.t = root;
        rec.p = r.at(rec.t);
        vec3 outward_normal = (rec.p - center) / radius;
        rec.set_face_normal(r, outward_normal);
        rec.albedo = albedo;
        rec.is_metal = is_metal;
        rec.is_dielectric = is_dielectric;
        rec.fuzz = fuzz;
        rec.ir = ir;
        return true;
    }
};

#endif
