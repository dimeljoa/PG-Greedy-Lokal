#include "renderer.hpp"

#include <fstream>
#include <sstream>
#include <iostream>

#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>  // for glm::scale, glm::translate, glm::inverse
#include <glm/glm.hpp>

/// Constructor stores the base directory path for shader files.
Renderer::Renderer(const std::string& shaderPath)
    : shaderPath_(shaderPath)
{}

/// Destructor: deletes linked shader programs if they exist.
Renderer::~Renderer() {
    if (pointShader_) glDeleteProgram(pointShader_);
    if (labelShader_) glDeleteProgram(labelShader_);
}

/// Initialize OpenGL function pointers and enable point size control.
/// @returns true if glad loaded successfully, false otherwise.
bool Renderer::initGL() {
    if (!gladLoadGL()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    // Allow shader to set point sizes via gl_PointSize
    glEnable(GL_PROGRAM_POINT_SIZE);
    return true;
}

/// Compile and link point and label shader programs, caching uniform locations.
void Renderer::loadShaders() {
    // Compile vertex and fragment shaders for point rendering
    unsigned int vs = compileShader(shaderPath_ + "/point.vert", GL_VERTEX_SHADER);
    unsigned int fs = compileShader(shaderPath_ + "/point.frag", GL_FRAGMENT_SHADER);
    pointShader_    = linkProgram(vs, fs);
    glDeleteShader(vs);  // shaders no longer needed after linking
    glDeleteShader(fs);
    uViewPoint_     = glGetUniformLocation(pointShader_, "u_view");

    // Compile and link shaders for label outline rendering
    unsigned int lvs = compileShader(shaderPath_ + "/label.vert", GL_VERTEX_SHADER);
    unsigned int lfs = compileShader(shaderPath_ + "/label.frag", GL_FRAGMENT_SHADER);
    labelShader_     = linkProgram(lvs, lfs);
    glDeleteShader(lvs);
    glDeleteShader(lfs);
    uViewLabel_      = glGetUniformLocation(labelShader_, "u_view");
}

/// Reads GLSL source from a file, compiles it, and reports errors.
/// @param filePath Path to the shader source file
/// @param type     GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
/// @return Shader object handle (0 on failure)
unsigned int Renderer::compileShader(const std::string& filePath, unsigned int type) const {
    std::ifstream in(filePath);
    if (!in) {
        std::cerr << "Failed to open shader file: " << filePath << std::endl;
        return 0;
    }
    std::stringstream buf;
    buf << in.rdbuf();
    std::string src = buf.str();
    const char* cstr = src.c_str();

    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &cstr, nullptr);
    glCompileShader(shader);

    // Check compilation status
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compile error (" << filePath << "):\n" << log << std::endl;
    }
    return shader;
}

/// Links a vertex shader and fragment shader into a program, logging errors.
/// @param vert Compiled vertex shader handle
/// @param frag Compiled fragment shader handle
/// @return Program object handle (0 on failure)
unsigned int Renderer::linkProgram(unsigned int vert, unsigned int frag) const {
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    // Check link status
    int success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        std::cerr << "Program link error:\n" << log << std::endl;
    }
    return prog;
}

/// Render points: bind point shader, set view matrix, and draw arrays.
void Renderer::drawPoints(unsigned int pointVao, int pointCount, const glm::mat4& proj) {
    glUseProgram(pointShader_);
    // Upload projection/view matrix to shader uniform
    glUniformMatrix4fv(uViewPoint_, 1, GL_FALSE, &proj[0][0]);
    if (pointCount > 0) {
        glBindVertexArray(pointVao);
        glDrawArrays(GL_POINTS, 0, pointCount);
    }
}

/// Render label outlines in two passes: valid (green) and invalid (red).
void Renderer::drawLabels(unsigned int greenVao, int greenCount,
                          unsigned int redVao,   int redCount,
                          const glm::mat4& proj) {
    glUseProgram(labelShader_);
    glUniformMatrix4fv(uViewLabel_, 1, GL_FALSE, &proj[0][0]);
    if (greenCount > 0) {
        glBindVertexArray(greenVao);
        glDrawArrays(GL_LINES, 0, greenCount);
    }
    if (redCount > 0) {
        glBindVertexArray(redVao);
        glDrawArrays(GL_LINES, 0, redCount);
    }
}

// --- Utility: adjust view matrix for zoom centered on cursor ---
/// Computes a new view matrix by scaling and translating so that
/// the world coordinate beneath the mouse cursor remains fixed.
/// @param window      GLFW window for retrieving cursor and buffer size
/// @param currentView Current view matrix (model-view-projection)
/// @param zoomFactor  Factor by which to scale the view
/// @return Modified view matrix with centered zoom applied
glm::mat4 zoomViewMatrix(GLFWwindow* window, glm::mat4 currentView, float zoomFactor) {
    // Query cursor position and framebuffer size
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);

    // Convert to Normalized Device Coordinates [-1, 1]
    glm::vec2 ndc = glm::vec2((x / w) * 2 - 1, 1 - (y / h) * 2);

    // Transform NDC back to world-space before zoom
    glm::mat4 inverseView = glm::inverse(currentView);
    glm::vec4 worldBeforeZoom = inverseView * glm::vec4(ndc, 0.0f, 1.0f);

    // Apply uniform scaling in view matrix
    currentView = glm::scale(currentView, glm::vec3(zoomFactor, zoomFactor, 1.0f));

    // Recompute world-space coordinates under cursor after zoom
    glm::mat4 newInverse = glm::inverse(currentView);
    glm::vec4 worldAfterZoom = newInverse * glm::vec4(ndc, 0.0f, 1.0f);

    // Compute translation to keep the same point under cursor
    glm::vec2 diff = glm::vec2(worldBeforeZoom - worldAfterZoom);
    currentView = glm::translate(currentView, glm::vec3(diff, 0.0f));

    return currentView;
}
