#include "camera.hpp"
#include <glm/gtc/matrix_transform.hpp>  // for glm::ortho

/// Default constructor sets zoom to 1.0 (no zoom) and offset to (0,0)
Camera2D::Camera2D()
    : zoom_(1.0f), offset_(0.0f, 0.0f) {}

/// Reset zoom and pan to default values
void Camera2D::reset() {
    zoom_ = 1.0f;
    offset_ = {0.0f, 0.0f};
}

/// Scroll callback to handle zooming centered on the mouse cursor
/// - @window: GLFW window to query size and cursor position
/// - @xoffset: horizontal scroll (ignored)
/// - @yoffset: vertical scroll (positive zooms in, negative zooms out)
void Camera2D::onScroll(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    if (yoffset == 0.0) return;  // No zoom change

    // Get framebuffer dimensions
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);

    // Get current cursor position in pixels
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    // Convert mouse pos to Normalized Device Coordinates [-1,1]
    float ndcX = float(mx)  / float(fbw) * 2.0f - 1.0f;
    float ndcY = 1.0f - float(my) / float(fbh) * 2.0f;

    // Aspect ratio of viewport
    float aspect = float(fbw) / float(fbh);

    // Compute world-space coords under cursor before zoom change
    float w_before = aspect / zoom_;
    float h_before = 1.0f  / zoom_;
    float worldXb = offset_.x + ndcX * w_before;
    float worldYb = offset_.y + ndcY * h_before;

    // Update zoom factor (10% per scroll step)
    zoom_ *= (yoffset > 0 ? 1.1f : (1.0f / 1.1f));

    // Compute world-space coords under cursor after zoom change
    float w_after = aspect / zoom_;
    float h_after = 1.0f  / zoom_;
    float worldXa = offset_.x + ndcX * w_after;
    float worldYa = offset_.y + ndcY * h_after;

    // Adjust offset so the same world point remains under the cursor
    offset_.x += (worldXb - worldXa);
    offset_.y += (worldYb - worldYa);
}

/// Compute the orthographic projection matrix based on current zoom and offset
/// - @fbWidth, @fbHeight: framebuffer dimensions
/// Returns a matrix mapping world-space (x,y) to clip space
glm::mat4 Camera2D::getProjectionMatrix(int fbWidth, int fbHeight) const {
    float aspect = float(fbWidth) / float(fbHeight);
    float w = aspect / zoom_;
    float h = 1.0f  / zoom_;
    
    // Create orthographic projection: left, right, bottom, top
    return glm::ortho(
        -w + offset_.x, w + offset_.x,
        -h + offset_.y, h + offset_.y,
        -1.0f, 1.0f    // near and far planes
    );
}
