// src/renderer.cpp
#include "renderer.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>  // scale, translate, inverse

// ============================================================================
// Constructor / Destructor
// ============================================================================
Renderer::Renderer(const std::string& shaderPath)
    : shaderPath_(shaderPath) {}

Renderer::~Renderer() {
    if (pointShader_) glDeleteProgram(pointShader_);
    if (labelShader_) glDeleteProgram(labelShader_);
}

// ============================================================================
// GL Initialization
// ============================================================================
bool Renderer::initGL() {
    if (!gladLoadGL()) {
        std::cerr << "[Renderer] Failed to initialize GLAD" << std::endl;
        return false;
    }
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

// ============================================================================
// Shader Management
// ============================================================================
void Renderer::loadShaders() {
    auto safeLink = [&](const std::string& vsPath,
                        const std::string& fsPath,
                        unsigned int& dstProgram,
                        int& uViewLoc,
                        const char* uniformName)->bool {
        unsigned int vs = compileShader(vsPath, GL_VERTEX_SHADER);
        if (!vs) return false;
        unsigned int fs = compileShader(fsPath, GL_FRAGMENT_SHADER);
        if (!fs) { glDeleteShader(vs); return false; }

        unsigned int prog = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
        if (!prog) return false;

        // Replace existing only on success
        if (dstProgram) glDeleteProgram(dstProgram);
        dstProgram = prog;
        uViewLoc = glGetUniformLocation(dstProgram, uniformName);
        if (uViewLoc < 0) {
            std::cerr << "[Renderer] Warning: uniform '" << uniformName
                      << "' not found in " << vsPath << "/" << fsPath << "\n";
        }
        return true;
    };

    safeLink(shaderPath_ + "/point.vert",
             shaderPath_ + "/point.frag",
             pointShader_, uViewPoint_, "u_view");

    safeLink(shaderPath_ + "/label.vert",
             shaderPath_ + "/label.frag",
             labelShader_, uViewLabel_, "u_view");
}

unsigned int Renderer::compileShader(const std::string& filePath, unsigned int type) const {
    std::ifstream in(filePath, std::ios::binary);
    if (!in) {
        std::cerr << "[Renderer] Cannot open shader file: " << filePath << std::endl;
        return 0;
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    std::string src = oss.str();
    if (src.empty()) {
        std::cerr << "[Renderer] Empty shader source: " << filePath << std::endl;
        return 0;
    }

    const char* cstr = src.c_str();
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &cstr, nullptr);
    glCompileShader(shader);

    int ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        int len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? len : 1);
        glGetShaderInfoLog(shader, (GLsizei)log.size(), nullptr, log.data());
        std::cerr << "[Renderer] Compile fail (" << filePath << "):\n" << log.data() << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

unsigned int Renderer::linkProgram(unsigned int vert, unsigned int frag) const {
    if (!vert || !frag) return 0;
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    int ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        int len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? len : 1);
        glGetProgramInfoLog(prog, (GLsizei)log.size(), nullptr, log.data());
        std::cerr << "[Renderer] Link fail:\n" << log.data() << std::endl;
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// ============================================================================
// Rendering
// ============================================================================
void Renderer::drawPoints(unsigned int pointVao, int pointCount, const glm::mat4& proj) {
    if (!pointShader_ || pointCount <= 0) return;
    glUseProgram(pointShader_);
    glUniformMatrix4fv(uViewPoint_, 1, GL_FALSE, &proj[0][0]);
    glBindVertexArray(pointVao);
    glDrawArrays(GL_POINTS, 0, pointCount);
    glBindVertexArray(0);
}

void Renderer::drawLabels(unsigned int greenVao, int greenCount,
                          unsigned int redVao,   int redCount,
                          const glm::mat4& proj) {
    if (!labelShader_) return;
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
    glBindVertexArray(0);
}

// ============================================================================
// Zoom Utility
// ============================================================================
glm::mat4 zoomViewMatrix(GLFWwindow* window,
                         glm::mat4   currentView,
                         float       zoomFactor) {
    // Query cursor + framebuffer
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);

    // Mouse in Normalized Device Coordinates [-1, 1]
    glm::vec2 ndc((x / w) * 2.0 - 1.0, 1.0 - (y / h) * 2.0);

    // World coords under cursor (before zoom)
    glm::mat4 invView = glm::inverse(currentView);
    glm::vec4 worldBefore = invView * glm::vec4(ndc, 0.0f, 1.0f);

    // Apply uniform scaling
    currentView = glm::scale(currentView, glm::vec3(zoomFactor, zoomFactor, 1.0f));

    // World coords under cursor (after zoom)
    glm::mat4 newInv = glm::inverse(currentView);
    glm::vec4 worldAfter = newInv * glm::vec4(ndc, 0.0f, 1.0f);

    // Translate to keep same world point under cursor
    glm::vec2 diff = glm::vec2(worldBefore - worldAfter);
    currentView = glm::translate(currentView, glm::vec3(diff, 0.0f));

    return currentView;
}
