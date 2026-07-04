# Changelog

Notable changes to MASS-Easy. This log starts at the beginning of the **MCP /
`libmassedit`** work (the "model control" effort); earlier history is in
`git log`.

Format loosely follows [Keep a Changelog](https://keepachangelog.com/). Dates are
ISO (YYYY-MM-DD).

## [Unreleased]

### Added — Visual Studio 2026 solution (fully native, PascalCase)
- Native VS 2026 solution `MASS.slnx` at the **repo root**. Every project is
  hand-authored, PascalCase, and committed next to its sources (no reliance on
  CMake-generated files under the git-ignored `build/`). Solution folders:
  - **App/** — `core/Mss.vcxproj` (static lib), `Arena/Arena.vcxproj`,
    `render/Render.vcxproj`, `python/Pymss.vcxproj`. The heavy
    DART/Bullet/GLFW/GLUT/assimp/Python targets: dependency resolution is
    delegated to **vcpkg MSBuild auto-link** via `Legacy.props` (imports vcpkg's
    `vcpkg.props`/`vcpkg.targets`), so DART & friends link automatically and the
    runtime DLLs are copied by vcpkg's applocal step. `Legacy.props` also carries
    the CMake-parity recipe (`MASS_ROOT_DIR`, `DART_HAVE_spdlog=0`,
    `DART_ACTIVE_LOG_LEVEL=2`, `FMT_HEADER_ONLY=1`, `TINYXML2_IMPORT`,
    `_USE_MATH_DEFINES`, `/bigobj /utf-8`, eigen3/bullet include dirs). Outputs
    keep their legacy names (`arena.exe`, `render.exe`, `mss.lib`,
    `pymss.cp310-win_amd64.pyd`).
  - **MCP/** — `libmassedit/MassEdit.vcxproj` (static lib),
    `libmassedit/server/MassMcp.vcxproj`.
  - **MCP/tests/** — `libmassedit/test/Test*.vcxproj` (TestIndex, TestKinematics,
    TestBatch, TestModel, TestQuery, TestComplete, TestAtlas, TestGroom, TestMcp).
- `libmassedit/common.props` centralizes the light-target toolchain (v145 / SDK
  10.0.26100.0 / C++17, vcpkg include/lib, `tinyxml2.dll` copy); exes link
  `MassEdit` via `ProjectReference`.
- Executables/modules go to `Dist/x64/<Config>/` (same as `build-dist.ps1`);
  static libs stay in their own `build/`.
- Verified: the whole solution builds via MSBuild in VS 2026 (all 15 projects:
  `mss`/`arena`/`render`/`pymss` + `MassEdit`/`MassMcp` + 9 tests); the nine test
  binaries pass and `MassMcp` answers over TCP from `Dist`.

### The `libmassedit` library (new)
A shared C++17 library over the `mass::Model` struct, used by the MCP server and
(planned) the in-process Arena bridge. No DART, no Python. Nine ctest suites,
green on MSVC 14.51, several running over the real `data/human.mass`.

- **Relational index** (`Index`): generational-handle `EntityId`, O(1) name/uid
  lookups, reverse indices (`bone→muscles`, `bone→waypoints`,
  `joint→crossing muscles`), skeleton topology, and a group selector
  mini-language (`side=R AND crossing(TibiaR)`, `subtree(FemurL)`, …).
- **Stable ids** (`MassModel::assignUids`): persisted `uid` on nodes/muscles with
  backward-compatible migration of legacy `.mass`.
- **Query facade** (`Query`): JSON `describe_model` / `get_node` / `get_muscle` /
  `select` for the MCP tool layer.
- **Forward kinematics** (`Kinematics`, `DofMap`): rotate a joint, rigidly move
  its bone subtree, re-anchor muscle waypoints; anatomical DOFs
  (`elbow_R.flex`, …).
- **Batch edits** (`Batch`): `scale_bone` (the "lengthen the femur / shorten the
  fingers" edit — bone scales along its axis, child subtree keeps length and
  slides, waypoints re-anchored), `translate_subtree`, L/R symmetric mirror.
- **Completion** (`Complete`): generate finger phalanx chains (Revolute flex) so
  a bare hand can articulate — enabling "open/close the right hand" — plus
  `list_gaps` and symmetric fill.
- **Anatomy atlas** (`Atlas`): parse an OpenSim `.osim` (tinyxml2), join by a
  normalized name, `validate` the model (origin/insertion, f0 deviation) and
  `sync` empty metadata / Hill params from it.
- **Grooms** (`Groom`): persisted `GroomParams` (the handful of authored controls
  that drive procedural hair) plus `HairSim`, a Position-Based-Dynamics guide
  solver (root pinned, gravity + wind, inextensible segments, damping).
- **MCP server** (`Mcp`): `McpServer` maps MCP JSON-RPC (`initialize`,
  `tools/list`, `tools/call`) onto the above (16 tools); `McpQueue` is the
  single-writer co-edit queue (submit from any thread, drain on the owner thread).
- **Standalone server** (`libmassedit/server/mass-mcp`): a runnable MCP server
  over TCP (newline-delimited JSON-RPC, Boost.Asio). Smoke-tested end to end.

### Docs
- `Docs/MCP-Study.md`: the design study (co-edit architecture, three storage
  tiers, generational-id index, FK, atlas, grooms, the 100 MP offline-render
  target) plus an implementation-status section (§13).

### Known limitations / remaining
- The MCP is **not yet hosted inside `arena.exe`**, so Arena and the MCP cannot
  edit the *same* live model concurrently. `mass-mcp` runs as a separate process
  with its own in-memory model. Live co-edit needs: extract `MassModel` into
  `libmassedit` (Arena has its own copy → ODR clash), add an Asio `McpBridge`
  mirroring `TrainBridge`, and drain `McpQueue` in `App::frame()`.

## Commit history (MCP / libmassedit effort) — 2026-07-03
- `e10d8c8` feat(mass-mcp): standalone MCP server over TCP + implementation status
- `2e49dff` feat(libmassedit): phase 4a — MCP server core + single-writer co-edit queue
- `a0ec9e0` feat(libmassedit): phase 7 — groom tiers (PBD guide solver) + persistence
- `02ca6fa` feat(libmassedit): phase 6 — OpenSim .osim atlas parse + validate + sync
- `e7d4e99` feat(libmassedit): phase 5 — structural completion (fingers) + list_gaps
- `9900123` feat(libmassedit): phase 1b — stable uids + JSON query facade
- `f079576` feat(libmassedit): phase 3 — batch scale_bone + subtree slide + L/R mirror
- `8ed309e` feat(libmassedit): phase 2 — FK Kinematics + anatomical DofMap
- `f2ccacc` feat(libmassedit): phase 1 — relational Index (ids, reverse indices, selector)
- `5af23e7` docs(mcp): initial design study for in-process model-control MCP
