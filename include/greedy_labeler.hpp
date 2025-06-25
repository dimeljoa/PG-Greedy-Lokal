#pragma once

#include <array>
#include <vector>

/// Simple axis-aligned bounding box represented by minimum and maximum extents.
/// Used for collision detection between label candidates.
struct Rect {
    float xmin; ///< Minimum X coordinate
    float ymin; ///< Minimum Y coordinate
    float xmax; ///< Maximum X coordinate
    float ymax; ///< Maximum Y coordinate
};

/// Represents one potential label placement (a square) anchored at a point.
/// Each anchor point generates four candidates (one per corner), with a size,
/// priority for selection order, and a flag indicating if placed.
struct LabelCandidate {
    std::array<float,2> anchor; ///< (x,y) coordinate of the attached data point
    float               size;   ///< Side length of the square label
    int                 corner; ///< Corner index fixed at anchor: 0=top-left, 1=top-right,
                                 ///< 2=bottom-right, 3=bottom-left
    float               priority; ///< Higher values are placed first in greedy algorithm
    bool                valid;    ///< Set to true when this candidate is selected for placement
};

/// GenerateLabelCandidates:
/// For each input point, creates four LabelCandidate entries—one for each
/// corner position. This provides alternative label positions to avoid
/// overlapping other labels.
/// @param points Vector of (x,y) data points
/// @returns Vector of 4*N LabelCandidate objects for N input points
std::vector<LabelCandidate>
generateLabelCandidates(const std::vector<std::array<float,2>>& points);

/// getAABB:
/// Computes the axis-aligned bounding box for a given candidate.
/// Uses the anchor, size, and corner index to determine the rectangle extents.
/// @param cand LabelCandidate to evaluate
/// @returns Rect representing [xmin, ymin, xmax, ymax] of the candidate
Rect getAABB(const LabelCandidate& cand);

/// isCollision:
/// Inline helper that tests for overlap between two Rects using AABB separation.
/// Returns true if rectangles overlap, false otherwise.
inline bool isCollision(const Rect& a, const Rect& b) {
    return !(a.xmax <= b.xmin || b.xmax <= a.xmin
          || a.ymax <= b.ymin || b.ymax <= a.ymin);
}

/// greedyPlaceOneLabelPerPoint:
/// Performs a simple greedy selection to choose at most one label per anchor.
/// Iterates through candidates in groups of four (one point), sorting each block
/// by descending priority, and picks the first valid candidate that does not
/// collide with already placed labels. Marks cand.valid=true for placed ones.
/// @param candidates Vector of candidates (modified in-place)
/// @returns Vector of placed label Rects
std::vector<Rect>
greedyPlaceOneLabelPerPoint(std::vector<LabelCandidate>& candidates);

/// placeLabelsRecursive:
/// Advanced recursive algorithm using quadtree partitioning:
/// 1. Runs a greedy pass within the specified region [minX, minY]–[maxX, maxY].
/// 2. Collects any candidates not placed in this region.
/// 3. Subdivides the region into four quadrants and recurses until maxDepth.
/// This reduces comparisons by spatial locality, improving performance on large sets.
/// @param candidates All label candidates (valid flags updated)
/// @param placed     Output vector of placed Rects appended in order
/// @param minX, minY Lower bounds of current region
/// @param maxX, maxY Upper bounds of current region
/// @param maxDepth   Recursion depth limit (default 8)
void placeLabelsRecursive(
    std::vector<LabelCandidate>& candidates,
    std::vector<Rect>&           placed,
    float                        minX,
    float                        minY,
    float                        maxX,
    float                        maxY,
    int                          maxDepth = 8
);
