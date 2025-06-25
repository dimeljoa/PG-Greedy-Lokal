#pragma once

// Prevent GLFW from including OpenGL headers; we'll load with glad
#define GLFW_INCLUDE_NONE
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <string>

/// Renderer wraps shader compilation, program linkage, and draw calls for both
/// data points and label rectangles.
class Renderer {
public:
    /// Construct with the directory path where shader source files reside.
    /// @param shaderPath Base directory for vertex and fragment shader files.
    explicit Renderer(const std::string& shaderPath);

    /// Destructor: cleans up GPU resources (shader programs, buffers).
    ~Renderer();

    /// Initialize OpenGL context state.
    /// - Loads GL function pointers via glad
    /// - Enables point size control
    /// @return true if GL was successfully initialized
    bool initGL();

    /// Load, compile, and link shader programs for points and labels.
    /// Retrieves and stores uniform locations for projection matrices.
    void loadShaders();

    /// Draws a batch of points.
    /// @param pointVao    Vertex Array Object bound with point vertex data
    /// @param pointCount  Number of points to render
    /// @param proj        Projection matrix mapping world to clip space
    void drawPoints(unsigned int pointVao,
                    int             pointCount,
                    const glm::mat4& proj);

    /// Draws label rectangles in two passes:
    /// - Green VAO for valid labels
    /// - Red VAO for invalid labels
    /// @param greenVao    VAO containing geometry for valid-label quads
    /// @param greenCount  Vertex count for valid-label quads
    /// @param redVao      VAO for invalid-label quads
    /// @param redCount    Vertex count for invalid-label quads
    /// @param proj        Projection matrix for transforming quads
    void drawLabels(unsigned int greenVao,
                    int             greenCount,
                    unsigned int    redVao,
                    int             redCount,
                    const glm::mat4& proj);

private:
    std::string shaderPath_;  ///< Directory path to shader files

    unsigned int pointShader_ = 0; ///< GL handle for point shader program
    unsigned int labelShader_ = 0; ///< GL handle for label shader program

    int uViewPoint_ = -1;  ///< Uniform location for point projection matrix
    int uViewLabel_ = -1;  ///< Uniform location for label projection matrix

    /// Compile a shader of given type (GL_VERTEX_SHADER or GL_FRAGMENT_SHADER).
    /// @param filePath Full path to shader source
    /// @param type     Shader type enum
    /// @return GL handle of compiled shader
    unsigned int compileShader(const std::string& filePath, unsigned int type) const;

    /// Link a vertex and fragment shader into a shader program.
    /// @param vert Compiled vertex shader handle
    /// @param frag Compiled fragment shader handle
    /// @return GL handle of linked shader program
    unsigned int linkProgram(unsigned int vert, unsigned int frag) const;
};
