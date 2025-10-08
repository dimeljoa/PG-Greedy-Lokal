// src/greedy_labeler.cpp
#include "greedy_labeler.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <vector>
#include <climits>
#include <memory>  // ADD THIS

// -------------------- spatial hashing (move to top) --------------------
struct CellKey { int x, y; };
struct CellHash {
    size_t operator()(const CellKey& k) const noexcept {
        // simple mix
        return (size_t)k.x * 1315423911u ^ (size_t)k.y * 2654435761u;
    }
};
struct CellEq { bool operator()(const CellKey& a, const CellKey& b) const noexcept {
    return a.x == b.x && a.y == b.y;
}};
static inline int cellOf(float v, float cs) { return (int)std::floor(v / cs); }

// -------------------- helpers --------------------
static inline bool rectContainsPoint(const Rect& r, float x, float y) {
    // open interior (edges allowed)
    return x > r.xmin && x < r.xmax && y > r.ymin && y < r.ymax;
}
static inline bool overlapsStrict(const Rect& a, const Rect& b) {
    // edges may touch
    return (a.xmin < b.xmax && a.xmax > b.xmin &&
            a.ymin < b.ymax && a.ymax > b.ymin);
}
static inline float rectGap(const Rect& a, const Rect& b) {
    const float dx = (a.xmax < b.xmin) ? (b.xmin - a.xmax)
                   : (b.xmax < a.xmin) ? (a.xmin - b.xmax) : 0.0f;
    const float dy = (a.ymax < b.ymin) ? (b.ymin - a.ymax)
                   : (b.ymax < a.ymin) ? (a.ymin - b.ymax) : 0.0f;
    return std::sqrt(dx*dx + dy*dy); // 0 => edge-touch
}

// AABB from candidate
Rect getAABB(const LabelCandidate& c) {
    const float x = c.anchor[0];
    const float y = c.anchor[1];
    const float s = c.size;
    const float xmin = (c.corner == 1 || c.corner == 2) ? x : x - s;
    const float ymin = (c.corner >= 2)                  ? y : y - s;
    return { xmin, ymin, xmin + s, ymin + s };
}

// helper: candidate index -> owning point index
static inline int ownerOf(int candIndex, int perPoint) {
    return perPoint ? (candIndex / perPoint) : 0;
}

// Generate 4 candidates per point (all corners), choose best fixed corner globally
std::vector<LabelCandidate>
generateLabelCandidates(const std::vector<std::array<float,2>>& pts, float baseSize) {
    std::vector<LabelCandidate> out;
    if (pts.empty()) return out;

    const int N = (int)pts.size();
    out.reserve(N * 4); // 4 candidates per point

    // Generate all 4 corners for each point
    for (int pid = 0; pid < N; ++pid) {
        const auto& p = pts[pid];
        for (int corner = 0; corner < 4; ++corner) {
            out.push_back({{p[0], p[1]}, baseSize, corner, 1.0f, false});
        }
    }

    return out;
}

// (Removed legacy KD-tree code: replaced by grid-based orthant clearance.)

// Grid for points (fast "any point strictly inside rect?")
struct PointGrid {
    float cs;
    const std::vector<std::array<float,2>>& pts;
    std::unordered_map<CellKey, std::vector<int>, CellHash, CellEq> grid;

    // NEW: bounds of occupied cells
    int minCx = INT_MAX, maxCx = INT_MIN;
    int minCy = INT_MAX, maxCy = INT_MIN;

    PointGrid(const std::vector<std::array<float,2>>& p, float cellSize)
        : cs(cellSize), pts(p) {
        grid.reserve(p.size() * 2);
        for (int i = 0; i < (int)p.size(); ++i) {
            const int cx = cellOf(p[i][0], cs), cy = cellOf(p[i][1], cs);
            grid[{cx, cy}].push_back(i);
            if (cx < minCx) minCx = cx; if (cx > maxCx) maxCx = cx;
            if (cy < minCy) minCy = cy; if (cy > maxCy) maxCy = cy;
        }
    }

    bool withinBounds(int cx, int cy) const {
        return cx >= minCx && cx <= maxCx && cy >= minCy && cy <= maxCy;
    }

    bool anyInside(const Rect& r, int ignoreIdx) const {
        const int x0 = cellOf(r.xmin, cs), x1 = cellOf(r.xmax, cs);
        const int y0 = cellOf(r.ymin, cs), y1 = cellOf(r.ymax, cs);
        for (int cy = y0; cy <= y1; ++cy)
            for (int cx = x0; cx <= x1; ++cx) {
                auto it = grid.find({cx, cy});
                if (it == grid.end()) continue;
                for (int idx : it->second) {
                    if (idx == ignoreIdx) continue;
                    const auto& p = pts[idx];
                    if (rectContainsPoint(r, p[0], p[1])) return true;
                }
            }
        return false;
    }

    // local density (3x3 neighborhood) for sorting hardness
    int localCount(float x, float y) const {
        const int cx = cellOf(x, cs), cy = cellOf(y, cs);
        int cnt = 0;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                auto it = grid.find({cx + dx, cy + dy});
                if (it != grid.end()) cnt += (int)it->second.size();
            }
        return cnt;
    }
};

// NEW: grid-based orthant clearance (Chebyshev) used to choose corners.
// Scans cells in increasing rings, only within the requested orthant.
static float orthantClearanceGrid(const PointGrid& pg,
                                  int i, float xi, float yi,
                                  int sx, int sy, float eps) {
    const int cx = cellOf(xi, pg.cs), cy = cellOf(yi, pg.cs);
    float best = std::numeric_limits<float>::infinity();

    // Max rings to cover occupied area
    const int maxR = 2 + std::max(pg.maxCx - pg.minCx, pg.maxCy - pg.minCy);

    auto step = [](int s){ return s > 0 ? 1 : -1; };
    const int stepx = step(sx), stepy = step(sy);

    for (int r = 1; r <= maxR; ++r) {
        bool touched = false;

        // Edge where |a| == r (x-edge of the ring in this orthant)
        const int ax = (sx > 0) ? (cx + r) : (cx - r);
        const int by0 = (sy > 0) ? (cy + 1) : (cy - 1);
        const int by1 = (sy > 0) ? (cy + r) : (cy - r);
        for (int by = by0; (sy > 0) ? (by <= by1) : (by >= by1); by += stepy) {
            if (!pg.withinBounds(ax, by)) continue;
            touched = true;
            auto it = pg.grid.find({ax, by});
            if (it == pg.grid.end()) continue;
            for (int j : it->second) if (j != i) {
                float dx = pg.pts[j][0] - xi, dy = pg.pts[j][1] - yi;
                if (dx * sx > eps && dy * sy > eps) {
                    float cand = std::min(std::fabs(dx), std::fabs(dy));
                    if (cand < best) best = cand;
                }
            }
        }

        // Edge where |b| == r (y-edge of the ring in this orthant)
        const int by = (sy > 0) ? (cy + r) : (cy - r);
        const int ax0 = (sx > 0) ? (cx + 1) : (cx - 1);
        const int ax1 = (sx > 0) ? (cx + r) : (cx - r);
        for (int ax2 = ax0; (sx > 0) ? (ax2 <= ax1) : (ax2 >= ax1); ax2 += stepx) {
            if (!pg.withinBounds(ax2, by)) continue;
            touched = true;
            auto it = pg.grid.find({ax2, by});
            if (it == pg.grid.end()) continue;
            for (int j : it->second) if (j != i) {
                float dx = pg.pts[j][0] - xi, dy = pg.pts[j][1] - yi;
                if (dx * sx > eps && dy * sy > eps) {
                    float cand = std::min(std::fabs(dx), std::fabs(dy));
                    if (cand < best) best = cand;
                }
            }
        }

        // Stop if further rings cannot improve best
        if (std::isfinite(best) && (r * pg.cs) >= (best - eps)) break;

        // If this ring hit nothing and we already stepped beyond bounds in both axes, bail
        if (!touched) {
            const bool pastX = (sx > 0) ? (cx + r > pg.maxCx) : (cx - r < pg.minCx);
            const bool pastY = (sy > 0) ? (cy + r > pg.maxCy) : (cy - r < pg.minCy);
            if (pastX && pastY) break;
        }
    }
    return best;
}

// Replace chooseFixedCornersByConflicts to use orthantClearanceGrid.
// This restores outward-facing behavior with grid-based complexity.
static std::vector<int> chooseFixedCornersByConflicts(
    const std::vector<std::array<float,2>>& points,
    float /*baseSize*/) {

    const int N = (int)points.size();
    std::vector<int> fixedCorner(N, 1); // TR default
    if (N == 0) return fixedCorner;

    // Use point grid + orthant clearance (Chebyshev) without KD-tree dependence.
    PointGrid pg(points, /*cellSize*/ 0.05f); // cellSize heuristic; original baseSize may be unavailable here.
    const float eps = 1e-6f;

    auto clearance = [&](int i, int sx, int sy){
        return orthantClearanceGrid(pg, i, points[i][0], points[i][1], sx, sy, eps);
    };

    for (int i = 0; i < N; ++i) {
        float cTL = clearance(i, -1, -1);
        float cTR = clearance(i, +1, -1);
        float cBR = clearance(i, +1, +1);
        float cBL = clearance(i, -1, +1);
        float clear[4] = {cTL, cTR, cBR, cBL};
        int best = 0; float bestV = -1.f;
        for (int c = 0; c < 4; ++c) {
            float v = clear[c];
            if (!std::isfinite(v)) { best = c; bestV = v; break; }
            if (v > bestV) { best = c; bestV = v; }
        }
        fixedCorner[i] = best;
    }
    return fixedCorner;
}

// --- distance between a rect and an AABB (0 if touch/overlap) ---
static inline float rectGapToAABB(const Rect& a, const Rect& b) {
    return rectGap(a, b); // same metric as before
}

// -------------------- Quadtree for rectangles --------------------
struct QuadNode {
    Rect bounds;
    int depth;
    std::vector<Rect> items;           // rects that don't fit entirely in a child
    std::unique_ptr<QuadNode> child[4];
    QuadNode(const Rect& b, int d) : bounds(b), depth(d) {}
};

struct QuadRectIndex {
    const int maxDepth;
    const int cap; // max items in a leaf before split
    std::unique_ptr<QuadNode> root;

    QuadRectIndex(const Rect& world, int maxDepth_=14, int cap_=16)
        : maxDepth(maxDepth_), cap(cap_), root(std::make_unique<QuadNode>(world, 0)) {}

    static inline Rect childBounds(const Rect& b, int q) {
        const float mx = 0.5f*(b.xmin + b.xmax);
        const float my = 0.5f*(b.ymin + b.ymax);
        switch (q) {
            case 0: return {b.xmin, my,    mx,    b.ymax}; // TL
            case 1: return {mx,     my,    b.xmax,b.ymax}; // TR
            case 2: return {mx,     b.ymin,b.xmax,my    }; // BR
            default:return {b.xmin, b.ymin,mx,    my    }; // BL
        }
    }

    static inline int whichChild(const Rect& b, const Rect& r) {
        const float mx = 0.5f*(b.xmin + b.xmax);
        const float my = 0.5f*(b.ymin + b.ymax);
        const bool left  = r.xmax <= mx;
        const bool right = r.xmin >= mx;
        const bool bot   = r.ymax <= my;
        const bool top   = r.ymin >= my;
        // must fit fully in exactly one child to descend
        if (left && top)  return 0;
        if (right&& top)  return 1;
        if (right&& bot)  return 2;
        if (left && bot)  return 3;
        return -1; // spans multiple children -> keep here
    }

    void split(QuadNode* n) {
        if (n->child[0]) return;
        for (int q=0;q<4;++q)
            n->child[q] = std::make_unique<QuadNode>(childBounds(n->bounds,q), n->depth+1);
        // reinsert items that fit fully in a child
        std::vector<Rect> keep;
        keep.reserve(n->items.size());
        for (const Rect& r : n->items) {
            int c = whichChild(n->bounds, r);
            if (c >= 0) n->child[c]->items.push_back(r);
            else keep.push_back(r);
        }
        n->items.swap(keep);
    }

    void insert(const Rect& r) { insertRec(root.get(), r); }

    void insertRec(QuadNode* n, const Rect& r) {
        if (n->depth < maxDepth) {
            int c = whichChild(n->bounds, r);
            if (c >= 0) {
                if (!n->child[0]) split(n);
                insertRec(n->child[c].get(), r);
                return;
            }
        }
        n->items.push_back(r);
        if ((int)n->items.size() > cap && n->depth < maxDepth) {
            split(n);
        }
    }

    bool overlapsAny(const Rect& r) const { return overlapsAnyRec(root.get(), r); }

    bool overlapsAnyRec(const QuadNode* n, const Rect& r) const {
        if (!overlapsStrict(n->bounds, r) && rectGapToAABB(n->bounds, r) > 0.f) return false;
        for (const Rect& x : n->items)
            if (overlapsStrict(x, r)) return true;
        if (!n->child[0]) return false;
        for (int q=0;q<4;++q)
            if (overlapsAnyRec(n->child[q].get(), r)) return true;
        return false;
    }

    float minGapToAny(const Rect& r) const {
        float best = std::numeric_limits<float>::infinity();
        minGapRec(root.get(), r, best);
        return best;
    }

    void minGapRec(const QuadNode* n, const Rect& r, float& best) const {
        // prune by bbox lower bound
        float lb = rectGapToAABB(n->bounds, r);
        if (!(lb < best)) return;
        for (const Rect& x : n->items) {
            float g = rectGap(x, r);
            if (g < best) best = g;
            if (best == 0.f) return;
        }
        if (!n->child[0]) return;
        // visit children in order of lower bound (cheap heuristic)
        struct C { const QuadNode* node; float lb; };
        C kids[4];
        for (int i=0;i<4;++i) {
            kids[i].node = n->child[i].get();                               // FIX
            kids[i].lb   = rectGapToAABB(n->child[i]->bounds, r);           // FIX
        }
        std::sort(std::begin(kids), std::end(kids), [](const C& a, const C& b){ return a.lb < b.lb; });
        for (int i=0;i<4;++i) {
            if (!(kids[i].lb < best)) break;
            minGapRec(kids[i].node, r, best);
            if (best == 0.f) return;
        }
    }
};

// Grid for placed rectangles (fast overlap + gap)
struct RectGrid {
    float cs;
    std::vector<Rect> rects;
    std::unordered_map<CellKey, std::vector<int>, CellHash, CellEq> grid;

    explicit RectGrid(float cellSize, size_t expectedRects = 0) : cs(cellSize) {
        if (expectedRects) {
            rects.reserve(expectedRects);
            grid.reserve(expectedRects * 4); // heuristically ~4 cells per rect
        } else {
            grid.reserve(2048);
        }
    }

    void insert(const Rect& r) {
        const int id = (int)rects.size();
        rects.push_back(r);
        const int x0 = cellOf(r.xmin, cs), x1 = cellOf(r.xmax, cs);
        const int y0 = cellOf(r.ymin, cs), y1 = cellOf(r.ymax, cs);
        for (int cy = y0; cy <= y1; ++cy)
            for (int cx = x0; cx <= x1; ++cx)
                grid[{cx, cy}].push_back(id);
    }

    bool overlapsAny(const Rect& r) const {
        const int x0 = cellOf(r.xmin, cs), x1 = cellOf(r.xmax, cs);
        const int y0 = cellOf(r.ymin, cs), y1 = cellOf(r.ymax, cs);
        for (int cy = y0; cy <= y1; ++cy)
            for (int cx = x0; cx <= x1; ++cx) {
                auto it = grid.find({cx, cy});
                if (it == grid.end()) continue;
                for (int id : it->second)
                    if (overlapsStrict(r, rects[id])) return true;
            }
        return false;
    }

    float minGapToAny(const Rect& r) const {
        float best = std::numeric_limits<float>::infinity();
        const int x0 = cellOf(r.xmin, cs), x1 = cellOf(r.xmax, cs);
        const int y0 = cellOf(r.ymin, cs), y1 = cellOf(r.ymax, cs);
        for (int cy = y0; cy <= y1; ++cy)
            for (int cx = x0; cx <= x1; ++cx) {
                auto it = grid.find({cx, cy});
                if (it == grid.end()) continue;
                for (int id : it->second)
                    best = std::min(best, rectGap(r, rects[id]));
            }
        return best;
    }
};
// ------------------------------------------------------------

// Greedy with grids (O(n log n) due to one sort)
namespace {
static std::vector<Rect> greedyPlaceInternal(
    std::vector<LabelCandidate>& candidates,
    const std::vector<std::array<float,2>>& points
) {
    for (auto& c : candidates) c.valid = false;

    std::vector<Rect> placed;
    placed.reserve(points.size());
    if (points.empty() || candidates.empty()) return placed;

    const size_t perPoint = std::max<size_t>(1, candidates.size() / points.size());
    const float cs = candidates[0].size;

    // --- FIX: Use PointGrid for containment and RectGrid for overlap ---
    PointGrid pg(points, cs);
    RectGrid rg(cs, points.size());
    // --- END FIX ---

    auto rectHasOtherPoint = [&](const Rect& R, int skip)->bool {
        return pg.anyInside(R, skip);
    };
    // --- END FIX ---

    const int N = (int)points.size();

    // Simple O(1) hardness using density
    std::vector<float> pointKey(N);
    for (int i = 0; i < N; ++i) {
        int localDensity = pg.localCount(points[i][0], points[i][1]);
        pointKey[i] = -localDensity; // negative so higher density sorts first
    }

    // Precompute density once for tie-breaking
    std::vector<int> density(N);
    for (int i = 0; i < N; ++i) density[i] = pg.localCount(points[i][0], points[i][1]);

    std::vector<int> order(N); 
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b){
        if (pointKey[a] != pointKey[b]) return pointKey[a] < pointKey[b];
        if (density[a] != density[b])   return density[a] > density[b];
        return a < b;
    });

    for (int pid : order) {
        float bestScore = std::numeric_limits<float>::infinity();
        int   bestIdx   = -1;
        Rect  bestRect{};
        const size_t base = (size_t)pid * perPoint;

        for (size_t j = 0; j < perPoint && base + j < candidates.size(); ++j) {
            const size_t k = base + j;
            const Rect r = getAABB(candidates[k]);

            if (rectHasOtherPoint(r, pid)) continue;   // O(1) expected with grid
            if (rg.overlapsAny(r))           continue; // O(1) expected with grid

            float score = rg.minGapToAny(r);
            if (!std::isfinite(score)) score = 0.f;
            if (score < bestScore) { bestScore = score; bestIdx = (int)k; bestRect = r; }
            if (bestScore == 0.f) break;
        }

        if (bestIdx >= 0) {
            candidates[bestIdx].valid = true;
            placed.push_back(bestRect);
            rg.insert(bestRect);
        }
    }
    return placed;
}
} // namespace

// Monotone greedy: on zoom-in keep only a feasible subset of the previous labels;
// on zoom-out recompute greedily to add more.
std::vector<Rect>
greedyPlaceMonotone(std::vector<LabelCandidate>& candidates,
                    const std::vector<std::array<float,2>>& points,
                    float baseSize,
                    MonotoneState* state) { // state is now a pointer
    
    for (auto& c : candidates) { c.size = baseSize; c.valid = false; }
    
    std::vector<Rect> placed;
    const int N = (int)points.size();
    if (N == 0 || candidates.empty()) {
        *state = {}; state->lastBase = baseSize; return placed;
    }

    const int perPoint = 4;

    // 1) Determine fixed corners
    if ((int)state->fixedCorner.size() != N) {
        state->fixedCorner = chooseFixedCornersByConflicts(points, baseSize);
    }

    // 2) Set corners on all candidates
    for (int pid = 0; pid < N; ++pid) {
        const int corner = state->fixedCorner[pid];
        for (int j = 0; j < perPoint && pid * perPoint + j < (int)candidates.size(); ++j) {
            candidates[pid * perPoint + j].corner = corner;
        }
    }

    // 3) Apply "used once" rule
    if ((int)state->usedOnce.size() != N) state->usedOnce.assign(N, 0);

    const bool havePrev = state->lastBase >= 0.f;
    const bool zoomingOut = !havePrev || baseSize < state->lastBase;

    // Build fast indices for this pass
    PointGrid pg(points, baseSize);
    RectGrid rg(baseSize, points.size());
    auto rectHasOtherPoint = [&](const Rect& R, int skip)->bool {
        return pg.anyInside(R, skip);
    };

    std::vector<int> next_active; // Temporary vector for the new active set
    next_active.reserve(N);

    // 4) Keep feasible previous labels
    std::vector<int> keep;
    keep.reserve(state->active.size());
    for (int idx : state->active) {
        int pid = ownerOf(idx, perPoint);
        if (pid >= 0 && pid < N) {
            keep.push_back(pid * perPoint + state->fixedCorner[pid]);
        }
    }
    std::sort(keep.begin(), keep.end());
    keep.erase(std::unique(keep.begin(), keep.end()), keep.end());

    std::vector<unsigned char> isActiveNow(N, 0);
    for (int idx : keep) {
        const int pid = ownerOf(idx, perPoint);
        const Rect r = getAABB(candidates[idx]);
        if (rectHasOtherPoint(r, pid) || rg.overlapsAny(r)) continue;
        
        candidates[idx].valid = true;
        rg.insert(r);
        placed.push_back(r);
        next_active.push_back(idx);
        isActiveNow[pid] = 1;
        state->usedOnce[pid] = 1;
    }

    // 5) On zoom-out: add new labels
    if (zoomingOut) {
        std::vector<int> order;
        order.reserve(N);
        for (int pid = 0; pid < N; ++pid) {
            if (!isActiveNow[pid]) order.push_back(pid);
        }
        
        // --- FIX: Cache density before sorting ---
        std::vector<int> dens(N);
        for (int pid = 0; pid < N; ++pid) {
            dens[pid] = pg.localCount(points[pid][0], points[pid][1]);
        }
        std::sort(order.begin(), order.end(), [&](int a, int b){
            return dens[a] > dens[b];
        });
        // --- END FIX ---

        for (int pid : order) {
            const int k = pid * perPoint + state->fixedCorner[pid];
            const Rect r = getAABB(candidates[k]);
            if (rectHasOtherPoint(r, pid) || rg.overlapsAny(r)) continue;

            candidates[k].valid = true;
            rg.insert(r);
            placed.push_back(r);
            next_active.push_back(k);
            state->usedOnce[pid] = 1;
        }
    }

    state->active = std::move(next_active); // Update the active set
    state->lastBase = baseSize;

    return placed;
}

// Keep the internal helper in the anonymous namespace above untouched.

// Exported shim to satisfy old call sites and enforce monotone + usedOnce.
std::vector<Rect>
greedyPlaceOneLabelPerPoint(std::vector<LabelCandidate>& candidates,
                            const std::vector<std::array<float,2>>& points) {
    static MonotoneState s; // The single, persistent state object
    const float baseSize = candidates.empty() ? 0.02f : candidates[0].size;
    return greedyPlaceMonotone(candidates, points, baseSize, &s);
}
