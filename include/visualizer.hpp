#pragma once

/**
 * @file visualizer.hpp
 * @brief Interactive OpenGL/ImGui visualizer for greedy point label placement.
 *
 * Responsibilities:
 *  - Own window + GL context (GLFW) and immediate-mode GUI (ImGui).
 *  - Manage GPU buffers for points and label quads.
 *  - Re-run monotone label placement when zoom / base size changes.
 *  - Render points, labels, and UI per frame.
 */

#include <vector>
#include <array>
#include <string>
#include "greedy_labeler.hpp"

// Prevent GLFW from including legacy OpenGL headers
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

/**
 * @struct VisualizerConfig
 * @brief Bundles initial data and runtime options for the visualizer.
 *
 * Fields:
 *  - points: input anchor points.
 *  - candidates: label candidates (valid flags updated externally or inside visualizer).
 *  - shaderPath: directory containing shader sources.
 *  - initialBaseSize: optional starting label size (used to seed baseSize_).
 */
struct VisualizerConfig {
    std::vector<std::array<float,2>> points;
    std::vector<LabelCandidate>      candidates;
    std::string                      shaderPath;
    float                            initialBaseSize = 0.02f;
};

/**
 * @class PointLabelVisualizer
 * @brief Runs an interactive render loop showing points and placed labels.
 *
 * Core loop:
 *  - init(): create window, load GL, set up shaders/buffers, init ImGui.
 *  - run(): while window open -> poll, update (placement if needed), renderFrame().
 *  - shutdown(): cleanup GL + ImGui + window.
 *
 * Monotone labeling:
 *  - placeMonotone() uses monoState_ + baseSize_ with greedyPlaceMonotone().
 *  - Called when zoom or baseSize changes or on first initialization.
 */
class PointLabelVisualizer {
public:
    /**
     * @brief Construct from configuration snapshot (copied).
     * @param cfg Visualizer configuration (points, candidates, shader path).
     */
    explicit PointLabelVisualizer(const VisualizerConfig& cfg);

    /**
     * @brief Destructor ensures resources are released (calls shutdown() if needed).
     */
    ~PointLabelVisualizer();

    /**
     * @brief Initialize window, GL loader, shaders, buffers, and ImGui.
     * @return true on success, false on fatal error.
     */
    bool init();

    /**
     * @brief Enter the main event/render loop (blocking until window close).
     */
    void run();

    /**
     * @brief Explicit cleanup (safe to call multiple times).
     */
    void shutdown();

    /**
     * @brief External scroll handler (adjust zoom / base size).
     * @param yoffset Scroll delta (positive = zoom in).
     */
    void onScroll(double yoffset);

private:
    // ---- Init helpers ----
    void initWindow();          ///< Create GLFW window + make context current.
    void initGL();              ///< Load GL symbols (glad) + basic state.
    void initImGui();           ///< Setup ImGui context + bindings.
    void teardownImGui();       ///< Destroy ImGui context.

    // ---- Shaders & buffers ----
    std::string loadFile(const std::string& path) const; ///< Read whole file into string.
    GLuint compileShader(GLenum type, const std::string& src) const; ///< Compile single shader stage.
    GLuint linkProgram(const std::string& vertPath, const std::string& fragPath) const; ///< Build program from files.
    void   loadShaders();        ///< Compile/link point + label programs.
    void   buildPointBuffer();   ///< Create / fill VBO/VAO for points.
    void   buildLabelBuffer();   ///< Create VAO/VBO for label quads (initial).
    void   updateLabelBuffer();  ///< Update label quad data after placement.

    // ---- Frame rendering ----
    void renderFrame();          ///< Draw one frame (points, labels, UI).

    // ---- Monotone placement ----
    void placeMonotone();        ///< Re-run labeling with current baseSize_, updating buffers.

    // ---- State ----
    VisualizerConfig config_;    ///< Original config (copied).
    std::string      shaderDir_; ///< Cached shader directory.

    GLFWwindow* window_ = nullptr; ///< GLFW window handle.

    // Shader programs
    GLuint ptProgram_ = 0;       ///< Program for points.
    GLuint sqProgram_ = 0;       ///< Program for labels.

    // Point geometry
    GLuint ptsVAO_ = 0;
    GLuint ptsVBO_ = 0;

    // Label geometry
    GLuint sqVAO_ = 0;
    GLuint sqVBO_ = 0;

    // Uniform locations
    GLint  uViewPt_ = -1;
    GLint  uViewSq_ = -1;

    // Counts
    int    ptsCount_ = 0;        ///< Number of points.
    int    sqCount_ = 0;         ///< Number of label quads (valid + invalid if drawn).

    // View / interaction
    float  zoom_ = 1.0f;         ///< Current zoom factor (affects baseSize_).
    float  offsetX_ = 0.0f;      ///< Pan offset X (if implemented in shaders).
    float  offsetY_ = 0.0f;      ///< Pan offset Y.

    bool   imguiInited_ = false; ///< ImGui initialization flag.

    // Persistent monotone labeling state
    MonotoneState monoState_;    ///< Greedy monotone placement state.
    float         baseSize_ = 0.02f; ///< Current label side length (UI-controlled).
};
