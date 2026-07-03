# MASS on Windows (native, MSVC + vcpkg)

Windows-native port of MASS. Physics via DART 6.15 (built from source by vcpkg),
training via PyTorch (CUDA), viewer via freeglut/OpenGL.

## Prerequisites (already provisioned on this machine)

- **Visual Studio 2026** (VS18) with the C++ toolchain (MSVC 14.51).
- **CMake 3.16+** (4.2 in use).
- **Python 3.10** at `C:\Users\Hermes\AppData\Local\Programs\Python\Python310`
  (needs the `include\` headers and `libs\python310.lib`, shipped by the python.org installer).
- **NVIDIA GPU** (RTX 2060 SUPER) with a current driver for CUDA training.

## Dependency layout (`Deps/`, git-ignored)

- `Deps/vcpkg/` — vcpkg, provides `dartsim[collision-bullet,gui,utils]`, `tinyxml`,
  `pybind11`, `freeglut` (+ transitive eigen3, assimp, bullet3) for the `x64-windows` triplet.
- `Deps/venv/` — Python 3.10 virtualenv with `torch` (cu124), `numpy`, `matplotlib`, `ipython`.

Recreate deps if needed:

```powershell
git clone https://github.com/microsoft/vcpkg Deps\vcpkg
Deps\vcpkg\bootstrap-vcpkg.bat -disableMetrics
Deps\vcpkg\vcpkg.exe install "dartsim[collision-bullet,gui,utils]" tinyxml pybind11 freeglut --triplet x64-windows

py -3.10 -m venv Deps\venv
Deps\venv\Scripts\python.exe -m pip install torch torchvision --index-url https://download.pytorch.org/whl/cu124
Deps\venv\Scripts\python.exe -m pip install numpy matplotlib ipython
```

## Build

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
```

Produces `core\Release\mss.lib`, `python\Release\pymss.cp310-win_amd64.pyd`,
`build\render\Release\render.exe`.

## Run

```powershell
# Train (PPO on GPU); networks saved to nn\
powershell -ExecutionPolicy Bypass -File scripts\train.ps1

# View reference motion
powershell -ExecutionPolicy Bypass -File scripts\view.ps1

# View a trained muscle policy
powershell -ExecutionPolicy Bypass -File scripts\view.ps1 -Nn ..\nn\max.pt -MuscleNn ..\nn\max_muscle.pt
```

Viewer keys: `space` simulate, `s` single step, `r` reset, `f` toggle focus,
`o` toggle OBJ meshes, `Esc` quit.

## Changes made for the Windows port

- All four `CMakeLists.txt` bumped to `cmake_minimum_required(VERSION 3.16)`, C++17,
  MSVC flag guards (`/bigobj /permissive-`, `NOMINMAX`, `_USE_MATH_DEFINES`), vcpkg targets
  (`dart`, `dart-gui`, `dart-collision-bullet`, `dart-utils`, `unofficial-tinyxml`,
  `OpenGL::GL`, `GLUT::GLUT`, `assimp::assimp`, `pybind11`), `pybind11_add_module` for `pymss`.
- `core/Muscle.cpp`: added `#include <numeric>`/`<algorithm>` (MSVC does not pull them transitively).
- `core/DARTHelper.cpp`: OBJ meshes loaded via `MeshShape::loadMesh(Uri::createFromPath(...), retriever)`
  so Windows drive letters (`D:`) are not mis-parsed as a URI scheme.
- `python/main.py`: `np.array(self.replay_buffer.buffer, dtype=object)` for NumPy 2.x.
