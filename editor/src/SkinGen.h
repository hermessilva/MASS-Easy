#pragma once
// Procedural skin generation: smooth-union of per-body ellipsoids (a signed field)
// meshed with Marching Cubes into one continuous surface. Regenerated on demand.
#include "Math.h"
#include "MassModel.h"
#include <vector>

namespace mass {

struct SkinParams {
    float cell = 0.02f;      // marching-cubes cell size (m) — ~2cm default (fast to edit)
    float inflate = 0.015f;  // skin thickness added outside the anatomy (m)
    float smooth = 0.06f;    // smooth-union blend radius (m) — higher = smoother fusion
    float bodyScale = 1.15f; // per-body ellipsoid inflation over the box half-extents
};

// Build a continuous skin mesh (triangle soup: pos + per-vertex normals) from the
// model's bodies in their current (rest) transforms.
void GenerateSkin(const Model& m, const SkinParams& p,
                  std::vector<V3>& outPos, std::vector<V3>& outNrm);

} // namespace mass
