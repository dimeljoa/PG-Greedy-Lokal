// src/ui_controls.cpp

#include "ui_controls.hpp"
#include "greedy_labeler.hpp"  // generateLabelCandidates(), greedyPlaceOneLabelPerPoint()

#include <imgui.h>
#include <algorithm>    // <-- for std::remove_if
#include <random>
#include <vector> 

namespace ui_controls {

// Zoom slider stays the same
bool Slider_Zoom(float& zoomLevel) {
    ImGui::Text("Zoom: %.2fx", zoomLevel);
    return ImGui::SliderFloat("Zoom##ui", &zoomLevel, 0.1f, 10.0f, "%.2fx");
}

// Remove any candidates that weren’t successfully placed
bool Button_RemoveInvalid(std::vector<LabelCandidate>& candidates) {
    if (!ImGui::Button("Remove Red Squares"))
        return false;

    // now remove_if is visible
    candidates.erase(
        std::remove_if(candidates.begin(),
                       candidates.end(),
                       [](auto const &c){ return !c.valid; }),
        candidates.end()
    );
    return true;
}

// Generate a fresh set of random points + rebuild their 4-corner candidates
bool Button_GeneratePoints(
    std::vector<std::array<float,2>>& points,
    std::vector<LabelCandidate>&      candidates,
    int                                count
) {
    if (!ImGui::Button("Generate Points"))
        return false;

    // Sample new random points
    std::mt19937_64 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    points.clear();
    points.reserve(count);
    for (int i = 0; i < count; ++i)
        points.push_back({ dist(rng), dist(rng) });

    // Build new candidates
    candidates = generateLabelCandidates(points);
    return true;
}

// Run *one* greedy pass over all candidates (marks exactly one label per point)
bool Button_RunGreedyStep(std::vector<LabelCandidate>& candidates) {
    if (!ImGui::Button("Run Greedy Step"))
        return false;

    // Reset validity
    for (auto &c : candidates)
        c.valid = false;

    // Invoke library greedy placement (it sets c.valid appropriately)
    greedyPlaceOneLabelPerPoint(candidates);
    return true;
}

} // namespace ui_controls
