#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include "sphere.h"
#include "ray.h"

class scene {
public:
    std::vector<sphere> objects;

    void add(const sphere& s) {
        objects.push_back(s);
    }

    bool hit(const ray& r, double t_min, double t_max, hit_record& rec) const {
        hit_record temp_rec;
        bool hit_anything = false;
        auto closest = t_max;

        for (const auto& object : objects) {
            if (object.hit(r, t_min, closest, temp_rec)) {
                hit_anything = true;
                closest = temp_rec.t;
                rec = temp_rec;
            }
        }
        return hit_anything;
    }
};

#endif
