#version 330 core

// Vertex shader for transforming 2D positions and passing vertex color

// Vertex attributes:
// location 0: 2D position (x, y)
layout(location = 0) in vec2 aPos;
// location 1: RGB color specified per-vertex
layout(location = 1) in vec3 aColor;

// Uniform: view/projection matrix combining orthographic projection and camera transform
uniform mat4 u_view;

// Output to fragment shader: interpolated color
out vec3 vColor;

void main() {
    // Pass through the vertex color to the fragment shader
    vColor = aColor;

    // Transform the 2D position into clip space using the view matrix
    // Expand to vec4 with z=0.0 and w=1.0
    gl_Position = u_view * vec4(aPos, 0.0, 1.0);
}