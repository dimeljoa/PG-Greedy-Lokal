#pragma once

/**
 * @file renderer.hpp
 * @brief Lightweight OpenGL renderer for point + label visualization.
 *
 * Responsibilities:
 *  - Own shader program objects for points and labels.
 *  - Provide initialization (GL loader, basic state).
 *  - Provide simple draw entry points for points and colored label quads.
 *
 * Label coloring convention:
 *  - Valid (placed) labels rendered with the "green" VAO.
 *  - Invalid / rejected labels rendered with the "red" VAO.
 */

#define GLFW_INCLUDE_NONE
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <string>

/**
 * @class Renderer
 * @brief Manages shader programs and issues draw calls for points and labels.
 *
 * Usage pattern:
 *  @code
 *    Renderer r(shaderDir);
 *    if (!r.initGL()) return;
 *    r.loadShaders();
 *    r.drawPoints(pointsVao, pointCount, proj);
 *    r.drawLabels(validVao, validCount, invalidVao, invalidCount, proj);
 *  @endcode
 */
class Renderer {
public:
    /**
     * @brief Construct with base directory containing the shader source files.
     * @param shaderPath Directory path (no trailing slash required).
     */
    explicit Renderer(const std::string& shaderPath);

    /**
     * @brief Destructor releases GL resources (if any remain).
     */
    ~Renderer();

    /**
     * @brief Initialize OpenGL state (after a context is current).
     *
     * Loads GL symbols via glad (caller must have created a context),
     * sets point size (if desired), and enables blending for alpha.
     *
     * @return true if initialization succeeded, false otherwise.
     */
    bool initGL();

    /**
     * @brief Compile + link the point and label shader programs.
     *
     * Relies on shaderPath_ to locate required vertex/fragment files.
     * Replaces existing programs if already created.
     */
    void loadShaders();

    /**
     * @brief Draw all points.
     * @param vao   VAO holding point vertex data (e.g., positions).
     * @param count Number of point vertices to draw.
     * @param proj  Projection (view) matrix uniform.
     */
    void drawPoints(unsigned int vao, int count, const glm::mat4& proj);

    /**
     * @brief Draw labels split by validity (green = valid, red = invalid).
     * @param greenVao   VAO containing geometry for valid labels.
     * @param greenCount Vertex (or index) count for valid label quads.
     * @param redVao     VAO containing geometry for invalid labels.
     * @param redCount   Vertex (or index) count for invalid label quads.
     * @param proj       Projection matrix applied to both batches.
     */
    void drawLabels(unsigned int greenVao, int greenCount,
                    unsigned int redVao,   int redCount,
                    const glm::mat4& proj);

private:
    std::string shaderPath_;        ///< Base directory for shader source files.

    unsigned int pointShader_ = 0;  ///< Linked GL program for point rendering.
    unsigned int labelShader_ = 0;  ///< Linked GL program for label rendering.

    int uViewPoint_ = -1;           ///< Uniform location: projection matrix (points).
    int uViewLabel_ = -1;           ///< Uniform location: projection matrix (labels).

    /**
     * @brief Compile a single shader stage from file.
     * @param filePath Absolute or relative path to shader source.
     * @param type     GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
     * @return Shader object ID (0 on failure).
     */
    unsigned int compileShader(const std::string& filePath, unsigned int type) const;

    /**
     * @brief Link vertex + fragment shader objects into a program.
     * @param vert Vertex shader object ID.
     * @param frag Fragment shader object ID.
     * @return Program object ID (0 on failure).
     */
    unsigned int linkProgram(unsigned int vert, unsigned int frag) const;
};
