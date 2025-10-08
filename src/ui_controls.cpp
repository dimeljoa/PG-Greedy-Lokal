// src/ui_controls.cpp

#include "ui_controls.hpp"
#include "greedy_labeler.hpp"

#include <imgui.h>
#include <random>
#include <vector>
#include <array>
#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace ui_controls {

// -----------------------------------------------------------------------------
// Zoom Slider
// -----------------------------------------------------------------------------
bool Slider_Zoom(float& zoom) {
    ImGui::Text("Zoom: %.2fx", zoom);
    return ImGui::SliderFloat("Zoom##ui", &zoom, 0.1f, 10.0f, "%.2fx");
}

// -----------------------------------------------------------------------------
// Generate Random Points + Candidates
// -----------------------------------------------------------------------------
bool Button_GeneratePoints(std::vector<std::array<float,2>>& points,
                           std::vector<LabelCandidate>&     candidates,
                           int                              count,
                           float                            baseSize) {
    if (!ImGui::Button("Generate Points"))
        return false;

    // Sample random points in [-1,1]^2
    std::mt19937_64 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    points.clear();
    points.reserve(count);
    for (int i = 0; i < count; ++i) {
        points.push_back({ dist(rng), dist(rng) });
    }

    // Build candidates with user-specified base size
    candidates = generateLabelCandidates(points, baseSize);

    return true;
}

// -----------------------------------------------------------------------------
// Run Single Greedy Step (no manual valid reset needed)
// -----------------------------------------------------------------------------
bool Button_RunGreedyStep(std::vector<LabelCandidate>&                candidates,
                          const std::vector<std::array<float,2>>&     points,
                          std::vector<Rect>&                          placedOut) {
    if (!ImGui::Button("Run Greedy Step"))
        return false;
    placedOut = greedyPlaceOneLabelPerPoint(candidates, points);
    return !placedOut.empty();
}

// -----------------------------------------------------------------------------
// Rebuild candidates (same points) with new base size
// -----------------------------------------------------------------------------
bool Button_RebuildCandidates(std::vector<std::array<float,2>>& points,
                              std::vector<LabelCandidate>&     candidates,
                              float baseSize) {
    if (!ImGui::Button("Rebuild Candidates (size only)")) return false;
    candidates = generateLabelCandidates(points, baseSize);
    return true;
}

// -----------------------------------------------------------------------------
// Auto-update labels when base size changed
// -----------------------------------------------------------------------------
bool Slider_BaseSize_Auto(float& baseSize,
                          std::vector<std::array<float,2>>& points,
                          std::vector<LabelCandidate>& candidates,
                          std::vector<Rect>& placedOut) {
    static float prev = baseSize;
    bool changed = ImGui::SliderFloat("Base size", &baseSize, 0.005f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
    if (changed && std::fabs(baseSize - prev) > 1e-6f) {
        // regenerate candidates with new size
        candidates = generateLabelCandidates(points, baseSize);
        placedOut = greedyPlaceOneLabelPerPoint(candidates, points);
        prev = baseSize;
        return true;
    }
    return false;
}

// Keep original slider (if still needed)
bool Slider_BaseSize(float& baseSize) {
    ImGui::Text("Base Size: %.4f", baseSize);
    return ImGui::SliderFloat("Base size (manual)", &baseSize, 0.005f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
}

} // namespace ui_controls
