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
  - Recursive ray bouncing (up to 50 light bounces)
  - Gamma correction for accurate color representation
- **Animation Support**: 360-degree camera rotation with frame-by-frame rendering
- **Optimized Performance**: Efficient hit detection and vector mathematics

## Project Structure

```
Trace_IT/
├── include/              # Header files
│   ├── camera.h         # Camera system with field-of-view control
│   ├── color.h          # Color utilities and output functions
│   ├── hittable.h       # Hit record structure for ray intersections
│   ├── ray.h            # Ray class for light propagation
│   ├── scene.h          # Scene management and object collection
│   ├── sphere.h         # Sphere primitive with material properties
│   ├── utils.h          # Utility functions (random, math, reflection)
│   └── vec3.h           # 3D vector mathematics
├── src/                 # Source files
│   └── main.cpp         # Main rendering loop and scene setup
├── assets/              # Output images
│   └── scene1.ppm       # Sample rendered scene
├── docs/                # Documentation
│   └── WriteUp for the assignment.txt
└── README.md
```

## Technical Implementation

### Core Concepts

- **Object-Oriented Design**: Clean class hierarchy for cameras, rays, materials, and geometric primitives
- **Vector Mathematics**: Custom 3D vector class with dot/cross products, normalization, and transformations
- **Computational Geometry**: Ray-sphere intersection using quadratic formula optimization
- **Data Structures**: Efficient scene representation with std::vector for object management

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
- FFmpeg (optional, for video generation)

### Compilation

```bash
# Compile the raytracer
g++ -std=c++11 -O3 -I./include src/main.cpp -o raytracer.exe

# For faster compilation during development
g++ -std=c++11 -I./include src/main.cpp -o raytracer.exe
```

### Running

```bash
# Run the raytracer (generates animation frames)
./raytracer.exe

# Output will be saved to:
# - frames/frame000.ppm to frames/frame249.ppm
# - output.mp4 (if FFmpeg is installed)
```

## Configuration

Edit `src/main.cpp` to customize rendering parameters:

```cpp
const auto aspect_ratio = 16.0 / 9.0;  // Image aspect ratio
const int image_width = 800;            // Resolution width
const int samples_per_pixel = 100;      // Anti-aliasing samples
const int max_depth = 50;               // Maximum ray bounces
```

## Sample Scene

The included scene features:
- Large ground plane with matte gray material
- Central glass sphere with refractive index 1.5
- 100 randomly placed spheres with varied materials (60% diffuse, 15% metal, 25% glass)
- Rotating camera path (360 degrees over 250 frames at 25 fps)

## Performance

- **Rendering Time**: ~6 hours for full 250-frame animation (800×450 resolution, 100 samples/pixel)
- **Optimization Opportunities**: 
  - Implement BVH (Bounding Volume Hierarchy) for faster intersection tests
  - Add multithreading for parallel pixel rendering
  - Use SIMD instructions for vector operations

## Learning Outcomes

Through this project, I gained expertise in:

- **C++ Programming**: Modern C++11 features, memory management, and optimization
- **Computer Graphics**: Ray tracing algorithms, BRDF models, and rendering equations
- **Linear Algebra**: Vector/matrix operations, transformations, and coordinate systems
- **Software Architecture**: Modular design, separation of concerns, and extensible code structure
- **Algorithm Optimization**: Performance profiling and computational efficiency

## Future Enhancements

- GPU acceleration with CUDA or OpenCL

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
