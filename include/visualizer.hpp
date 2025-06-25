#pragma once

#include <vector>
#include <array>
#include <string>
#include "greedy_labeler.hpp"

// Disable GLFW's OpenGL header; we'll use glad
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

/// Configuration structure for PointLabelVisualizer.
/// - `points`: input 2D points to render.
/// - `candidates`: label candidates with `valid` flags indicating placement.
/// - `shaderPath`: directory containing GLSL shader files.
struct VisualizerConfig {
    std::vector<std::array<float, 2>> points;     ///< Points to visualize
    std::vector<LabelCandidate>       candidates; ///< Label candidates for each point
    std::string                       shaderPath; ///< Base directory for shader files
};

/// `PointLabelVisualizer` sets up an interactive window to display
/// points and their labels, offering controls for zoom, pan, and
/// greedy labeling steps using ImGui.
class PointLabelVisualizer {
public:
    /// Construct with a filled `VisualizerConfig`.
    /// Copies configuration for use in initialization.
    explicit PointLabelVisualizer(const VisualizerConfig& cfg);

    /// Destructor: ensures clean resource shutdown.
    ~PointLabelVisualizer();

    /// Initialize all subsystems:
    /// 1. GLFW window
    /// 2. OpenGL context and glad
    /// 3. ImGui GUI framework
    /// 4. Shader programs and GPU buffers
    /// @return true if all initialization steps succeed
    bool init();

    /// Main application loop:
    /// - Handle input
    /// - Render points, labels, and UI controls
    /// - Update buffers on candidate changes
    void run();

    /// Clean up all allocated resources:
    /// - ImGui
    /// - OpenGL buffers and programs
    /// - GLFW window
    void shutdown();

    /// Scroll callback to adjust zoom centered at mouse position.
    /// @param yoffset Vertical scroll amount (positive to zoom in).
    void onScroll(double yoffset);

private:
    // Initialization helpers
    void initWindow();  ///< Set up GLFW window and callbacks
    void initGL();      ///< Load GL functions and set state (e.g., blending)
    void initImGui();   ///< Initialize ImGui context and bindings
    void teardownImGui(); ///< Clean up ImGui resources

    // Shader, buffer, and UI setup
    std::string loadFile(const std::string& path) const;  ///< Read file into string
    GLuint      compileShader(GLenum type, const std::string& src) const; ///< Compile shader source
    GLuint      linkProgram(const std::string& vertPath, const std::string& fragPath) const; ///< Link vertex+fragment
    void        loadShaders();       ///< Load and link both point and square shaders
    void        buildPointBuffer();  ///< Create VAO/VBO for point data
    void        buildLabelBuffer();  ///< Create VAO/VBO for all label candidates
    void        updateLabelBuffer(); ///< Update VBO when valid flags change

    // Frame rendering logic
    void renderFrame(); ///< Render a single frame: clears, draws, and swaps buffers

    // Application state and handles
    VisualizerConfig config_; ///< Copy of initial configuration
    std::string      shaderDir_; ///< Path to shader directory

    GLFWwindow* window_ = nullptr; ///< GLFW window handle
    GLuint       ptProgram_ = 0;   ///< Shader program for points
    GLuint       sqProgram_ = 0;   ///< Shader program for label rectangles
    GLint        uViewPt_   = -1;  ///< Uniform location for point projection
    GLint        uViewSq_   = -1;  ///< Uniform location for square projection

    GLuint       ptsVAO_   = 0;    ///< VAO for point vertices
    GLuint       ptsVBO_   = 0;    ///< VBO for point positions
    int          ptsCount_ = 0;    ///< Number of points to draw

    GLuint       sqVAO_    = 0;    ///< VAO for square edge vertices
    GLuint       sqVBO_    = 0;    ///< VBO for square geometry
    int          sqCount_  = 0;    ///< Number of line vertices in VBO

    // View transform parameters
    float zoom_    = 1.0f;        ///< Zoom factor (1=no zoom)
    float offsetX_ = 0.0f;        ///< Pan offset in X
    float offsetY_ = 0.0f;        ///< Pan offset in Y
};
