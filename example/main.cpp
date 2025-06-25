#include "greedy_labeler.hpp"
#include "visualizer.hpp"
#include <random>
#include <array>
#include <vector>
#include <iostream>
#include <chrono>

struct AppConfig {
    int    numPoints = 2300;
    float  minDomain = -1.0f;
    float  maxDomain =  1.0f;
};

int main(int argc, char** argv) {
    AppConfig cfg;

    // Start timing full pipeline (point generation + labeling)
    auto t0 = std::chrono::high_resolution_clock::now();

    // 1) Generate random points
    std::mt19937_64 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> dist(cfg.minDomain, cfg.maxDomain);

    std::vector<std::array<float,2>> points;
    points.reserve(cfg.numPoints);
    for (int i = 0; i < cfg.numPoints; ++i) {
        points.push_back({ dist(rng), dist(rng) });
    }

    // 2) Create label candidates
    auto candidates = generateLabelCandidates(points);

    // 3) Greedy placement
    greedyPlaceOneLabelPerPoint(candidates);

    // End timing
    auto t1 = std::chrono::high_resolution_clock::now();
    auto msTotal = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    std::cout << "Full labeling pipeline (" << cfg.numPoints << " points) took "
              << msTotal.count() << " ms\n";

    // 4) Configure visualizer
    VisualizerConfig vcfg;
    vcfg.points     = std::move(points);
    vcfg.candidates = std::move(candidates);
    vcfg.shaderPath = (argc > 1 ? argv[1] : "shaders");

    // 5) Initialize and run visualizer
    PointLabelVisualizer viz(vcfg);
    try {
        if (!viz.init()) {
            std::cerr << "Visualizer initialization failed\n";
            return -1;
        }
        viz.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return -1;
    }

    return 0;
}