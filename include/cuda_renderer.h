#ifndef CUDA_RENDERER_H
#define CUDA_RENDERER_H

#include "scene.h"

#include <string>

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
);

bool cuda_renderer_available();

#endif
