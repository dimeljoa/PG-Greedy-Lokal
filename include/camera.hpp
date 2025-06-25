#pragma once

// Disable GLFW's OpenGL includes; we'll load our own with glad
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>


/// Camera2D handles zoom and pan (offset) for a 2D orthographic view.
/// It supports zooming centered at the mouse cursor via a scroll callback.
class Camera2D {
public:
    /// Default constructor: initializes zoom to 1.0 (no zoom) and offset to (0,0) (no pan)
    Camera2D();

    /// GLFW scroll callback: attach this to glfwSetScrollCallback.
    /// - @window: the GLFW window receiving the scroll event.
    /// - @xoffset: horizontal scroll (ignored).
    /// - @yoffset: vertical scroll (use positive values to zoom in, negative to zoom out).
    /// This updates both the zoom factor and offset so that zooming is centered at the cursor.
    void onScroll(GLFWwindow* window, double xoffset, double yoffset);

    /// Computes and returns the 4x4 orthographic projection matrix based on current zoom and pan.
    /// - @fbWidth, @fbHeight: dimensions of the framebuffer in pixels.
    /// The resulting matrix maps world coordinates to normalized device coordinates.
    glm::mat4 getProjectionMatrix(int fbWidth, int fbHeight) const;

    /// Reset zoom and pan to their default values (zoom=1, offset=(0,0)).
    void reset();

    /// Get current zoom factor (1.0 = no zoom; >1 = zoom in; <1 = zoom out)
    float getZoom() const { return zoom_; }

    /// Get current world-space offset (pan) vector.
    const glm::vec2& getOffset() const { return offset_; }

private:
    float     zoom_;    // Zoom factor: scales world units to screen units
    glm::vec2 offset_;  // World-space offset: translation applied to the view
};
