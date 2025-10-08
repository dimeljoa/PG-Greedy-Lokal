#include "greedy_labeler.hpp"
#include "visualizer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

// Helper: safe parse int
static bool tryParseInt(const std::string& s, int& out) {
    try {
        size_t idx = 0; int v = std::stoi(s, &idx); if (idx == s.size()) { out = v; return true; } } catch (...) {}
    return false;
}
static bool tryParseFloat(const std::string& s, float& out) {
    try { size_t idx = 0; float v = std::stof(s, &idx); if (idx == s.size()) { out = v; return true; } } catch (...) {}
    return false;
}

int main(int argc, char** argv) {
    // Modes:
    //   Random: [numPoints] [minDomain] [maxDomain] [shaderPath]
    //   CSV: --input=path/to/file.csv
    // Flags:
    //   --input=FILE         load CSV (x,y[,side][,corner])
    //   --shader=DIR         shader directory (default shaders)
    //   --base-size=SIZE     override per-point sizes, regenerate uniform candidates
    //   --cap-inf=SIZE       display cap for INF side values (default 5.0)

    int numPoints = 100000;
    float minDomain = -1.f;
    float maxDomain = 1.f;
    std::string shaderPath = "shaders";
    float baseOverride = -1.f;
    float infCap = 5.0f;
    std::string inputCSV;

    // First scan flags
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.rfind("--input=",0)==0) inputCSV = a.substr(8);
        else if (a.rfind("--shader=",0)==0) shaderPath = a.substr(9);
        else if (a.rfind("--base-size=",0)==0) baseOverride = std::stof(a.substr(12));
        else if (a.rfind("--cap-inf=",0)==0) infCap = std::stof(a.substr(10));
    }

    bool csvMode = !inputCSV.empty();

    // Only parse positional numbers if not CSV mode
    if (!csvMode) {
        if (argc > 1) { int tmp; if (tryParseInt(argv[1], tmp)) numPoints = tmp; }
        if (argc > 2) { float tmp; if (tryParseFloat(argv[2], tmp)) minDomain = tmp; }
        if (argc > 3) { float tmp; if (tryParseFloat(argv[3], tmp)) maxDomain = tmp; }
        if (argc > 4) shaderPath = argv[4];
    }

    auto t0 = std::chrono::high_resolution_clock::now();

    std::vector<std::array<float,2>> points;
    std::vector<LabelCandidate> candidates;

    if (csvMode) {
        std::ifstream in(inputCSV);
        if (!in) { std::cerr << "Could not open input CSV: " << inputCSV << "\n"; return -1; }
        std::string line; bool first = true; size_t lineNum = 0;
        std::vector<float> perSide;
        std::vector<int> perCorner;
        while (std::getline(in, line)) {
            ++lineNum;
            if (line.empty()) continue;
            if (first) {
                first = false;
                bool hasAlpha = false; for (unsigned char c: line) { if (std::isalpha(c)) { hasAlpha = true; break; } }
                if (hasAlpha) { // treat as header, skip
                    continue;
                }
            }
            std::replace(line.begin(), line.end(), ';', ',');
            std::stringstream ss(line);
            std::string xs, ys, sideStr, cornerStr;
            if (!std::getline(ss, xs, ',')) continue;
            if (!std::getline(ss, ys, ',')) continue;
            // side & corner optional
            std::getline(ss, sideStr, ',');
            std::getline(ss, cornerStr, ',');

            float x,y; if(!tryParseFloat(xs,x) || !tryParseFloat(ys,y)) continue;
            points.push_back({x,y});

            float s = (baseOverride > 0.f) ? baseOverride : 0.02f; // default
            if (!sideStr.empty()) {
                if (sideStr == "INF" || sideStr == "inf" || sideStr == "+inf" || sideStr == "+INF") s = infCap; else {
                    float maybe; if (tryParseFloat(sideStr, maybe)) { if (maybe > 1e-4f) s = maybe; else s = 1e-4f; }
                }
            }
            perSide.push_back(s);

            int cidx = -1;
            if (!cornerStr.empty()) { int maybe; if (tryParseInt(cornerStr, maybe)) cidx = maybe; }
            perCorner.push_back(cidx);
        }

        // Build candidates
        candidates.reserve(points.size()*4);
        for (size_t i=0;i<points.size();++i) {
            auto local = generateLabelCandidates({points[i]}, perSide[i]);
            if (baseOverride > 0.f) {
                for (auto &c: local) c.valid = false; // override ignores CSV corner validity
            } else {
                int chosen = perCorner[i];
                if (chosen >= 0 && chosen < 4) {
                    for (auto &c: local) c.valid = (c.corner == chosen);
                } else {
                    for (auto &c: local) c.valid = false; // user can interactively place
                }
            }
            candidates.insert(candidates.end(), local.begin(), local.end());
        }
        std::cout << "Loaded " << points.size() << " points from CSV: " << inputCSV << "\n";
    } else {
        std::mt19937_64 rng{std::random_device{}()};
        std::uniform_real_distribution<float> dist(minDomain, maxDomain);
        points.reserve(numPoints);
        for (int i=0;i<numPoints;++i) points.push_back({dist(rng), dist(rng)});
        float base = (baseOverride > 0.f) ? baseOverride : 0.02f;
        candidates = generateLabelCandidates(points, base);
        static MonotoneState monoState; // random mode monotone
        greedyPlaceMonotone(candidates, points, base, &monoState);
        std::cout << "Generated random " << numPoints << " points in domain [" << minDomain << ", " << maxDomain << "]\n";
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::cout << "Setup completed in " << elapsed << " ms\n";

    VisualizerConfig vcfg; vcfg.points = points; vcfg.candidates = candidates; vcfg.shaderPath = shaderPath;
    PointLabelVisualizer viz(vcfg);
    try {
        if (!viz.init()) { std::cerr << "Failed to initialize visualizer\n"; return -1; }
        viz.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n"; return -1; }
    return 0;
}