# MCP-Study — In-process Model Control for Arena

Initial engineering study for an **MCP (Model Context Protocol) server embedded in Arena**
that lets an AI agent and a human co-edit the same live musculoskeletal model, complete
missing anatomy, animate parts to validate the rig, and validate against an anatomy atlas.

> Status: **design + implementation in progress**. The `libmassedit` library and a runnable
> standalone `mass-mcp` server are built and tested; see §13 for what is done and what remains.

---

## 1. Goals

1. **Manipulate** the model easily and intuitively (single element and in bulk).
2. **Complete** missing structure (e.g. finger joints, extra vertebrae).
3. **Animate parts** like a real body — "open/close right hand", "raise left arm". These
   motion commands are the **test** that validates the bone/muscle rig.
4. **Validate** muscles / bones / (future) organs against an **anatomy atlas** (external
   ground truth), not only internal consistency.
5. **Co-edit**: the MCP runs *inside* `arena.exe` (not a separate process); AI and human
   mutate the same live model and both see changes immediately.

Non-goal: real-time photoreal rendering. The viewport is a **real-time proxy** (wireframe /
simple shaded, hair as a coarse mass). The photoreal frame is produced **offline** by a path
tracer from the same data (see §9).

---

## 2. The model today

Single project file `data/human.mass` (JSON, ~17k lines), loaded into the C++ `Model` struct
(`Arena/src/MassModel.*`). Current census:

| Collection | Count | Notes |
|---|---|---|
| `skeleton` (Node = Body+Joint) | 23 | tree via `Node.parent` |
| `muscles` (Hill + waypoints) | 284 | names are OpenSim-standard |
| waypoints total | 1212 | `Waypoint.body` is the only bone↔muscle link |
| motions | 1 | BVH |

### 2.1 How pieces link today
- **Bone ↔ bone**: `Node.parent` + `joint` → a tree (Pelvis is the limb root).
- **Bone ↔ muscle**: *only* via `Waypoint.body` (a body id string per waypoint). There is
  **no stored reverse reference**. To find muscles on the Pelvis you scan all 284 muscles.
  (Pelvis: 112 distinct muscles touch it; 104 as origin, 2 as insertion.)
- A muscle's trajectory is its ordered waypoint list; first waypoint body = origin, last =
  insertion, middle = via-points (wrapping). Origin/insertion are **inferred by position**,
  not explicit (`AtlasEntry` has explicit `origin_body`/`insertion_body`, but `Model` does
  not use them).

### 2.2 Limitations to fix
- Linking is by **name string** → rename breaks refs; `findNode`/`findMuscle` are O(n) scans.
- No reverse index (`body → muscles`, `joint → crossing muscles`).
- `anatomy.group` metadata is **empty** → cannot tell which muscles actuate a joint.
- No explicit origin/insertion; no via-point vs anchor distinction.

---

## 3. Two representations, one source of truth

The `.mass` project + in-memory `Model`/indices is the **single source of truth**. Two
consumers read it:

| | Real-time (Arena viewport) | Offline (path tracer) |
|---|---|---|
| Bone/muscle/organ | wireframe / simple shaded (exists) | mesh + SSS + displacement |
| Hair/fur | **coarse mass** (low-poly shell / guide ribbons) | expand guides → real curves |
| Cost | 60 fps authoring | minutes/frame on a farm |
| Data | `.mass` + groom **params** | same params, procedurally expanded |

Editing manipulates params/guides; rendering expands them. Nothing changes source.

---

## 4. Storage in three tiers

The full-detail human body is **not** one uniform object set. Do not conflate the tiers.

| Tier | Example | Volume | Engine | State |
|---|---|---|---|---|
| **Semantic (relational)** | bones, joints, muscles, organs, named vessels | 10³–10⁵ | relational index + FK | stateless |
| **Instanced static** | short body hair, goosebumps, pores | ~10⁶ | groom + surface mask + shader | stateless |
| **Instanced dynamic** | long hair, cloth | 10⁶ render / 10² sim | PBD/DER solver on guides | **stateful (velocity)** |

Full-body census (order of magnitude): ~206 bones, ~360 joints, ~640 muscles, ~79 major
organs, ~1–3k named vessels/nerves → **semantic ≈ 10⁴** (up to 10⁵ with branching trees).
Hair: scalp ~100–150k, **whole-body follicles ~5,000,000** → but authored via **~10²–10³
controls** (guides + density/direction maps + groom params).

**Key principle across all tiers: a few authored controls drive millions of primitives.**
Hair is a particle/procedural system, never relational rows. No SQL database — the volume
driver (hair) never enters the relational index. See §4.1–4.3.

### 4.1 Why not a SQL database
Semantic model fits in L2 cache; in-memory map queries are nanoseconds. A SQL *server* solves
a problem we don't have, adds an external process, and fights the per-frame single-writer +
undo-snapshot authoring model (§7). **Verdict:** in-memory relational index is the store;
an **optional embedded SQLite read-only mirror** (rebuilt from `Model`) can serve ad-hoc
grouping/atlas-join reports in a later phase — never the write path.

### 4.2 Static instanced hair (e.g. "goosebumps on the arms")
Hairs *are* linked to the arm — but once, at groom↔surface↔bone level, not 5M times:
```
Bone(ArmR) --skinning--> skin patch (region "arm") --> groom bound to surface --> strands
```
Follicles are scattered on the skin mesh → inherit the skin's skinning to ArmR for free.
"Which arm does strand N belong to" is **derived** from root position, never stored.
Piloerection = set one region parameter `erection` masked by an "arm" weightmap; a procedural
shader rotates every strand toward its own surface normal using the per-follicle local frame
(root pos + normal + direction) already in the groom's SoA arrays. One float drives all;
the surface mask replaces 5M foreign keys.

### 4.3 Dynamic instanced hair (long hair moving in wind/gravity)
Physics runs on **~100–300 guide curves**, not the millions; render strands interpolate from
guides. Per guide: a particle chain solved with **PBD or DER** (Discrete Elastic Rods):
- forces: gravity + turbulent wind field (curl-noise + gusts) + root inertia (bone motion);
- constraints: inextensibility, bending stiffness, collision vs the body capsules/boxes that
  already exist in `Model`, root pinned to the scalp (bound to `Head` bone);
- damping.
State (velocity) lives in the solver on a worker thread (like `SimBridge`), **not** in
`.mass`. `.mass` stores only groom params (length, stiffness, damping, density, guide count).
Cost: ~200 guides × ~30 segments ≈ 6k particles → trivially real-time.

---

## 5. Linking by unique ID + indices

Replace name-string links with **generational handles** (ECS pattern):
```cpp
struct EntityId { uint16 type; uint24 slot; uint8 gen; };  // stable, 6 bytes
// `name` becomes a display label, not the key
```
- `gen` detects stale refs after delete → never points at garbage.
- `slot` indexes a dense array → O(1), cache-friendly.
- rename touches only the label; id-based refs stay intact.

**Reverse indices** (built once on load, maintained incrementally on edit):
- `name → id` per type (kills the O(n) scan);
- `bone_id → [muscle_ids]` (Pelvis's 112 instantly);
- `bone_id → [waypoint refs]` (re-anchor on pose);
- `joint_id → [crossing muscles]` (needed by motion commands to find agonist/antagonist).

**Groups** (the core of bulk manipulation), mixed:
- **computed** (always correct): `side` from R/L suffix, `subtree(Pelvis)` from tree,
  `crossing(knee)` from indices;
- **explicit** (persisted): `hip_flexor`, anatomical region, user-defined.

**Selector mini-language** (the "SQL feel" without SQL) compiles to an id set that every
bulk op consumes:
```
side=R AND group=hip_flexor
subtree(FemurL)
crossing(ElbowR)
```

### 5.1 Persistence / migration
Add a stable `uid` field per entity in `.mass`; `name` becomes a label; relations serialize
by `uid`. Loading an old file without `uid` assigns them and resolves name-links → uids
(backward compatible).

---

## 6. FK (pose) vs index — distinct, used together

**FK ≠ the index.**
- **FK** = pose math on the **bone tree** (`Node.parent`). Input: a joint angle. Output: new
  world transforms of the subtree bodies. Muscles are not involved.
- **Index** = reverse map `body → muscles/waypoints`, used *after* FK to know which waypoints
  follow the moved bone.

Move-a-bone pipeline:
```
1. FK propagates bones (parent tree)            # kinematics
2. index[bone] → waypoints on that bone         # the reverse index
3. re-anchor those waypoints (world<->local)    # muscle follows bone
```
A "move left arm" command uses both: FK rotates shoulder/elbow, the index drags those bones'
muscle waypoints along.

### 6.1 Pose engine choice — FK in C++, in-process **(default)**
| | FK-native (in-process) | DART live (via SimBridge) |
|---|---|---|
| single pose | µs–ms | ms + rebuild concerns |
| **bulk edit** (scale femur → ~500 waypoints) | in-memory, fast | **worse** — structural change forces DART skeleton rebuild |
| **complete bone** (add phalanx) | array edit, instant | **worse** — full DART rebuild + xml re-export |
| physical fidelity | kinematic only (clamp limits) | **better** — collision, dynamics |
| robustness | one process, deterministic | worse — needs live Arena, IPC, thread sync |

The bottleneck of DART-live is not pose but the **skeleton rebuild per structural edit**.
Use FK-native for the fast authoring/validate loop; keep DART as an **optional** on-demand
dynamic check.

### 6.2 Anatomical DOF map (the intuitive layer)
Joints are raw (`ForeArmL` Revolute axis `[0,1,0]` limit `-2.3..0`). Users speak anatomy. A
DOF map bridges natural-language commands to joint math:

| Intuitive term | → (joint, axis, sign, range) |
|---|---|
| `elbow_L.flex 0..1` | ForeArmL, Y, −, [0 → −2.3] |
| `knee_R.flex` | TibiaR, X, +, [0 → 2.3] |
| `shoulder_L.flex/abduct/rotate` | ArmL Ball, 3 decomposed DOFs |
| `hand_R.curl 0..1` | all finger joints → flexion (after completion) |

Named groups (`arm_L`, `hand_R.fingers`, `leg_R`) apply a DOF across a chain. Command:
`pose(elbow_L=0.8, shoulder_L.abduct=0.5)` or natural language mapped to it.

---

## 7. In-process co-edit architecture

The MCP is a thread inside `arena.exe`. Pattern mirrors the existing `TrainBridge` (Boost.Asio
TCP server on a worker thread, JSON, marshaled to the UI via a mutex).

**Transport:** `McpBridge` hosts an MCP JSON-RPC endpoint (Streamable HTTP/SSE) on a port
(e.g. 8766). The external AI client connects by URL. No extra process, no stdio, no Python.

**Concurrency = single-writer (UI thread).** Only the UI thread writes `mModel`.
```
Asio thread (MCP)                UI thread (human + applies AI)
  receive tools/call        →    App::frame():
  mutating? enqueue Command        1. drain command queue BEFORE drawing
  wait (condvar)                   2. per cmd: snapshot() undo → libmassedit applies to mModel
                            ←      3. signal condvar → Asio replies with result
  read-only (describe/             4. draw mModel (human already sees the AI edit)
   validate) → snapshot
```
- Human edits (gizmo/panels) and AI edits (drained queue) both run on the UI thread,
  serialized per frame → **no model locks, no data races**.
- Both see each other's edits the next frame; the viewport is the shared source.
- AI addresses elements by id/name, human by click → selections don't collide. Same-frame
  double-write on one element = last-writer-wins, no corruption.
- **Undo is shared** (`mUndo`/`mRedo` already exist). Optionally tag command origin later.

IPC to a *separate* process (or DART live) is a later, optional phase — not needed here.

---

## 8. `libmassedit` — shared library

Extract `MassModel` out of `Arena/src/` into a new static lib used by **both** Arena and the
MCP bridge, guaranteeing zero drift.

```
libmassedit/
  MassModel.*    Model struct + .mass IO (moved from Arena)
  Index.*        generational handles + reverse indices + groups + selector
  Kinematics.*   FK: rotate joint, propagate subtree, re-anchor waypoints
  Batch.*        scale_bone, transform_subtree, mirror
  Complete.*     templates (finger/vertebra/toe), list_gaps
  DofMap.*       anatomical DOF layer
  Atlas.*        parse .osim (tinyxml), join by name
  Validate.*     internal + atlas checks
  Groom.*        hair/fur params, guides, static/dynamic tiers (later phase)
```
Validation round-trip links the `mss` core (C++) and loads exported xml via
`Character`/`DARTHelper` — **replaces pymss, no Python**. DART is an optional heavy target
for dynamic checks only.

Everything is native C++: `nlohmann/json`, `tinyxml`, `boost_asio` are already in the build.

---

## 9. The 100 MP offline frame (why the split matters)

Target shot: 200 mm f/2.8 on a 100 MP medium-format sensor (~11648×8736, 44×33 mm), face
filling the frame.
- Magnification m ≈ 0.165, distance ≈ 1.4 m, **DoF ≈ 2–5 mm** (razor-thin plane), skin
  sampling **~23 µm/pixel**.
- Terminal hair ~70 µm → ~3 px (individually resolved); eyelashes ~4–5 px; vellus ~1–2 px;
  pores ~2–4 px. The frame contains **10⁵–10⁶ hairs**, most sub-pixel but forming the rim-lit
  peach-fuzz halo that sells real skin.

This forces: every visible strand = **real curve geometry** (not cards), vellus as a real
groom, sub-pixel AA (path-traced, many samples), **lens-simulated DoF/bokeh** (not post
z-blur — thin semi-occluding blurred strands over a sharp eye need correct order-independent
transparency), Marschner hair shading + deep-opacity self-shadow, skin SSS + pore
displacement. This is **offline path tracing** (minutes–hours/frame), fed by expanding the
same groom params/guides used as a coarse mass in the real-time viewport. Stray strands over
the eye = **hero/adaptive guides** (dozens) simulated finely; the rest interpolate.

The architecture reaches this scene without any hand-placed geometry or database: **few
authored controls → procedural expansion to real geometry → physical path tracer.**

---

## 10. MCP tools (first cut)

- **Query**: `describe_model`, `get_node`, `get_muscle`, `select(selector)`,
  `muscles_of_body`, `muscles_crossing_joint`.
- **Manipulate**: `scale_bone`, `transform_subtree`, `mirror_edit`, `set_body_props`,
  `set_muscle_props`, `move_waypoints`.
- **Complete**: `list_gaps`, `complete_joint(template)`, `add_dof`.
- **Animate**: `pose(dofs)`, `named_pose(open_hand|fist|wave|...)`, `reset_pose`,
  `dofmap_query`.
- **Validate**: `validate_motion` (internal: limits, muscle length plausibility, no gross
  self-intersection, antagonist coherence), `validate_anatomy` (atlas: origin/insertion on
  correct bone, joints crossed, physiological f0/PCSA/pennation, symmetry, proportions),
  `sync_from_atlas(fields, dry_run)` (fill empty metadata), `validate_export` (native mss
  round-trip).
- **Groom** (later): `set_groom_params`, `wind`, `hero_region`.
- **IO**: `load`, `save`, `diff`, `undo`, `redo`.

---

## 11. Roadmap

1. `libmassedit` extraction + `.mass` IO + generational ids + reverse indices + selector;
   `describe_model`/query tools.
2. FK `Kinematics` + subtree propagation + waypoint re-anchor; DOF map; `pose`.
3. Batch: `scale_bone`, `transform_subtree`, `mirror_edit`.
4. `McpBridge` (Asio + MCP JSON-RPC + command queue) wired into `App::frame()`; co-edit live.
5. Completion (finger/vertebra templates) + `list_gaps`; enables "close right hand".
6. Atlas import (.osim) + `validate_anatomy` + `sync_from_atlas`; native `validate_export`.
7. Groom tiers (static mask hair, dynamic guide sim) + offline-render export.
8. Optional: DART dynamic-check target; SQLite read-only report mirror; IPC.

---

## 12. Open decisions (recommended defaults)

1. Pose engine: **FK-native in-process (default)** vs DART live.
2. Interface: **in-process co-edit via UI-thread command queue (default)** vs cross-process IPC.
3. Muscle atlas source: **OpenSim gait2392 (default)** — names already match — vs Rajagopal/other.
4. Organs in phase 1: **out of scope (default)** — model has none yet.
5. Generate hand phalanges in phase 1: **yes (default)** — it is the "close right hand" test case.

---

## 13. Implementation status

Built and unit/integration-tested in `libmassedit/` (9 ctest suites green, MSVC 14.51; the
real-data suites run over `data/human.mass`):

| Area | Module | Status |
|---|---|---|
| Relational index (ids, reverse lookups, selector) | `Index` | done |
| Stable uids + migration | `MassModel::assignUids` | done |
| JSON query facade | `Query` | done |
| FK (rotate joint, subtree, re-anchor) + DOF map | `Kinematics`, `DofMap` | done |
| Batch scale_bone / translate / L-R mirror | `Batch` | done |
| Completion (finger chains) + list_gaps | `Complete` | done |
| Atlas (.osim) parse + validate + sync | `Atlas` | done |
| Groom params + PBD guide solver | `Groom` | done |
| MCP JSON-RPC dispatch + co-edit queue | `Mcp` (`McpServer`, `McpQueue`) | done |
| **Standalone server** (TCP, newline JSON-RPC) | `libmassedit/server/mass-mcp` | **built + smoke-tested** |

Run the standalone server (verified: `initialize`, `describe_model`→23/284,
`muscles_of_body Pelvis`→112):
```powershell
cmake -S libmassedit/server -B libmassedit/server/build -G "Visual Studio 18 2026" -A x64
cmake --build libmassedit/server/build --config Release
libmassedit/server/build/Release/mass-mcp.exe data/human.mass 8766
```

### Remaining: in-process hosting inside `arena.exe`
The co-edit *mechanism* (`McpQueue`, single-writer drain) and *transport* (mass-mcp) exist and
are tested. Hosting it inside the running editor still requires, in order:
1. **Extract `MassModel` into `libmassedit`** and make Arena consume it (Arena currently has its
   own copy → two `mass::Model` definitions would be an ODR violation if linked together). This
   is a refactor of `Arena/src` and needs a full Arena rebuild (DART/vcpkg).
2. Add `McpBridge` mirroring `TrainBridge` (Asio TCP thread) that calls `McpQueue::submit` and
   writes back the resolved future.
3. In `App::frame()`, before drawing, call `queue.drain(server, mModel, mIndex)` and
   `snapshot()` per applied mutation so human + AI edits share one undo stack.

Deferred by design: `pose(named)` presets, `validate_motion`, DART dynamic-check target,
SQLite report mirror, groom render-expansion export.

## References
- Original MASS (SIGGRAPH 2019): https://github.com/lsw9021/MASS
- Existing bridges to mirror: `Arena/src/TrainBridge.*`, `Arena/src/SimBridge.*`.
- Model struct + IO: `Arena/src/MassModel.*`. Legacy xml round-trip: `Arena/src/Bootstrap.*`,
  `core/DARTHelper.cpp`, `core/Character.cpp`.
