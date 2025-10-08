#include "camera.hpp"
#include <glm/gtc/matrix_transform.hpp> // glm::ortho

/**
 * @file camera.cpp
 * @brief Implementation of Camera2D: orthographic 2D zoom + pan with cursor-centered zooming.
 */

/**
 * @brief Default constructor (zoom = 1, offset = (0,0)).
 */
Camera2D::Camera2D() : zoom_(1.0f), offset_(0.0f, 0.0f) {}

/**
 * @brief Reset camera to identity zoom and zero offset.
 */
void Camera2D::reset() {
    zoom_   = 1.0f;
    offset_ = {0.0f, 0.0f};
}

void Camera2D::onScroll(GLFWwindow* window, double, double yoffset) {
    if (yoffset == 0.0) return;

    // Framebuffer size + cursor position
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    // Cursor → NDC [-1,1]
    float ndcX = float(mx) / fbw * 2.0f - 1.0f;
    float ndcY = 1.0f - float(my) / fbh * 2.0f;

    float aspect = float(fbw) / fbh;

    // World coords before zoom
    float w_before = aspect / zoom_;
    float h_before = 1.0f / zoom_;
    float wx_before = offset_.x + ndcX * w_before;
    float wy_before = offset_.y + ndcY * h_before;

    // Apply zoom (10% per scroll step)
    zoom_ *= (yoffset > 0 ? 1.1f : 1.0f / 1.1f);

    // World coords after zoom
    float w_after = aspect / zoom_;
    float h_after = 1.0f / zoom_;
    float wx_after = offset_.x + ndcX * w_after;
    float wy_after = offset_.y + ndcY * h_after;

    // Adjust offset so cursor stays pinned to same world point
    offset_.x += (wx_before - wx_after);
    offset_.y += (wy_before - wy_after);
}

glm::mat4 Camera2D::getProjectionMatrix(int fbWidth, int fbHeight) const {
    float aspect = float(fbWidth) / fbHeight;
    float w = aspect / zoom_;
    float h = 1.0f / zoom_;

    return glm::ortho(-w + offset_.x, w + offset_.x,
                      -h + offset_.y, h + offset_.y,
                      -1.0f, 1.0f);
}
