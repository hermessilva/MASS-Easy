// Integration test: build the Index over the real data/human.mass and confirm
// the reverse-index counts match the reference scan (Pelvis=112 muscles, 23 bones,
// 284 muscles, 1212 waypoints).
#include "../Index.h"
#include "../MassModel.h"
#include <cstdio>
#include <string>

using namespace mass;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_fail; } \
                              else { std::printf("ok  : %s\n", msg); } } while (0)

int main(int argc, char** argv) {
    std::string path = (argc > 1) ? argv[1] : "../../../../data/human.mass";
    std::string err;
    auto m = Model::LoadMass(path, &err);
    if (!m) { std::printf("cannot load %s: %s\n", path.c_str(), err.c_str()); return 2; }

    std::printf("loaded: %zu bones, %zu muscles\n", m->skeleton.size(), m->muscles.size());
    CHECK(m->skeleton.size() == 23, "23 bones");
    CHECK(m->muscles.size() == 284, "284 muscles");

    Index ix;
    ix.build(*m);

    // reference counts from the Python scan of human.mass
    CHECK(ix.musclesOfBone("Pelvis").size() == 112, "Pelvis touched by 112 muscles");
    CHECK(ix.musclesOfBone("FemurR").size() > 0, "FemurR has muscles");
    CHECK(ix.waypointsOfBone("FemurR").size() == 118, "FemurR has 118 waypoints");
    CHECK(ix.waypointsOfBone("FemurL").size() == 118, "FemurL has 118 waypoints");

    // topology sanity: Pelvis is the root of the leg chains
    CHECK(ix.boneSlot("Pelvis") >= 0, "Pelvis present");
    CHECK(ix.parentOf(ix.boneSlot("TibiaR")) == ix.boneSlot("FemurR"), "TibiaR parent FemurR");
    CHECK(ix.subtreeBones("Pelvis").size() == 23, "subtree(Pelvis) == whole skeleton (23)");

    // muscles crossing the right knee should be a non-trivial subset
    auto knee = ix.musclesCrossingJoint("TibiaR");
    std::printf("muscles crossing right knee: %zu\n", knee.size());
    CHECK(knee.size() > 0 && knee.size() < m->muscles.size(), "knee crossers is a proper subset");

    // selector on real data
    auto rightSide = ix.select("side=R");
    std::printf("side=R entities: %zu\n", rightSide.size());
    CHECK(rightSide.size() > 0, "side=R non-empty on real data");

    std::printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nALL CHECKS PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
