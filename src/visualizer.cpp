#include "visualizer.hpp"
#include "ui_controls.hpp"

#include <imgui.h>                               // Dear ImGui core
#include <imgui_impl_glfw.h>                     // ImGui GLFW binding
#include <imgui_impl_opengl3.h>                  // ImGui OpenGL3 binding

#include <glm/glm.hpp>                           // GLM core types
#include <glm/gtc/matrix_transform.hpp>          // GLM matrix transformations

#include <fstream>
#include <sstream>
#include <iostream>

// GLFW scroll callback: forwards scroll events to our visualizer instance
static void glfwScrollCallback(GLFWwindow* win, double /*xoff*/, double yoff) {
    // Retrieve the visualizer pointer stored as user data
    auto* vis = static_cast<PointLabelVisualizer*>(glfwGetWindowUserPointer(win));
    if (vis) vis->onScroll(yoff);
}

// Construct with configuration (points, candidates) and shader directory
PointLabelVisualizer::PointLabelVisualizer(const VisualizerConfig& cfg)
    : config_(cfg), shaderDir_(cfg.shaderPath) {}

// Destructor: ensure graceful cleanup of resources
PointLabelVisualizer::~PointLabelVisualizer() {
    shutdown();
}

// Initialize all subsystems: window, OpenGL, ImGui, shaders, GPU buffers
bool PointLabelVisualizer::init() {
    initWindow();        // Create GLFW window and set callbacks
    initGL();            // Load GL functions and set state (point size)
    initImGui();         // Initialize ImGui context and bindings
    loadShaders();       // Compile/link shaders and query uniforms
    buildPointBuffer();  // Upload point vertex data
    buildLabelBuffer();  // Upload label rectangle data
    return true;
}

// Create GLFW window, set context, and attach scroll callback
void PointLabelVisualizer::initWindow() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    // Window size 1200x800 titled "Point Labeler"
    window_ = glfwCreateWindow(1200, 800, "Point Labeler", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    // Make this window's context current
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);  // Enable vsync

    // Store `this` pointer for use in callbacks
    glfwSetWindowUserPointer(window_, this);
    glfwSetScrollCallback(window_, glfwScrollCallback);
}

// Initialize OpenGL: load function pointers and enable point size control
void PointLabelVisualizer::initGL() {
    if (!gladLoadGL()) {
        throw std::runtime_error("Failed to initialize GLAD");
    }
    glEnable(GL_PROGRAM_POINT_SIZE);  // Allow shaders to set gl_PointSize
}

// Initialize Dear ImGui for GLFW + OpenGL3
void PointLabelVisualizer::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    ImGui::StyleColorsDark();       // Set dark theme
}

// Tear down ImGui resources
void PointLabelVisualizer::teardownImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

// Utility: read entire file into a string (used for loading shaders)
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

// Compile a GLSL shader of given type, logging compile errors
GLuint PointLabelVisualizer::compileShader(GLenum type, const std::string& src) const {
    GLuint s = glCreateShader(type);
    const char* ptr = src.c_str();
    glShaderSource(s, 1, &ptr, nullptr);
    glCompileShader(s);
    // Check compile status
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(s, 512, nullptr, buf);
        std::cerr << "Shader compile error: " << buf << "\n";
    }
    return s;
}

// Link vertex and fragment shaders into a program, then delete shader objects
GLuint PointLabelVisualizer::linkProgram(const std::string& vertPath,
                                         const std::string& fragPath) const {
    // Load and compile each stage
    GLuint vs = compileShader(GL_VERTEX_SHADER, loadFile(vertPath));
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, loadFile(fragPath));
    // Link into program
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    // Clean up shader objects
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// Compile and link point/label shaders, storing uniform locations
void PointLabelVisualizer::loadShaders() {
    ptProgram_ = linkProgram(shaderDir_ + "/point.vert", shaderDir_ + "/point.frag");
    sqProgram_ = linkProgram(shaderDir_ + "/label.vert", shaderDir_ + "/label.frag");
    uViewPt_   = glGetUniformLocation(ptProgram_, "u_view");
    uViewSq_   = glGetUniformLocation(sqProgram_, "u_view");
}

// Upload 2D point positions to GPU: create VAO, VBO, and set attribute
void PointLabelVisualizer::buildPointBuffer() {
    ptsCount_ = static_cast<int>(config_.points.size());
    glGenVertexArrays(1, &ptsVAO_);
    glGenBuffers(1, &ptsVBO_);
    glBindVertexArray(ptsVAO_);
      glBindBuffer(GL_ARRAY_BUFFER, ptsVBO_);
      glBufferData(GL_ARRAY_BUFFER,
                   ptsCount_ * 2 * sizeof(float),
                   config_.points.data(),
                   GL_STATIC_DRAW);
      // Attribute 0: vec2 position
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));
      glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// Compute label rectangles via greedy placement and upload as line segments
void PointLabelVisualizer::buildLabelBuffer() {
    // Place one label per point
    auto rects = greedyPlaceOneLabelPerPoint(config_.candidates);

    std::vector<float> verts;
    verts.reserve(rects.size() * 8 * 2);  // 4 segments x 2 verts each x 2 coords

    // For each rectangle, append line segment endpoints
    for (auto& R : rects) {
        float xmin = R.xmin, ymin = R.ymin;
        float xmax = R.xmax, ymax = R.ymax;
        // Bottom edge
        verts.insert(verts.end(), { xmin, ymin, xmax, ymin });
        // Right edge
        verts.insert(verts.end(), { xmax, ymin, xmax, ymax });
        // Top edge
        verts.insert(verts.end(), { xmax, ymax, xmin, ymax });
        // Left edge
        verts.insert(verts.end(), { xmin, ymax, xmin, ymin });
    }

    sqCount_ = static_cast<int>(verts.size() / 2);  // vertex count
    glGenVertexArrays(1, &sqVAO_);
    glGenBuffers(1, &sqVBO_);
    glBindVertexArray(sqVAO_);
      glBindBuffer(GL_ARRAY_BUFFER, sqVBO_);
      glBufferData(GL_ARRAY_BUFFER,
                   verts.size() * sizeof(float),
                   verts.data(),
                   GL_DYNAMIC_DRAW);
      // Attribute 0: vec2 position
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));
      glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// Update GPU label buffer with new placements (used after greedy steps)
void PointLabelVisualizer::updateLabelBuffer() {
    auto rects = greedyPlaceOneLabelPerPoint(config_.candidates);
    std::vector<float> verts;
    verts.reserve(rects.size() * 8 * 2);
    for (auto& R : rects) {
        float xmin = R.xmin, ymin = R.ymin;
        float xmax = R.xmax, ymax = R.ymax;
        verts.insert(verts.end(), { xmin, ymin, xmax, ymin });
        verts.insert(verts.end(), { xmax, ymin, xmax, ymax });
        verts.insert(verts.end(), { xmax, ymax, xmin, ymax });
        verts.insert(verts.end(), { xmin, ymax, xmin, ymin });
    }
    glBindBuffer(GL_ARRAY_BUFFER, sqVBO_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(float), verts.data());
    sqCount_ = static_cast<int>(verts.size() / 2);
}

// Render a single frame: clear, draw points, draw label lines
void PointLabelVisualizer::renderFrame() {
    int w, h;
    glfwGetFramebufferSize(window_, &w, &h);
    float aspect = static_cast<float>(w) / h;
    // Orthographic projection with zoom and pan
    glm::mat4 proj = glm::ortho(
        -aspect/zoom_ + offsetX_, +aspect/zoom_ + offsetX_,
        -1.0f/zoom_  + offsetY_, +1.0f/zoom_  + offsetY_,
        -1.0f, +1.0f
    );

    // Clear to white
    glClearColor(1,1,1,1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw points as black dots
    glUseProgram(ptProgram_);
    glUniformMatrix4fv(uViewPt_, 1, GL_FALSE, &proj[0][0]);
    glBindVertexArray(ptsVAO_);
    glPointSize(3.0f);
    glDrawArrays(GL_POINTS, 0, ptsCount_);

    // Draw label edges
    glUseProgram(sqProgram_);
    glUniformMatrix4fv(uViewSq_, 1, GL_FALSE, &proj[0][0]);
    glBindVertexArray(sqVAO_);
    glDrawArrays(GL_LINES, 0, sqCount_);
}

// Handle scroll input to zoom/pan, keeping cursor world-point fixed
void PointLabelVisualizer::onScroll(double yoffset) {
    int w, h;
    glfwGetFramebufferSize(window_, &w, &h);
    double mx, my;
    glfwGetCursorPos(window_, &mx, &my);
    // Convert to NDC
    float ndcX = float(mx) / w * 2.0f - 1.0f;
    float ndcY = 1.0f - float(my) / h * 2.0f;
    
    // World coords before zoom
    float w0 = (static_cast<float>(w)/h) / zoom_;
    float h0 = 1.0f / zoom_;
    float wx0 = offsetX_ + ndcX * w0;
    float wy0 = offsetY_ + ndcY * h0;
    
    // Apply zoom factor (10% per scroll)
    zoom_ *= (yoffset > 0 ? 1.1f : 1.0f/1.1f);
    
    // World coords after zoom
    float w1 = (static_cast<float>(w)/h) / zoom_;
    float h1 = 1.0f / zoom_;
    float wx1 = offsetX_ + ndcX * w1;
    float wy1 = offsetY_ + ndcY * h1;
    
    // Adjust pan to keep same world point under cursor
    offsetX_ += wx0 - wx1;
    offsetY_ += wy0 - wy1;
}

// Main loop: handle events, update UI, render frames, and swap buffers
void PointLabelVisualizer::run() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // Start new ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // UI controls panel
        ImGui::Begin("Controls");
          ui_controls::Slider_Zoom(zoom_);
          if (ui_controls::Button_GeneratePoints(
                  config_.points, config_.candidates,
                  static_cast<int>(config_.points.size()))) {
              buildPointBuffer();
             
         //   updateLabelBuffer();
          }
          if (ui_controls::Button_RunGreedyStep(config_.candidates)) {
             buildLabelBuffer();
             updateLabelBuffer();
          }
        ImGui::End();

        // Render ImGui over scene
        ImGui::Render();
        renderFrame();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }
}

// Cleanup: destroy ImGui, delete GL resources, destroy window
void PointLabelVisualizer::shutdown() {
    teardownImGui();

    if (ptsVBO_)  { glDeleteBuffers(1, &ptsVBO_);  glDeleteVertexArrays(1, &ptsVAO_); }
    if (sqVBO_)   { glDeleteBuffers(1, &sqVBO_);   glDeleteVertexArrays(1, &sqVAO_);  }
    if (ptProgram_) glDeleteProgram(ptProgram_);
    if (sqProgram_) glDeleteProgram(sqProgram_);
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }
}