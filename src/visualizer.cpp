// src/visualizer.cpp
#include "visualizer.hpp"
#include "ui_controls.hpp"
#include "greedy_labeler.hpp"

#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>   // std::any_of, std::max
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cmath>        // ADD THIS

// -----------------------------------------------------------------------------
// GLFW callbacks
// -----------------------------------------------------------------------------
static void glfwScrollCallback(GLFWwindow* win, double xoff, double yoff) {
    if (auto* vis = static_cast<PointLabelVisualizer*>(glfwGetWindowUserPointer(win))) {
        vis->onScroll(yoff);
    }
}

// -----------------------------------------------------------------------------
// Ctor / Dtor
// -----------------------------------------------------------------------------
PointLabelVisualizer::PointLabelVisualizer(const VisualizerConfig& cfg)
    : config_(cfg)
    , shaderDir_(cfg.shaderPath.empty() ? std::string("shaders") : cfg.shaderPath)
    , window_(nullptr)
    , ptProgram_(0)
    , sqProgram_(0)
    , ptsVAO_(0)
    , ptsVBO_(0)
    , sqVAO_(0)
    , sqVBO_(0)
    , uViewPt_(0)
    , uViewSq_(0)
    , ptsCount_(0)
    , sqCount_(0)
    , zoom_(1.0f)
    , offsetX_(0.0f)
    , offsetY_(0.0f) {}

PointLabelVisualizer::~PointLabelVisualizer() {
    shutdown();
}

// -----------------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------------
bool PointLabelVisualizer::init() {
    try {
        initWindow();
        initGL();
        initImGui();
        loadShaders();
        buildPointBuffer();
        buildLabelBuffer(); // creates label VAO/VBO even if none are valid yet
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Initialization error: " << e.what() << std::endl;
        shutdown(); // Clean up any partially initialized resources
        return false;
    }
}

void PointLabelVisualizer::initWindow() {
    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");

    // Window + context
    window_ = glfwCreateWindow(1200, 800, "Point Labeler", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    // callbacks
    glfwSetWindowUserPointer(window_, this);
    glfwSetScrollCallback(window_, glfwScrollCallback);
}

void PointLabelVisualizer::initGL() {
    if (!gladLoadGL()) {
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLAD");
    }

    glEnable(GL_PROGRAM_POINT_SIZE); // allow gl_PointSize
}

void PointLabelVisualizer::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    imguiInited_ = true;
}

void PointLabelVisualizer::teardownImGui() {
    if (!imguiInited_) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    imguiInited_ = false;
}

// -----------------------------------------------------------------------------
// Shader helpers
// -----------------------------------------------------------------------------
std::string PointLabelVisualizer::loadFile(const std::string& path) const {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open file: " << path << "\n";
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

GLuint PointLabelVisualizer::compileShader(GLenum type, const std::string& src) const {
    GLuint shader = glCreateShader(type);
    const char* csrc = src.c_str();
    glShaderSource(shader, 1, &csrc, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error (" << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << "):\n" << log << "\n";
    }
    return shader;
}

GLuint PointLabelVisualizer::linkProgram(const std::string& vertPath,
                                         const std::string& fragPath) const {
    const std::string vsrc = loadFile(vertPath);
    const std::string fsrc = loadFile(fragPath);
    if (vsrc.empty() || fsrc.empty()) {
        throw std::runtime_error("Failed to load shaders from: " + vertPath + " or " + fragPath);
    }
    
    GLuint vs = compileShader(GL_VERTEX_SHADER,   vsrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsrc);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    // Check link status
    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "Program link error:\n" << log << "\n";
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

void PointLabelVisualizer::loadShaders() {
    ptProgram_ = linkProgram(shaderDir_ + "/point.vert", shaderDir_ + "/point.frag");
    sqProgram_ = linkProgram(shaderDir_ + "/label.vert", shaderDir_ + "/label.frag");
    uViewPt_   = glGetUniformLocation(ptProgram_, "u_view");
    uViewSq_   = glGetUniformLocation(sqProgram_, "u_view");
}

// -----------------------------------------------------------------------------
// GPU buffers
// -----------------------------------------------------------------------------
void PointLabelVisualizer::buildPointBuffer() {
    ptsCount_ = static_cast<int>(config_.points.size());

    if (!ptsVAO_) glGenVertexArrays(1, &ptsVAO_);
    if (!ptsVBO_) glGenBuffers(1, &ptsVBO_);

    glBindVertexArray(ptsVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, ptsVBO_);
    glBufferData(GL_ARRAY_BUFFER,
                 ptsCount_ * 2 * sizeof(float),
                 config_.points.empty() ? nullptr : config_.points.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// Pack a rectangle (4 segments) into a flat vertex list
static inline void appendRectLines(const Rect& r, std::vector<float>& verts) {
    const float x0 = r.xmin, y0 = r.ymin, x1 = r.xmax, y1 = r.ymax;
    verts.insert(verts.end(), {
        x0, y0,  x1, y0,
        x1, y0,  x1, y1,
        x1, y1,  x0, y1,
        x0, y1,  x0, y0
    });
}

void PointLabelVisualizer::buildLabelBuffer() {
    std::vector<float> verts;
    verts.reserve(config_.candidates.size() * 16); // 8 verts * 2 floats

    // If nothing is marked valid yet, do a one-shot greedy so users see labels.
    const bool anyValid = std::any_of(config_.candidates.begin(), config_.candidates.end(),
                                      [](const LabelCandidate& c){ return c.valid; });
    if (!anyValid && !config_.candidates.empty()) {
        greedyPlaceOneLabelPerPoint(config_.candidates, config_.points);
    }

    for (const auto& c : config_.candidates)
        if (c.valid) appendRectLines(getAABB(c), verts);

    if (!sqVAO_) glGenVertexArrays(1, &sqVAO_);
    if (!sqVBO_) glGenBuffers(1, &sqVBO_);

    glBindVertexArray(sqVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, sqVBO_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.empty() ? nullptr : verts.data(),
                 GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    sqCount_ = static_cast<int>(verts.size() / 2);
}

void PointLabelVisualizer::updateLabelBuffer() {
    if (!sqVBO_) { buildLabelBuffer(); return; }

    std::vector<float> verts;
    verts.reserve(config_.candidates.size() * 16);
    for (const auto& c : config_.candidates)
        if (c.valid) appendRectLines(getAABB(c), verts);

    glBindBuffer(GL_ARRAY_BUFFER, sqVBO_);
    const GLsizeiptr newSize = static_cast<GLsizeiptr>(verts.size() * sizeof(float));
    GLint oldSize = 0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &oldSize);

    if (newSize != oldSize) {
        glBufferData(GL_ARRAY_BUFFER, newSize,
                     verts.empty() ? nullptr : verts.data(),
                     GL_DYNAMIC_DRAW);
    } else if (newSize > 0) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, newSize, verts.data());
    }

    sqCount_ = static_cast<int>(verts.size() / 2);
}

// -----------------------------------------------------------------------------
// Rendering
// -----------------------------------------------------------------------------
void PointLabelVisualizer::renderFrame() {
    int w = 1, h = 1;
    glfwGetFramebufferSize(window_, &w, &h);
    const float aspect = (h > 0) ? (static_cast<float>(w) / h) : 1.0f;

    const glm::mat4 proj = glm::ortho(
        -aspect/zoom_ + offsetX_,  aspect/zoom_ + offsetX_,
        -1.f/zoom_   + offsetY_,   1.f/zoom_   + offsetY_,
        -1.f, 1.f
    );

    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // points
    glUseProgram(ptProgram_);
    glUniformMatrix4fv(uViewPt_, 1, GL_FALSE, &proj[0][0]);
    glBindVertexArray(ptsVAO_);
    glPointSize(3.f);
    glDrawArrays(GL_POINTS, 0, ptsCount_);

    // labels
    glUseProgram(sqProgram_);
    glUniformMatrix4fv(uViewSq_, 1, GL_FALSE, &proj[0][0]);
    glBindVertexArray(sqVAO_);
    glDrawArrays(GL_LINES, 0, sqCount_);
}

// -----------------------------------------------------------------------------
// Interaction
// -----------------------------------------------------------------------------
void PointLabelVisualizer::onScroll(double yoff) {
    int w, h;
    glfwGetFramebufferSize(window_, &w, &h);
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(window_, &mx, &my);

    // Cursor in NDC [-1, 1]
    const float ndcX = static_cast<float>(mx) / w * 2.f - 1.f;
    const float ndcY = 1.f - static_cast<float>(my) / h * 2.f;

    // World before
    const float orthoW0 = (static_cast<float>(w) / h) / zoom_;
    const float orthoH0 = 1.f / zoom_;
    const float wx0 = offsetX_ + ndcX * orthoW0;
    const float wy0 = offsetY_ + ndcY * orthoH0;

    // Zoom
    const float scale = (yoff > 0) ? 1.1f : 1.f / 1.1f;
    zoom_ = std::max(1e-4f, zoom_ * scale);

    // World after
    const float orthoW1 = (static_cast<float>(w) / h) / zoom_;
    const float orthoH1 = 1.f / zoom_;
    const float wx1 = offsetX_ + ndcX * orthoW1;
    const float wy1 = offsetY_ + ndcY * orthoH1;

    // Pan to keep cursor anchored
    offsetX_ += (wx0 - wx1);
    offsetY_ += (wy0 - wy1);
}

// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------
void PointLabelVisualizer::run() {
    if (!window_) {
        std::cerr << "Cannot run - window not initialized\n";
        return;
    }

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Controls");
        {
            ui_controls::Slider_Zoom(zoom_);

            static float baseSize = 0.02f;
            static float prevBase = baseSize;
            // Increased max from 1.0f to 5.0f and added an InputFloat for manual overshoot
            ImGui::SliderFloat("Base Size", &baseSize, 1e-4f, 5.0f, "%.5f", ImGuiSliderFlags_Logarithmic);
            ImGui::SameLine();
            if (ImGui::InputFloat("##BaseSizeManual", &baseSize, 0.0f, 0.0f, "%.5f")) {
                if (baseSize < 1e-4f) baseSize = 1e-4f;
            }
            if (baseSize != prevBase) {
                config_.candidates = generateLabelCandidates(config_.points, baseSize);
                for (auto& c : config_.candidates) c.valid = false;
                greedyPlaceOneLabelPerPoint(config_.candidates, config_.points);
                buildLabelBuffer();
                prevBase = baseSize;
            }

            if (ui_controls::Button_GeneratePoints(
                    config_.points, config_.candidates,
                    config_.points.empty() ? 200 : static_cast<int>(config_.points.size()),
                    baseSize))
            {
                buildPointBuffer();
                for (auto& c : config_.candidates) c.valid = false;
                std::vector<Rect> placed =
                    greedyPlaceOneLabelPerPoint(config_.candidates, config_.points);
                (void)placed;
                buildLabelBuffer();
            }

            std::vector<Rect> newPlaced;
            if (ui_controls::Button_RunGreedyStep(
                    config_.candidates, config_.points, newPlaced))
            {
                updateLabelBuffer();
            }
        }
        ImGui::End();

        // draw
        ImGui::Render();
        renderFrame();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }
}

// -----------------------------------------------------------------------------
// Shutdown
// -----------------------------------------------------------------------------
void PointLabelVisualizer::shutdown() {
    teardownImGui();

    if (ptsVBO_)  { glDeleteBuffers(1, &ptsVBO_);  ptsVBO_ = 0; }
    if (ptsVAO_)  { glDeleteVertexArrays(1, &ptsVAO_); ptsVAO_ = 0; }

    if (sqVBO_)   { glDeleteBuffers(1, &sqVBO_);   sqVBO_ = 0; }
    if (sqVAO_)   { glDeleteVertexArrays(1, &sqVAO_); sqVAO_ = 0; }

    if (ptProgram_) { glDeleteProgram(ptProgram_); ptProgram_ = 0; }
    if (sqProgram_) { glDeleteProgram(sqProgram_); sqProgram_ = 0; }

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }
}
