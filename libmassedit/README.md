# libmassedit

Shared model-editing library for the MASS **Arena** editor and the (planned) in-process
**MCP** bridge. Pure C++17 over the `mass::Model` struct — no DART, no Python. See the design
in [`Docs/MCP-Study.md`](../Docs/MCP-Study.md).

## Modules

| File | Role | Status |
|---|---|---|
| `MassModel.{h,cpp}` | `Model` struct + `.mass` JSON IO (canonical home; Arena will consume this) | present |
| `Index.{h,cpp}` | generational-handle ids, reverse indices, group selector | **done, tested** |
| `Kinematics.*` | FK: rotate joint, propagate subtree, re-anchor waypoints | **done, tested** |
| `DofMap.*` | anatomical DOF layer (name → joint/axis/sign/range) | **done, tested** |
| `Batch.*` | scale_bone (along-axis, subtree slide, waypoint re-anchor), translate_subtree, L/R symmetric | **done, tested** |
| `Complete.*` | joint/bone templates, `list_gaps` | planned |
| `Atlas.*` | OpenSim `.osim` parse + join | planned |
| `Validate.*` | internal + atlas checks | planned |

## Index (phase 1)

Replaces name-string scans with:
- **Generational handles** `EntityId{type,slot,gen}` — stale after rename/delete.
- O(1) `name → id`; reverse indices `bone → muscles`, `bone → waypoints`,
  `joint → crossing muscles`; skeleton `subtree`/`ancestors`.
- **Group selector** (OR of ANDs): `side=R | type=bone|muscle | body=BONE |
  subtree(BONE) | crossing(CHILDBONE) | group=NAME`, e.g.
  `side=R AND crossing(TibiaR)`.

## Build & test

Tests are standalone (no vcpkg needed for the unit test; the integration test uses the repo's
vcpkg `nlohmann/json` include if present):

```powershell
cmake -S libmassedit/test -B libmassedit/test/build -G "Visual Studio 18 2026" -A x64
cmake --build libmassedit/test/build --config Release
ctest --test-dir libmassedit/test/build -C Release --output-on-failure
```

`test_index` uses a hand-built model; `test_model` builds the Index over `data/human.mass`
and verifies reference counts (Pelvis = 112 muscles, 23 bones, 284 muscles, Femur = 118
waypoints); `test_kinematics` checks FK on a synthetic arm (subtree + waypoints follow, parent
fixed, round-trip, proximal→distal chain order, DofMap value mapping); `test_batch` lengthens a
synthetic femur (bone scales along its axis; child subtree keeps length and slides; waypoints
follow; L/R symmetric scales both legs).

> Note: MSVC `<cmath>` resolves `<math.h>` case-insensitively — do **not** put `Arena/src`
> (which contains `Math.h`) on the include path, or the UCRT `math.h` gets shadowed. This is
> why `MassModel.h` lives here.
