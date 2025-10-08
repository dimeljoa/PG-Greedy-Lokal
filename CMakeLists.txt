cmake_minimum_required(VERSION 3.10)
project(PointLabeler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release if user didn't set it
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ENABLE_LTO "Enable link-time optimization" OFF)

if(MSVC)
  add_compile_options(/O2 /DNDEBUG)
else()
  add_compile_options(-O3 -DNDEBUG -march=native)
endif()

# GCC/MinGW LTO (optional, OFF by default)
if(ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_msg)
  if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    # CMake 3.10 compatibility (no add_link_options)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto=auto")
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}   -flto=auto")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE    "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto=auto -Wl,--plugin-opt=-lto-partitions=8")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -flto=auto -Wl,--plugin-opt=-lto-partitions=8")
  endif()
endif()

# Includes
include_directories(${CMAKE_SOURCE_DIR}/include)

# Sources
set(SOURCES
    src/visualizer.cpp
    src/ui_controls.cpp
    src/greedy_labeler.cpp
    src/camera.cpp
    example/main.cpp
)

# ImGui sources
set(IMGUI_SOURCES
    src/imgui.cpp
    src/imgui_demo.cpp
    src/imgui_draw.cpp
    src/imgui_tables.cpp
    src/imgui_widgets.cpp
    src/imgui_impl_glfw.cpp
    src/imgui_impl_opengl3.cpp
)

# Optional: example
file(GLOB EXAMPLE_SOURCES "example/*.cpp")

# Find packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# GLAD
if(EXISTS "${CMAKE_SOURCE_DIR}/src/glad.c")
    add_library(glad STATIC src/glad.c)
    target_include_directories(glad PUBLIC include)
endif()

# ImGui
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC include)
target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
target_link_libraries(imgui PRIVATE glfw OpenGL::GL)

# Core library (no GUI deps) for algorithm/CLI
add_library(LabelerCore STATIC src/greedy_labeler.cpp)
target_include_directories(LabelerCore PUBLIC include)

# Main library (visual app) depends on core + GUI deps
add_library(MyLabelerLib ${SOURCES})
target_link_libraries(MyLabelerLib PUBLIC LabelerCore imgui glfw OpenGL::GL glm::glm)
if(TARGET glad)
  target_link_libraries(MyLabelerLib PUBLIC glad)
endif()

# Example exe (if any)
if(EXAMPLE_SOURCES)
    add_executable(labeler_example ${EXAMPLE_SOURCES})
    target_link_libraries(labeler_example PRIVATE MyLabelerLib)
endif()

if(TARGET labeler_example)
  add_custom_command(TARGET labeler_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/shaders"
            "$<TARGET_FILE_DIR:labeler_example>/shaders")
endif()

# CLI tools (no OpenGL/GLFW linkage)
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/labeler_cli.cpp")
  add_executable(labeler_cli tools/labeler_cli.cpp)
  target_link_libraries(labeler_cli PRIVATE LabelerCore)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tools/experiment_cli.cpp")
  add_executable(experiment_cli tools/experiment_cli.cpp)
  target_link_libraries(experiment_cli PRIVATE LabelerCore)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tools/csv_labeler.cpp")
  add_executable(csv_labeler tools/csv_labeler.cpp)
  target_link_libraries(csv_labeler PRIVATE LabelerCore)
endif()
