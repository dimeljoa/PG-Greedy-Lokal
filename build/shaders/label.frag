#version 330 core

// Fragment shader for simple colored rendering

// Input from the vertex shader: interpolated vertex color (RGB)
in vec3 vColor;

// Output: final fragment color including alpha
out vec4 FragColor;

void main() {
    // Set the fragment color using the interpolated vertex color with full opacity
    FragColor = vec4(vColor, 1.0);
}