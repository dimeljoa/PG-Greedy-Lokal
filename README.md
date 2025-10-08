## PG Greedy Lokal – Point Label Placement

High‑performance greedy point labeling in C++ with:

- Monotone zoom behavior (labels stay stable while zooming in, only add on zoom out)
- Grid‑accelerated conflict checks (no KD‑tree dependency any more)
- Two front‑ends:
	- `csv_labeler`: batch compute per‑point maximum feasible label size + chosen corner
	- `labeler_example`: interactive OpenGL/ImGui visualization of points and labels
- Optional geometric multi‑sampling + growth/refinement search for fast size threshold discovery
- Clean CSV outputs for downstream analysis / plotting

---

## 1. Features

| Area | Description |
|------|-------------|
| Candidate Model | 4 square candidates (TL, TR, BR, BL) per point; one chosen greedily. |
| Conflict Rules | No overlaps and labels must not cover other point centers (open interior). |
| Monotone State | Remembers fixed corner + active set to keep layout stable across size changes. |
| Search (CLI) | Coarse geometric growth + median refinement + optional logarithmic pre‑sampling. |
| Performance | Near‑linear scaling: ~70–90 ms for 100k points on commodity hardware (your runs). |
| Output | CSV: `x,y,side,size,corner` (`side=size` for convenience, `INF` when unlabeled). |
| Analysis | Python helper (optional) to aggregate runtimes & coverage (see `tools/analyze_greedy.py`). |

---

## 2. Build

Requires CMake (>=3.20 recommended) and a C++17 compiler.

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target csv_labeler labeler_example
```

Executables end up in `build/` (on multi‑config generators you may have a `Release/` subfolder).

---

## 3. CLI: `csv_labeler`

Computes, for each input point, the largest label size (up to span) that still allows a label at one of the four corners.

### Usage
```powershell
csv_labeler <input.csv> <output.csv> [options]
```

Key options (defaults in parentheses):

| Flag | Meaning |
|------|---------|
| `--smin v` | Minimum test size (1e-4) |
| `--smax v` | Maximum size (auto = span of data) |
| `--growth g` | Geometric growth factor (1.2) |
| `--max-growth n` | Max coarse growth iterations (56) |
| `--max-refine n` | Max refinement iterations (64) |
| `--eps-rel r` | Relative termination tolerance (6e-5) |
| `--multi-sample k` | Pre-sample k log-spaced sizes (auto if 0) |
| `--multi` | Force enable geometric pre-sampling |

### Example
```powershell
csv_labeler data\points_10000.csv out\labels_10000.csv --growth 1.22 --max-refine 80
```

### Output Schema
Header: `x,y,side,size,corner`

- `x,y` – original point
- `side` – final chosen label side length or `INF` (no label)
- `size` – duplicate (compat / plotting)
- `corner` – 0=TL,1=TR,2=BR,3=BL

Additional stdout summary: total points, parameters, run counts, coverage percentage.

---

## 4. Interactive Viewer: `labeler_example`

Load an existing CSV and explore placements visually.

```powershell
labeler_example --input=build\results\points_50000_type_0_noise_0.csv
```

Optional flags:
- `--shader=shaders` – path to GLSL shaders (default `shaders`)
- `--base-size=0.02` – override all per‑point sizes & regenerate candidates uniformly
- `--cap-inf=5.0` – visualization cap when displaying `INF` sizes

CSV interpretation for viewer:
- If a row has `side` + `corner`, that corner is pre‑selected. Otherwise candidate remains user‑placeable.

---

## 5. Performance Snapshot

Empirical (your measurements, Windows; grid-based implementation):

| Points | Setup (ms) | ~µs / point |
|--------|------------|-------------|
| 1k     | 1          | 1.0 |
| 10k    | 8–10       | 0.8–1.0 |
| 50k    | 34–38      | ~0.7 |
| 100k   | 65–88      | 0.65–0.88 |

Scaling exponent (log fit 5k–100k): α ≈ 0.96–0.97 (effectively linear + small constant overhead).

---

## 6. Analysis (Optional)

`tools/analyze_greedy.py` can batch run `csv_labeler`, parse stdout, and produce:

- `greedy_runs.csv`
- `runtime_vs_n.csv`
- `coverage_vs_n.csv`
- `parameter_sensitivity.csv` (if `--sensitivity`)

Example:
```powershell
python tools\analyze_greedy.py --points results --output metrics --growth 1.24 --max-refine 64 --multi-sample 1
```

---

## 7. Directory Layout

| Path | Purpose |
|------|---------|
| `src/` | Core algorithm + ImGui integration |
| `include/` | Public headers (algorithm + ImGui headers vendored) |
| `tools/` | CLI utilities (`csv_labeler`, analysis script) |
| `example/` | `main.cpp` for interactive viewer |
| `results/` | Sample or generated input/outputs (user supplied) |
| `shaders/` | GLSL shader sources for viewer |

---

## 8. Roadmap / Ideas

- Adaptive growth factor based on early alive ratio
- Parallel candidate evaluation (OpenMP or std::execution) for very large N
- Optional rectangle packing heuristics (score = distance to nearest conflict)
- Toggle for alternative output schema (`x,y,Size,Corner` legacy) via `--legacy-schema`
- Lightweight test suite (GoogleTest or Catch2) for regression on corner stability

Contributions and experiment feedback welcome.

---

## 9. License

Add a LICENSE file (e.g. MIT) to clarify usage if you plan to publish—currently unspecified.

---

## 10. Quick Start (TL;DR)

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target csv_labeler labeler_example
csv_labeler results\points_10000_type_0_noise_0.csv out\labels_10000.csv
labeler_example --input=out\labels_10000.csv
```

---

Feel free to open issues or request additional plots / metrics generation.
