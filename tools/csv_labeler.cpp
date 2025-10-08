#include "greedy_labeler.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <numeric>

// Simple command-line option helper
struct ArgsConfig {
    std::string inPath;
    std::string outPath;
    float Smin = 1e-4f;
    float Smax = -1.f; // auto via span if <0
    // Tuned for higher coverage (~70%+) without exploding runtime
    float epsRel = 6e-5f;   // tighter than 1e-4 => finer threshold resolution
    float growth = 1.2f;   // moderate coarse step; avoids 100s of runs from 1.003f
    int maxGrowth = 56;     // enough to span typical Smax/Smin ratios
    int maxRefine = 64;     // deeper refinement to tighten intervals
    bool multiSample = true;// enable geometric sweep to seed bounds
    int multiSamples = 0;   // auto choose if 0
};

static void printUsage(){
    std::cerr << "Usage: csv_labeler <input.csv> <output.csv> [options]\n"
              << "Options:\n"
              << "  --smin v          Minimum size (default 1e-4)\n"
              << "  --smax v          Maximum size (default = span)\n"
              << "  --growth g        Growth factor (>1) (default 1.24)\n"
              << "  --max-growth n    Max coarse growth iterations (default 56)\n"
              << "  --max-refine n    Max refinement iterations (default 64)\n"
              << "  --eps-rel r       Relative epsilon factor (default 6e-5)\n"
              << "  --multi-sample k  Pre-sample k geometric sizes (0=auto auto)\n"
              << "  --multi           Force enable geometric pre-sampling (default on)\n"
              << std::endl;
}

// ---------------- Batch / hybrid search for uniform zoom thresholds ----------------
struct ThresholdResult {
    std::vector<float> size;
    std::vector<int> corner;
    int growthRuns = 0;   // number of greedy runs during growth
    int refineRuns = 0;   // number of greedy runs during refinement
    int sweepRuns = 0;    // optional multi-sample passes
};

// Stateless per-scale test
static void runAtScale(const std::vector<std::array<float,2>>& pts, float S,
                       std::vector<unsigned char>& aliveOut,
                       std::vector<int>& chosenCorner) {
    auto cand = generateLabelCandidates(pts, S);
    greedyPlaceOneLabelPerPoint(cand, pts);
    int N = (int)pts.size();
    aliveOut.assign(N, 0);
    chosenCorner.assign(N, -1);
    for (int i=0;i<N;++i) {
        for (int c=0;c<4;++c) {
            const auto &C = cand[i*4 + c];
            if (C.valid) { aliveOut[i]=1; chosenCorner[i]=C.corner; break; }
        }
    }
}

static ThresholdResult computeZoomThresholds(const std::vector<std::array<float,2>>& pts,
                                             float Smin, float Smax,
                                             float eps, float growth,
                                             int maxGrowth, int maxRefine,
                                             bool multiSample, int multiSamples) {
    ThresholdResult r; int N = (int)pts.size();
    r.size.assign(N, Smin); r.corner.assign(N, 0);
    if (N == 0) return r;

    struct Interval { float lo, hi; bool resolved; };
    std::vector<Interval> iv(N, {Smin, Smax, false});
    std::vector<int> alive(N, 1);

    // Optional geometric sweep pre-pass to densify sampling
    if (multiSample) {
        if (multiSamples <= 0) {
            // choose count so that growth^k ~ Smax/Smin => k ~ log(Smax/Smin)/log(growth)
            multiSamples = std::max(8, (int)std::ceil(std::log(Smax/Smin)/std::log(growth))); // ensure >=8
        }
        float logMin = std::log(Smin);
        float logMax = std::log(Smax);
        for (int i=0;i<multiSamples;i++) {
            float t = (multiSamples==1)?0.f : (float)i/(multiSamples-1);
            float S = std::exp(logMin + t*(logMax - logMin));
            std::vector<unsigned char> aliveNow; std::vector<int> chosenNow;
            runAtScale(pts, S, aliveNow, chosenNow);
            r.sweepRuns++;
            for (int p=0;p<N;++p) {
                if (aliveNow[p]) {
                    if (S > iv[p].lo) { // extend lower bound if bigger
                        iv[p].lo = S; r.size[p] = S; if (chosenNow[p]>=0) r.corner[p]=chosenNow[p];
                    }
                } else {
                    // shrink hi if first time dead above current lo
                    if (S < iv[p].hi) iv[p].hi = S;
                }
            }
        }
        // mark resolved if tight already
        for (int i=0;i<N;++i) if (iv[i].hi - iv[i].lo <= eps) iv[i].resolved = true;
    }

    // Growth phase (coarse expansion)
    float S = (Smin > 0 ? Smin : 1e-4f);
    for (int g=0; g<maxGrowth && S < Smax; ++g) {
        std::vector<unsigned char> aliveNow; std::vector<int> chosenNow;
        runAtScale(pts, S, aliveNow, chosenNow);
        r.growthRuns++;
        for (int i=0;i<N;++i) {
            if (aliveNow[i]) {
                if (S > iv[i].lo) { iv[i].lo = S; r.size[i]=S; if(chosenNow[i]>=0) r.corner[i]=chosenNow[i]; }
            } else if (alive[i]) { iv[i].hi = S; alive[i]=0; }
        }
        bool anyAlive=false; for(int i=0;i<N;++i) if(alive[i]) { anyAlive=true; break; }
        if(!anyAlive) break;
        S *= growth; if (S > Smax) S = Smax;
    }
    for (int i=0;i<N;++i) if (alive[i]) iv[i].hi = std::min(iv[i].hi, Smax);

    // Refinement (batched median probing)
    for (int iter=0; iter<maxRefine; ++iter) {
        std::vector<float> mids; mids.reserve(N);
        for (int i=0;i<N;++i) if(!iv[i].resolved && iv[i].hi - iv[i].lo > eps) mids.push_back(0.5f*(iv[i].lo+iv[i].hi));
        if (mids.empty()) break;
        std::nth_element(mids.begin(), mids.begin()+mids.size()/2, mids.end());
        float testS = mids[mids.size()/2];
        std::vector<unsigned char> aliveNow; std::vector<int> chosenNow;
        runAtScale(pts, testS, aliveNow, chosenNow);
        r.refineRuns++;
        bool anyUnresolved=false;
        for (int i=0;i<N;++i) {
            if (iv[i].resolved) continue;
            if (aliveNow[i]) { iv[i].lo = testS; r.size[i]=testS; if(chosenNow[i]>=0) r.corner[i]=chosenNow[i]; }
            else { iv[i].hi = testS; }
            if (iv[i].hi - iv[i].lo <= eps) iv[i].resolved = true; else anyUnresolved=true;
        }
        if (!anyUnresolved) break;
    }
    return r;
}

static bool read_points_csv(const std::string& path, std::vector<std::array<float,2>>& pts) {
    std::ifstream in(path);
    if (!in) { std::cerr << "Failed to open input: " << path << "\n"; return false; }
    std::string line; bool first = true;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (first) {
            first = false;
            bool hasAlpha = false; for (unsigned char c: line) { if (std::isalpha(c)) { hasAlpha = true; break; } }
            if (hasAlpha) continue; // skip header row
        }
        std::replace(line.begin(), line.end(), ';', ',');
        std::stringstream ss(line);
        std::string xs, ys; if (!std::getline(ss, xs, ',')) continue; if (!std::getline(ss, ys, ',')) continue;
        try { float x = std::stof(xs); float y = std::stof(ys); pts.push_back({x,y}); } catch (...) { /* ignore */ }
    }
    return true;
}

static bool write_results_csv(const std::string& path,
                              const std::vector<std::array<float,2>>& pts,
                              const std::vector<LabelCandidate>& cands) {
    std::ofstream out(path);
    if (!out) { std::cerr << "Failed to write output: " << path << "\n"; return false; }
    out << "x,y,side,size,corner\n"; // extended header: include corner explicitly last
    const int perPoint = 4;
    for (int i=0;i<(int)pts.size();++i) {
        int base = i*perPoint; int chosen = 0; float side = std::numeric_limits<float>::infinity(); bool found=false;
        for (int j=0;j<4;++j) { const auto& c = cands[base+j]; if (c.valid) { chosen = c.corner; side = c.size; found=true; break; } }
        if (!found) side = std::numeric_limits<float>::infinity();
        out << pts[i][0] << "," << pts[i][1] << ",";
        if (std::isfinite(side)) out << side; else out << "INF";
        out << "," << (found?side:0) << "," << chosen << "\n"; // keep size duplicate for compatibility
    }
    return true;
}

static ArgsConfig parseArgs(int argc, char** argv) {
    ArgsConfig cfg; if (argc < 3) return cfg;
    cfg.inPath = argv[1]; cfg.outPath = argv[2];
    for (int i=3;i<argc;++i) {
        std::string a = argv[i];
        auto need = [&](int &i){ if(i+1>=argc){ std::cerr<<"Missing value after "<<a<<"\n"; return false;} return true; };
        if (a == "--smin" && need(i)) { cfg.Smin = std::stof(argv[++i]); }
        else if (a == "--smax" && need(i)) { cfg.Smax = std::stof(argv[++i]); }
        else if (a == "--growth" && need(i)) { cfg.growth = std::stof(argv[++i]); }
        else if (a == "--max-growth" && need(i)) { cfg.maxGrowth = std::stoi(argv[++i]); }
        else if (a == "--max-refine" && need(i)) { cfg.maxRefine = std::stoi(argv[++i]); }
        else if (a == "--eps-rel" && need(i)) { cfg.epsRel = std::stof(argv[++i]); }
        else if (a == "--multi-sample" && need(i)) { cfg.multiSamples = std::stoi(argv[++i]); cfg.multiSample = true; }
        else if (a == "--multi") { cfg.multiSample = true; }
        else if (a == "--help" || a == "-h") { printUsage(); }
    }
    return cfg;
}

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    std::cout.setf(std::ios::unitbuf);
    if (argc < 3) { printUsage(); return 2; }
    auto cfg = parseArgs(argc, argv);
    if (cfg.inPath.empty()) { printUsage(); return 2; }

    std::vector<std::array<float,2>> points; if(!read_points_csv(cfg.inPath, points)){ return 3; }
    if (points.empty()) { std::cerr << "No points loaded.\n"; return 4; }

    float minX=points[0][0], maxX=minX, minY=points[0][1], maxY=minY;
    for(auto &p:points){ if(p[0]<minX)minX=p[0]; if(p[0]>maxX)maxX=p[0]; if(p[1]<minY)minY=p[1]; if(p[1]>maxY)maxY=p[1]; }
    float span = std::max(maxX-minX, maxY-minY); if(span<=0) span=1.0f;

    float Smin = std::max(1e-6f, cfg.Smin);
    float Smax = (cfg.Smax>0? cfg.Smax : span);
    float eps = span * cfg.epsRel + 1e-6f;

    std::cout << "Points: " << points.size() << " span="<<span
              << " Smin="<<Smin<<" Smax="<<Smax<<" eps="<<eps<<"\n";
    std::cout << "Params: growth="<<cfg.growth<<" maxGrowth="<<cfg.maxGrowth
              << " maxRefine="<<cfg.maxRefine
              << (cfg.multiSample?" multiSample=on":" multiSample=off")
              << " epsRel="<<cfg.epsRel
              << "\n";

    auto tStart = std::chrono::high_resolution_clock::now();
    auto thresholds = computeZoomThresholds(points, Smin, Smax, eps,
                                            cfg.growth, cfg.maxGrowth, cfg.maxRefine,
                                            cfg.multiSample, cfg.multiSamples);
    auto tEnd = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    // Build candidates for output
    auto candidates = generateLabelCandidates(points, 0.0f);
    for (int i=0;i<(int)points.size();++i) {
        int chosen = thresholds.corner[i];
        for (int c=0;c<4;++c) {
            auto &cand = candidates[i*4 + c];
            cand.valid = (c==chosen);
            if (cand.valid) { cand.corner = chosen; cand.size = thresholds.size[i]; }
        }
    }

    std::cout << "Runs: sweep="<<thresholds.sweepRuns
              << " growth="<<thresholds.growthRuns
              << " refine="<<thresholds.refineRuns
              << " total(ms)="<<ms << "\n";

    write_results_csv(cfg.outPath, points, candidates);
    // Coverage metric (percentage of points that received a valid finite label)
    size_t labeled=0; for(size_t i=0;i<points.size();++i){
        bool any=false; for(int c=0;c<4;++c){ if(candidates[i*4+c].valid && std::isfinite(candidates[i*4+c].size)){ any=true; break; } }
        if(any) ++labeled;
    }
    double coveragePct = points.empty()?0.0:100.0*double(labeled)/double(points.size());
    std::cout << "Coverage: "<<labeled<<"/"<<points.size()<<" = "<<coveragePct<<"%\n";
    std::cout << "Wrote "<<cfg.outPath<<"\n";
    return 0;
}
