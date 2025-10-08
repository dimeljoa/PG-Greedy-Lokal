#pragma once
#include <array>
#include <vector>

/**
 * @file greedy_labeler.hpp
 * @brief Public types and APIs for greedy point label placement with monotone state.
 *
 * Overview:
 *  - Each point can have up to 4 square label candidates (one per corner).
 *  - A greedy algorithm chooses non-overlapping labels that also avoid covering other points.
 *  - Monotone mode preserves previously placed labels when zooming in, only adds when zooming out.
 */

/**
 * @struct Rect
 * @brief Axis-aligned bounding box for a placed label (open interior semantics in placement).
 */
struct Rect {
    float xmin, ymin, xmax, ymax; ///< Minimum and maximum coordinates.
};

/**
 * @struct LabelCandidate
 * @brief Single candidate square label anchored at a point with a specific corner.
 *
 * corner encoding:
 *  - 0: Top-Left (TL)
 *  - 1: Top-Right (TR)
 *  - 2: Bottom-Right (BR)
 *  - 3: Bottom-Left (BL)
 *
 * valid is set true if the placement algorithm selects this candidate.
 */
struct LabelCandidate {
    std::array<float,2> anchor; ///< Anchor point (original point position).
    float  size;                ///< Side length of the square label.
    int    corner;              ///< Corner code (0..3) relative to anchor.
    [[maybe_unused]] float  weight; ///< Optional weighting (currently unused).
    bool   valid;               ///< True if chosen by the placement pass.
};

/**
 * @struct MonotoneState
 * @brief Persistent state to support monotone label placement across size/zoom changes.
 *
 * Fields:
 *  - lastBase: last label size used (detect zoom direction).
 *  - active: indices of candidates currently placed.
 *  - fixedCorner: per point chosen corner (stabilizes layout).
 *  - usedOnce: marks points that have ever received a label (optional policy).
 */
struct MonotoneState {
    float lastBase = -1.0f;                 ///< Previous base label size (<0 means uninitialized).
    std::vector<int> active;                ///< Candidate indices active after last placement.
    std::vector<int> fixedCorner;           ///< Chosen corner (0..3) per point.
    std::vector<unsigned char> usedOnce;    ///< 1 if point labeled at least once.
};

/**
 * @brief Compute the axis-aligned bounding box of a label candidate.
 * @param c Candidate.
 * @return Rect covering the square label for this candidate.
 */
Rect getAABB(const LabelCandidate& c);

/**
 * @brief Generate 4 square label candidates (one per corner) for each point.
 * @param pts Input 2D points.
 * @param baseSize Desired side length of each label.
 * @return Flat vector of candidates (size = 4 * pts.size()).
 */
std::vector<LabelCandidate>
generateLabelCandidates(const std::vector<std::array<float,2>>& pts, float baseSize);

/**
 * @brief Convenience helper: place exactly one label per point using greedy strategy.
 *
 * Uses an internal static MonotoneState (not thread-safe). For external control or
 * multi-frame interaction, prefer greedyPlaceMonotone with your own state instance.
 *
 * @param candidates Candidate list (modified: valid flags set, size may be updated).
 * @param points Matching point set.
 * @return Vector of placed Rects in the order they were accepted.
 */
std::vector<Rect>
greedyPlaceOneLabelPerPoint(std::vector<LabelCandidate>& candidates,
                            const std::vector<std::array<float,2>>& points);

/**
 * @brief Monotone greedy label placement preserving stability across zoom changes.
 *
 * Behavior:
 *  - If baseSize increases (zoom in): retain subset of previously valid labels that remain feasible.
 *  - If baseSize decreases (zoom out): attempt to add more labels without removing existing ones.
 *
 * @param candidates  Candidate list (corner & size updated; valid flags written).
 * @param points      Input points (same order as used for candidate generation).
 * @param baseSize    Current label size.
 * @param state       Persistent state pointer (must outlive repeated calls).
 * @return Vector of placed Rects for this invocation.
 */
std::vector<Rect>
greedyPlaceMonotone(std::vector<LabelCandidate>& candidates,
                    const std::vector<std::array<float,2>>& points,
                    float baseSize,
                    MonotoneState* state); // Use a pointer to the state
