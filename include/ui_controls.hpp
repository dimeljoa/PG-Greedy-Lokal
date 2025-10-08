#pragma once

/**
 * @file ui_controls.hpp
 * @brief ImGui-based UI helper controls for interacting with point label demo.
 *
 * Provides small, self-contained wrapper functions that:
 *  - Expose a zoom slider tied to a float value.
 *  - Generate random points and corresponding label candidates on demand.
 *  - Execute a single greedy placement step and report newly placed labels.
 *
 * These helpers isolate UI logic from the core labeling algorithm so that
 * rendering / algorithm modules remain decoupled from immediate-mode GUI code.
 */

// Prevent GLFW from including legacy GL headers
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <array>

#include "greedy_labeler.hpp"

namespace ui_controls {

/**
 * @brief Draw a zoom slider (e.g., with ImGui) and update zoom value.
 *
 * Expected usage inside an ImGui frame. Implementation should clamp or
 * constrain zoom as desired (e.g., positive range).
 *
 * @param zoom In/out parameter: current zoom factor, modified if user changes slider.
 * @return true if the zoom value changed this frame.
 */
bool Slider_Zoom(float& zoom);

/**
 * @brief UI button: regenerate a fresh random point set and its label candidates.
 *
 * Replaces existing content of points and candidates vectors.
 * Generates 'count' random points (uniform) and creates 4 candidates per point
 * via generateLabelCandidates().
 *
 * @param points     Output: vector of (x,y) point positions.
 * @param candidates Output: vector of generated label candidates (size = 4 * count).
 * @param count      Number of points to generate (default 200).
 * @param baseSize   Label base side length assigned to each candidate.
 * @return true if generation occurred (button pressed).
 */
bool Button_GeneratePoints(std::vector<std::array<float,2>>& points,
                           std::vector<LabelCandidate>& candidates,
                           int count = 200,
                           float baseSize = 0.02f);

/**
 * @brief UI button: run a single greedy placement step over current candidates.
 *
 * Calls a greedy placement routine (e.g., greedyPlaceOneLabelPerPoint or monotone variant),
 * updating candidate.valid flags and filling 'placed' with resulting Rects.
 *
 * @param candidates Candidate labels (valid flags are updated).
 * @param points     Anchor point set used for collision / containment checks.
 * @param placed     Output vector of placed rectangles (overwritten or appended depending on impl).
 * @return true if at least one label was placed in this invocation.
 */
bool Button_RunGreedyStep(std::vector<LabelCandidate>& candidates,
                          const std::vector<std::array<float,2>>& points,
                          std::vector<Rect>& placed);

} // namespace ui_controls
