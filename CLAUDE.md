# CLAUDE.md — MASS-Easy project directives

Guidance for AI assistants (and humans) working in this repository.

## Language policy (IMPORTANT)

- **All artifacts in the repository MUST be in English**: source code, code comments,
  identifiers, UI strings/labels, README, docs, commit messages, and log/status text.
- When you touch a file that still contains Portuguese (this project was bootstrapped in
  pt-BR), **translate it to English** as you edit — UI labels, comments, panel titles, etc.
- Chat replies to the user may be in Portuguese (the user's language); the **code and
  product are English-only**. Do not introduce new Portuguese into the codebase.
- Keep full orthographic correctness in any human-facing English text.

> Translation status: much of the Arena UI and Docs/ were originally written in Portuguese
> and are being migrated to English incrementally. Prefer translating strings you touch.

## What this project is

**MASS-Easy** — a fork of [MASS](https://github.com/lsw9021/MASS) (Muscle-Actuated Skeletal
System, SIGGRAPH 2019). Goals: make it easy to use on **Windows**, and viable for
**animation**. Keeps the original simulation + PPO training pipeline; adds a Windows-native
build and a unified 3D editor called **Arena**.

Always credit the original project and keep the link in README.

## Architecture

- `core/` — C++ library `mss`: DART 6.15 + Bullet physics, Hill muscles, RL `Environment`.
- `python/` — `pymss` pybind11 module + PPO loop (`main.py`, `Model.py`).
- `render/` — original MASS OpenGL viewer (runs trained `.pt` models).
- `editor/` — **Arena**, the unified 3D editor (binary `arena.exe`). GLFW + Dear ImGui +
  ImGuizmo + OpenGL; embeds DART for live sim and Boost.Asio for training telemetry.
- `scripts/` — PowerShell build/run. `Docs/` — task-based guides.
- `Deps/` — vcpkg + Python venv (git-ignored).

## Build & run (Windows)

Toolchain: Visual Studio 2026 (MSVC 14.51), CMake, Python 3.10 (base install has the dev
headers/libs), vcpkg, PyTorch CUDA. Never commit `build/`, `Dist/`, `Deps/`, `*.pyd`,
`imgui.ini`, `nn/`.

```powershell
scripts\build-dist.ps1      # configure + build all (core, pymss, render, arena) -> Dist\x64
scripts\arena.ps1           # launch Arena (the 3D editor)
scripts\train.ps1           # PPO training on GPU
scripts\view.ps1 -Nn ..\nn\max.pt -MuscleNn ..\nn\max_muscle.pt
```

CMake must receive explicit Python artifacts or `FindPython` fails on `Development.Module`:
`-DPython_EXECUTABLE=<base>\python.exe -DPython_INCLUDE_DIR=<base>\include -DPython_LIBRARY=<base>\libs\python310.lib`.
Generator: `"Visual Studio 18 2026" -A x64`. After adding/removing source files, re-run
the CMake configure step (the build uses `file(GLOB ...)`).

## Arena editor notes

- Single project file **`.mass`** (JSON) holds skeleton + muscles + motions + training +
  anatomy metadata + scene lights. "Export for training" decomposes it into
  `human.xml` + `muscle284.xml` + `metadata.txt` (round-trip validated against pymss).
- Bones render from the model's real OBJ meshes (`data/OBJ/*.obj`), uploaded once to the GPU
  and transformed per body in the vertex shader; reactive to `obj`/type edits.
- Muscles render as smooth fusiform tubes (Catmull-Rom, radius from PCSA = f0/specific_tension).
- Skin is generated procedurally (metaballs of body ellipsoids + smooth-union + Marching
  Cubes) into a continuous mesh; editable via parameters (Skin panel). Roadmap: include
  muscle volume in the field, vertex sculpt, and skinning to follow motion.
- Picking ray-casts the actual mesh triangles (what you see is what you click).

## Critical gotchas (do not regress)

- **OpenMP conflict**: torch bundles `libiomp5md.dll`; DART/pymss also load OpenMP. Set env
  `KMP_DUPLICATE_LIB_OK=TRUE` before running Arena or training (the scripts do this).
- **GLSL array uniforms** need the `[0]` suffix in `glGetUniformLocation` (e.g. `"uLcol[0]"`)
  or NVIDIA returns -1 and the data silently doesn't upload.
- **3D viewport** renders into an FBO shown via `ImGui::Image` (NOT a transparent window over
  the backbuffer — that composited a dark layer and dimmed everything ~60%).
- **NumPy 2.x**: `np.array(list_of_namedtuples, dtype=object)` in `main.py`.
- MSVC does not pull transitive headers like GCC — include `<numeric>`, `<algorithm>`, etc.

## Conventions

- Match surrounding code style. Keep changes minimal and focused.
- Commit only when asked. On commits, end the message with the Co-Authored-By trailer.
- Verify builds after changes; for the editor, a window screenshot is a good smoke test.
