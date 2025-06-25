cmake_minimum_required(VERSION 3.14)
project(MyLabelerProject LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
# External dependencies
# ----------------------------------------------------------------------------
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm REQUIRED)


# Copy the entire shaders/ folder into the build tree
file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})



include(FetchContent)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.89.2
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_s
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_s PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_s PUBLIC glfw OpenGL::GL)

# ----------------------------------------------------------------------------
# Vendored GLAD
# ----------------------------------------------------------------------------
set(GLAD_C_FILE "${CMAKE_SOURCE_DIR}/src/glad.c")
if(NOT EXISTS "${GLAD_C_FILE}")
  message(FATAL_ERROR "Cannot find glad.c at ${GLAD_C_FILE}; please copy it into src/")
endif()

add_library(glad STATIC "${GLAD_C_FILE}")
set_target_properties(glad PROPERTIES LINKER_LANGUAGE C)
target_include_directories(glad PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)

# ----------------------------------------------------------------------------
# Our library
# ----------------------------------------------------------------------------
include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(MyLabelerLib
  src/camera.cpp
  src/renderer.cpp
  src/ui_controls.cpp
  src/greedy_labeler.cpp
  src/visualizer.cpp
)

target_link_libraries(MyLabelerLib
  PUBLIC
    glad
    glfw
    OpenGL::GL
    glm::glm
    imgui_s
)

# expose ImGui headers to code that includes visualizer.hpp
target_include_directories(MyLabelerLib PUBLIC
  ${imgui_SOURCE_DIR}             # for "imgui.h", "imgui.cpp"
  ${imgui_SOURCE_DIR}/backends    # for "imgui_impl_glfw.h", "imgui_impl_opengl3.h"
)


# ----------------------------------------------------------------------------
# Example executable
# ----------------------------------------------------------------------------
add_executable(labeler_example
  example/main.cpp
)

# Link the library to the example
target_link_libraries(labeler_example
  PRIVATE MyLabelerLib
)

# Copy the shaders folder into the build directory so your runtime can find them
file(
  COPY ${CMAKE_SOURCE_DIR}/shaders
  DESTINATION ${CMAKE_BINARY_DIR}
)