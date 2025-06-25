#include "greedy_labeler.hpp"
#include <algorithm>
#include <limits>
#include <cmath>

// Compute axis-aligned bounding box for a label candidate
Rect getAABB(const LabelCandidate& c) {
    float x = c.anchor[0];
    float y = c.anchor[1];
    float s = c.size;
    // Determine lower-left corner based on which corner is anchored
    float xmin = (c.corner == 1 || c.corner == 2) ? x : x - s;
    float ymin = (c.corner >= 2)               ? y : y - s;
    // Return rectangle spanning [xmin, ymin] to [xmin+s, ymin+s]
    return { xmin, ymin, xmin + s, ymin + s };
}

// Generate four candidate labels per point with sizes based on nearest-neighbor clearance
std::vector<LabelCandidate>
generateLabelCandidates(const std::vector<std::array<float,2>>& pts) {
    size_t N = pts.size();
    std::vector<LabelCandidate> out;
    out.reserve(N * 4);

    // 1) Compute L-infinity distance to nearest neighbor for each point
    std::vector<float> clearance(N, std::numeric_limits<float>::infinity());
    for (size_t i = 0; i < N; ++i) {
        const auto &p = pts[i];
        for (size_t j = 0; j < N; ++j) {
            if (i == j) continue;
            const auto &q = pts[j];
            float dx = std::abs(q[0] - p[0]);
            float dy = std::abs(q[1] - p[1]);
            // L-infinity metric: max(dx, dy)
            clearance[i] = std::min(clearance[i], std::max(dx, dy));
        }
    }

    // 2) Create four LabelCandidates per point using 90% of clearance as size
    for (size_t i = 0; i < N; ++i) {
        float rawSize = clearance[i] * 0.9f;
        // Clamp size to reasonable bounds [eps, 0.75]
        float sz = std::clamp(rawSize, 1e-6f, 0.75f);
        for (int corner = 0; corner < 4; ++corner) {
            // priority set to size, valid flag initialized false
            out.push_back({ pts[i], sz, corner, sz, false });
        }
    }

    // 3) Sort candidates so that for each anchor, larger labels come first
    std::sort(out.begin(), out.end(),
        [](auto const &a, auto const &b) {
            if (a.anchor != b.anchor) return a.anchor < b.anchor;
            return a.size > b.size;
        });

    return out;
}

// Greedy placement: one label per point, in descending priority order
std::vector<Rect>
greedyPlaceOneLabelPerPoint(std::vector<LabelCandidate>& cands) {
    std::vector<Rect> placed;
    placed.reserve(cands.size() / 4);

    // Iterate through blocks of four candidates (same anchor)
    for (size_t i = 0; i + 3 < cands.size(); i += 4) {
        for (int k = 0; k < 4; ++k) {
            auto &cand = cands[i + k];
            Rect r = getAABB(cand);
            bool ok = true;
            // Check collision against all already placed labels
            for (auto const &other : placed) {
                if (isCollision(r, other)) { ok = false; break; }
            }
            if (ok) {
                cand.valid = true;       // Mark as chosen
                placed.push_back(r);     // Keep its AABB for future checks
                break;                   // Stop after one label per point
            }
        }
    }
    return placed;
}

// Recursive quadtree-based label placement to reduce collision checks
void placeLabelsRecursive(
    std::vector<LabelCandidate>& candidates,
    std::vector<Rect>&           placed,
    float                        minX,
    float                        minY,
    float                        maxX,
    float                        maxY,
    int                          depth
) {
    if (depth <= 0) return;  // Stop recursion at depth 0

    // 1) Greedy pass within the current spatial tile
    for (auto &cand : candidates) {
        if (cand.valid) continue;
        auto r = getAABB(cand);
        // Skip candidates outside the current tile
        if (r.xmin < minX || r.xmax > maxX || r.ymin < minY || r.ymax > maxY)
            continue;
        bool ok = true;
        for (auto const &other : placed) {
            if (isCollision(r, other)) { ok = false; break; }
        }
        if (ok) {
            cand.valid = true;
            placed.push_back(r);
        }
    }

    // 2) Collect candidates within the tile that remain unplaced
    std::vector<LabelCandidate*> fails;
    for (auto &cand : candidates) {
        if (cand.valid) continue;
        auto r = getAABB(cand);
        if (r.xmin >= minX && r.xmax <= maxX && r.ymin >= minY && r.ymax <= maxY)
            fails.push_back(&cand);
    }
    if (fails.empty()) return;

    // 3) Subdivide the tile into four quadrants and recurse
    float midX = 0.5f * (minX + maxX);
    float midY = 0.5f * (minY + maxY);

    // Lower-left quadrant
    placeLabelsRecursive(candidates, placed, minX,  minY,  midX, midY, depth-1);
    // Lower-right quadrant
    placeLabelsRecursive(candidates, placed, midX,  minY,  maxX, midY, depth-1);
    // Upper-left quadrant
    placeLabelsRecursive(candidates, placed, minX,  midY,  midX, maxY, depth-1);
    // Upper-right quadrant
    placeLabelsRecursive(candidates, placed, midX,  midY,  maxX, maxY, depth-1);
}
