# Trace_IT

A photorealistic ray tracer built from scratch in C++ that simulates light interactions including reflection, refraction, and diffuse scattering to render high-quality 3D scenes.

## Overview

This project implements a complete ray tracing pipeline capable of rendering realistic images with multiple material types, recursive light bouncing, and anti-aliasing. The renderer supports animations through a rotating camera system and can generate video output via FFmpeg.

## Features

- **Physically-Based Rendering**: Simulates realistic light behavior with recursive path tracing
- **Multiple Material Types**:
  - Lambertian (diffuse) surfaces with soft, natural light scattering
  - Metallic materials with configurable roughness/fuzz
  - Dielectric (glass) materials with Schlick approximation for realistic refraction
- **Advanced Rendering Techniques**:
  - Multi-sample anti-aliasing (configurable samples per pixel)
  - Recursive ray bouncing (up to 40 light bounces by default)
  - Gamma correction for accurate color representation
- **Animation Support**: 360-degree camera rotation with frame-by-frame rendering
- **Optimized Performance**: Efficient hit detection and vector mathematics
- **Multithreaded CPU Rendering**: Multi-core CPU parallel rendering utilizing standard C++ threads

## Project Structure

```text
Trace_IT/
|-- CMakeLists.txt          # Standard build configuration
|-- include/                # Header files
|   |-- camera.h            # Camera system with field-of-view control
|   |-- color.h             # Color utilities and output functions
|   |-- hittable.h          # Hit record structure for ray intersections
|   |-- ray.h               # Ray class for light propagation
|   |-- scene.h             # Scene management and object collection
|   |-- sphere.h            # Sphere primitive with material properties
|   |-- utils.h             # Utility functions (random, math, reflection)
|   `-- vec3.h              # 3D vector mathematics
|-- src/                    # Source files
|   `-- main.cpp            # CPU renderer, CLI, scene setup
|-- assets/                 # Output images
|   `-- scene1.ppm          # Sample rendered scene
|-- docs/                   # Documentation
|   `-- WriteUp for the assignment.txt
`-- README.md
```

## Technical Implementation

### Core Concepts

- **Object-Oriented Design**: Clean class hierarchy for cameras, rays, materials, and geometric primitives
- **Vector Mathematics**: Custom 3D vector class with dot/cross products, normalization, and transformations
- **Computational Geometry**: Ray-sphere intersection using quadratic formula optimization
- **Data Structures**: Efficient scene representation with std::vector for object management
- **Multithreading**: Parallel per-pixel rendering on the CPU utilising all available hardware threads

### Rendering Pipeline

1. **Scene Setup**: Create objects with positions, materials, and properties
2. **Camera Configuration**: Set viewing frustum, position, and orientation
3. **Ray Generation**: Cast rays through each pixel with anti-aliasing samples
4. **Intersection Testing**: Find closest object hit along ray path
5. **Material Shading**: Calculate color based on material properties and recursive bounces
6. **Output**: Write PPM image format with gamma-corrected colors

## Building and Running

### Prerequisites

- C++ compiler with C++11 support (g++, clang++, or MSVC)
- CMake 3.18 or newer
- FFmpeg (optional, for video generation)

### Compilation

```bash

cmake -S . -B build
cmake --build build --config Release
```

### Running

```bash
# Run with multithreaded CPU rendering (default)
./build/raytracer --renderer cpu_multithreaded

# Force single-threaded CPU rendering.
./build/raytracer --renderer cpu

# Quick smoke test without video generation.
./build/raytracer --renderer cpu_multithreaded --frames 1 --width 320 --samples 4 --depth 8 --no-video
```

Output will be saved to:

- `frames/frame000.ppm` to `frames/frame249.ppm`
- `output.mp4` if FFmpeg is installed and `--no-video` is not used

## Configuration

You can change defaults in `src/main.cpp`, or use CLI flags for common settings:

```bash
.\build\Release\raytracer.exe --renderer cpu_multithreaded --width 1080 --samples 100 --depth 40 --frames 250
```

## Sample Scene

The included scene features:

- Large ground plane with matte gray material
- Central glass sphere with refractive index 1.5
- Randomly placed spheres with varied diffuse, metal, and dielectric materials
- Rotating camera path over 250 frames at 25 fps

## Performance

- **Rendering Time**: Single-threaded rendering can take hours; our new multithreaded renderer reduces this drastically depending on your CPU core count.
- **Optimization Opportunities**:
  - Implement BVH (Bounding Volume Hierarchy) for faster intersection tests
  - Use SIMD (SSE/AVX) instructions for vectorized calculations

## Learning Outcomes

Through this project, I gained expertise in:

- **C++ Programming**: Modern C++11 features, multithreading, memory management, and optimization
- **Computer Graphics**: Ray tracing algorithms, BRDF models, and rendering equations
- **Linear Algebra**: Vector/matrix operations, transformations, and coordinate systems
- **Software Architecture**: Modular design, separation of concerns, and extensible code structure
- **Algorithm Optimization**: Performance profiling and computational efficiency

## Future Enhancements

- BVH acceleration structure for faster CPU intersections
- Additional primitives and materials

## Output Format

Images are saved in PPM (Portable Pixmap) format, which can be converted to other formats:

```bash
# Convert PPM to PNG using ImageMagick
convert scene1.ppm scene1.png

# Or view directly with compatible viewers
feh scene1.ppm  # Linux
open scene1.ppm # macOS
```

## References

- *Ray Tracing in One Weekend* series by Peter Shirley

