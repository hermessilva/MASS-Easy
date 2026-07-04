# libmassedit

Shared model-editing library for the MASS **Arena** editor and the (planned) in-process
**MCP** bridge. Pure C++17 over the `mass::Model` struct — no DART, no Python. See the design
in [`Docs/MCP-Study.md`](../Docs/MCP-Study.md).

## Modules

| File | Role | Status |
|---|---|---|
| `MassModel.{h,cpp}` | `Model` struct + `.mass` JSON IO + stable `uid` migration (`assignUids`) | **done, tested** |
| `Index.{h,cpp}` | generational-handle ids, reverse indices, uid lookups, group selector | **done, tested** |
| `Query.{h,cpp}` | JSON facade for MCP tools (describe/getNode/getMuscle/select) | **done, tested** |
| `Kinematics.*` | FK: rotate joint, propagate subtree, re-anchor waypoints | **done, tested** |
| `DofMap.*` | anatomical DOF layer (name → joint/axis/sign/range) | **done, tested** |
| `Batch.*` | scale_bone (along-axis, subtree slide, waypoint re-anchor), translate_subtree, L/R symmetric | **done, tested** |
| `Complete.*` | finger/phalanx generation (Revolute flex chains), `list_gaps`, L/R symmetric | **done, tested** |
| `Atlas.*` | OpenSim `.osim` parse (tinyxml2), normalized join, `validate`, `sync` | **done, tested** |
| `Groom.*` | GroomParams (persisted) + `HairSim` PBD guide solver (dynamic tier) | **done, tested** |
| `Mcp.*` | MCP JSON-RPC dispatch (`McpServer`) + single-writer co-edit queue (`McpQueue`) | **done, tested** |

## Index (phase 1)

Replaces name-string scans with:
- **Generational handles** `EntityId{type,slot,gen}` — stale after rename/delete.
- O(1) `name → id`; reverse indices `bone → muscles`, `bone → waypoints`,
  `joint → crossing muscles`; skeleton `subtree`/`ancestors`.
- **Group selector** (OR of ANDs): `side=R | type=bone|muscle | body=BONE |
  subtree(BONE) | crossing(CHILDBONE) | group=NAME`, e.g.
  `side=R AND crossing(TibiaR)`.

## Build & test

### Visual Studio 2026 solution (native)
Open `MASS.slnx` (repo root) in VS 2026, or build from the command line:

```powershell
msbuild MASS.slnx /p:Configuration=Release /p:Platform=x64 /m
```

Standard VS layout — each project sits next to its sources:
`libmassedit/massedit.vcxproj` (static lib), `libmassedit/server/mass-mcp.vcxproj`,
`libmassedit/test/test_*.vcxproj`. Executables (`mass-mcp.exe`, `test_*.exe`) are emitted to
`Dist/x64/<Config>/` alongside `tinyxml2.dll` (same location as `build-dist.ps1`); the static
lib stays in its own `build/`.
`libmassedit/common.props` (imported by every project) wires the toolchain
(v145 / SDK 10.0.26100.0 / C++17) and the vcpkg `x64-windows` include+lib paths; exes link
`massedit` via `ProjectReference` and copy `tinyxml2.dll` post-build.

### CMake (used by CI/scripts)
The test suite also builds standalone via CMake (no vcpkg needed for the pure unit tests; the
integration tests use the repo's vcpkg `nlohmann/json` + `tinyxml2` if present):

```powershell
cmake -S libmassedit/test -B libmassedit/test/build -G "Visual Studio 18 2026" -A x64
cmake --build libmassedit/test/build --config Release
ctest --test-dir libmassedit/test/build -C Release --output-on-failure
```

Run the standalone MCP server:
```powershell
cmake -S libmassedit/server -B libmassedit/server/build -G "Visual Studio 18 2026" -A x64
cmake --build libmassedit/server/build --config Release
libmassedit/server/build/Release/mass-mcp.exe data/human.mass 8766
```

`test_index` uses a hand-built model; `test_model` builds the Index over `data/human.mass`
and verifies reference counts (Pelvis = 112 muscles, 23 bones, 284 muscles, Femur = 118
waypoints); `test_kinematics` checks FK on a synthetic arm (subtree + waypoints follow, parent
fixed, round-trip, proximal→distal chain order, DofMap value mapping); `test_batch` lengthens a
synthetic femur (bone scales along its axis; child subtree keeps length and slides; waypoints
follow; L/R symmetric scales both legs); `test_query` migrates uids + exercises the JSON facade
over real data; `test_complete` generates finger chains on a bare hand, flexes a finger (tip
moves — animatable), and checks `list_gaps` before/after + symmetric fill; `test_atlas` parses a
minimal `.osim`, joins by normalized name, and validates/syncs a model (origin/insertion,
f0 deviation, side inference); `test_groom` runs the PBD guide solver (root pinned, inextensible
segments settle to a vertical hang, wind deflects the tip) and checks GroomParams persistence;
`test_mcp` drives the MCP server over real data (initialize, tools/list, tools/call for
describe/scale_bone/generate_fingers/select, error handling) and the McpQueue single-writer
drain from many worker threads.

## MCP server & in-process co-edit

`McpServer::handle(request, model, index)` maps MCP JSON-RPC (`initialize`, `tools/list`,
`tools/call`) onto libmassedit ops. Tools: `describe_model`, `get_node`, `get_muscle`, `select`,
`muscles_of_body`, `muscles_crossing_joint`, `scale_bone`, `translate_subtree`, `rotate_joint`,
`generate_fingers`, `list_gaps`, `load_atlas`, `validate_anatomy`, `sync_from_atlas`, `save`,
`load`. Mutating tools rebuild the index.

`McpQueue` is the co-edit mechanism: any thread `submit`s a request and gets a `std::future`;
the owner thread (Arena's UI thread) calls `drain` once per frame to apply every request through
`McpServer` on that single thread — no model locks, no data races. The Asio transport that feeds
the queue is a thin wrapper (mirrors `Arena/src/TrainBridge`): a TCP server thread parses
JSON-RPC and calls `queue.submit`, then writes back the resolved future.

> Note: MSVC `<cmath>` resolves `<math.h>` case-insensitively — do **not** put `Arena/src`
> (which contains `Math.h`) on the include path, or the UCRT `math.h` gets shadowed. This is
> why `MassModel.h` lives here.
