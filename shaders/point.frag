#version 330 core

// Fragment shader for rendering points in a solid color

// Output: final fragment color
out vec4 FragColor;

void main() {
    // Always output black with full opacity for points
    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}