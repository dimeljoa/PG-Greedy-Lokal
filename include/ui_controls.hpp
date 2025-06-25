#pragma once

// Prevent GLFW from including its own GL headers; we'll use glad
#define GLFW_INCLUDE_NONE
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <string>

// Include labeler definitions for UI control actions
#include "greedy_labeler.hpp"
#include <vector>
#include <array>

namespace ui_controls {
    /// Slider control for zoom level adjustment.
    /// @param zoomLevel Current zoom factor, modified by the slider.
    /// @return true if the zoom level changed.
    bool Slider_Zoom(float& zoomLevel);

    /// Button to remove any label candidates marked invalid.
    /// @param candidates Vector of label candidates (modified to remove invalid entries).
    /// @return true if any candidates were removed.
    bool Button_RemoveInvalid(std::vector<LabelCandidate>& candidates);

    /// Button to generate a new set of random points and their label candidates.
    /// @param points      Vector to populate with new (x,y) points.
    /// @param candidates  Vector to populate with new LabelCandidate objects.
    /// @param count       Number of points to generate (default 200).
    /// @return true if generation occurred.
    bool Button_GeneratePoints(
        std::vector<std::array<float,2>>& points,
        std::vector<LabelCandidate>&     candidates,
        int count = 200
    );

    /// Button to run one iteration of the greedy labeling step.
    /// @param candidates Vector of LabelCandidate to process (valid flags updated).
    /// @return true if the greedy step placed any new labels.
    bool Button_RunGreedyStep(std::vector<LabelCandidate>& candidates);
}

/// Renderer handles compilation of GLSL shaders and issuing draw calls for
/// both points and label outlines within an OpenGL context.
class Renderer {
public:
    /// Constructor: supply the directory path (without trailing slash) containing GLSL files.
    explicit Renderer(const std::string& shaderPath);

    /// Destructor: frees allocated shader program resources.
    ~Renderer();

    /// Initializes OpenGL:
    /// - Loads OpenGL function pointers via glad
    /// - Enables programmable point size
    /// @returns true on successful initialization
    bool initGL();

    /// Loads and compiles vertex/fragment shader pairs for:
    ///  - point rendering program
    ///  - label edge rendering program
    /// Must be called after initGL().
    void loadShaders();

    /// Draws points stored in a VAO using the point shader.
    /// @param pointVao    VAO bound with 2D point positions
    /// @param pointCount  Number of points to render
    /// @param proj        Projection/View matrix to apply
    void drawPoints(GLuint pointVao,
                    int    pointCount,
                    const glm::mat4& proj);

    /// Draws label rectangles as line loops in two passes:
    ///  - greenVAO: valid labels
    ///  - redVao: invalid labels
    /// @param greenVao    VAO for valid-label line geometry
    /// @param greenCount  Number of line vertices in greenVao
    /// @param redVao      VAO for invalid-label line geometry
    /// @param redCount    Number of vertices in redVao
    /// @param proj        Projection/View matrix to apply
    void drawLabels(GLuint greenVao,
                    int    greenCount,
                    GLuint redVao,
                    int    redCount,
                    const glm::mat4& proj);

private:
    std::string shaderPath_;   ///< Directory path to shader sources

    GLuint pointShader_ = 0;   ///< OpenGL handle for point shader program
    GLuint labelShader_ = 0;   ///< OpenGL handle for label shader program

    GLint uViewPoint_ = -1;    ///< Uniform location for projection matrix in point shader
    GLint uViewLabel_ = -1;    ///< Uniform location in label shader

    /// Helper: load GLSL source from file and compile into a shader object.
    /// @param filePath Path to GLSL file
    /// @param type     GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
    /// @return Shader object handle
    GLuint compileShader(const std::string& filePath, GLenum type) const;

    /// Helper: link a compiled vertex and fragment shader into a shader program.
    /// @param vert Compiled vertex shader handle
    /// @param frag Compiled fragment shader handle
    /// @return Linked shader program handle
    GLuint linkProgram(GLuint vert, GLuint frag) const;
};
