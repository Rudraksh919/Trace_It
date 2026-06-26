cmake_minimum_required(VERSION 3.18)
project(Trace_IT LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CheckLanguage)
check_language(CUDA)

if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    add_executable(raytracer src/main.cpp src/cuda_renderer.cu)
    target_compile_definitions(raytracer PRIVATE TRACE_IT_ENABLE_CUDA)
    set_target_properties(raytracer PROPERTIES
        CUDA_STANDARD 11
        CUDA_STANDARD_REQUIRED ON
        CUDA_SEPARABLE_COMPILATION ON
    )
    message(STATUS "CUDA found: building GPU renderer")
else()
    add_executable(raytracer src/main.cpp)
    message(STATUS "CUDA compiler not found: building CPU-only renderer")
endif()

target_include_directories(raytracer PRIVATE include)

target_compile_options(raytracer PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:
        $<$<CXX_COMPILER_ID:MSVC>:/O2>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O3>
    >
    $<$<COMPILE_LANGUAGE:CUDA>:-O3>
)
