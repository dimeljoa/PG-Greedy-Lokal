#version 330 core

// Vertex shader for rendering 2D points without per-vertex color

// Vertex attribute: 2D position at location 0 (x, y)
layout(location = 0) in vec2 aPos;

// Uniform: view/projection matrix combining orthographic projection and camera transform
uniform mat4 u_view;

void main() {
    // Transform the 2D position into clip space
    // z is set to 0.0 and w to 1.0 for homogeneous coordinates
    gl_Position = u_view * vec4(aPos, 0.0, 1.0);
}