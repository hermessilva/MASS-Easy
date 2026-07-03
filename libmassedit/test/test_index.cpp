// Standalone unit test for libmassedit Index — no vcpkg/DART/JSON.
// Builds a tiny Model by hand and exercises ids, reverse indices, topology,
// generational staleness, and the group selector.
#include "../Index.h"
#include <cstdio>
#include <string>

using namespace mass;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_fail; } \
                              else { std::printf("ok  : %s\n", msg); } } while (0)

static Node bone(const std::string& id, const std::string& parent) {
    Node n; n.id = id; n.parent = parent; return n;
}
static Muscle muscle(const std::string& name, const std::string& side,
                     std::vector<std::string> bodies) {
    Muscle m; m.name = name; m.side = side;
    for (auto& b : bodies) { Waypoint w; w.body = b; m.waypoints.push_back(w); }
    return m;
}

int main() {
    Model m;
    // Pelvis -> FemurR -> TibiaR ; Pelvis -> FemurL -> TibiaL
    m.skeleton.push_back(bone("Pelvis", ""));
    m.skeleton.push_back(bone("FemurR", "Pelvis"));
    m.skeleton.push_back(bone("TibiaR", "FemurR"));
    m.skeleton.push_back(bone("FemurL", "Pelvis"));
    m.skeleton.push_back(bone("TibiaL", "FemurL"));

    // R_Rectus spans Pelvis->Femur->Tibia (crosses hip AND knee)
    m.muscles.push_back(muscle("R_Rectus_Femoris", "R", {"Pelvis", "FemurR", "TibiaR"}));
    // R_Vastus spans Femur->Tibia (crosses knee only)
    m.muscles.push_back(muscle("R_Vastus", "R", {"FemurR", "TibiaR"}));
    // L_Rectus spans left hip+knee
    m.muscles.push_back(muscle("L_Rectus_Femoris", "L", {"Pelvis", "FemurL", "TibiaL"}));

    Index ix;
    ix.build(m);

    // ---- name lookups ----
    CHECK(ix.boneSlot("FemurR") == 1, "boneSlot(FemurR)==1");
    CHECK(ix.muscleSlot("R_Vastus") == 1, "muscleSlot(R_Vastus)==1");
    CHECK(ix.boneSlot("Nope") == -1, "boneSlot(missing)==-1");

    // ---- reverse index: muscles anchoring to a bone ----
    CHECK(ix.musclesOfBone("FemurR").size() == 2, "FemurR touched by 2 muscles");
    CHECK(ix.musclesOfBone("Pelvis").size() == 2, "Pelvis touched by 2 muscles");
    CHECK(ix.waypointsOfBone("FemurR").size() == 2, "FemurR has 2 waypoints");

    // ---- crossing joint ----
    auto knee = ix.musclesCrossingJoint("TibiaR"); // parent FemurR
    CHECK(knee.size() == 2, "knee(TibiaR) crossed by 2 (Rectus+Vastus)");
    auto hip = ix.musclesCrossingJoint("FemurR");  // parent Pelvis
    CHECK(hip.size() == 1, "hip(FemurR) crossed by 1 (Rectus only)");

    // ---- topology ----
    CHECK(ix.parentOf(ix.boneSlot("TibiaR")) == ix.boneSlot("FemurR"), "parent(TibiaR)==FemurR");
    CHECK(ix.subtreeBones("Pelvis").size() == 5, "subtree(Pelvis)==5 bones");
    CHECK(ix.subtreeBones("FemurR").size() == 2, "subtree(FemurR)==2 bones");
    CHECK(ix.ancestorBones("TibiaR").size() == 2, "ancestors(TibiaR)==2");

    // ---- selector ----
    CHECK(ix.select("type=bone").size() == 5, "select type=bone ->5");
    CHECK(ix.select("side=R").size() == 4, "select side=R ->2 bones(FemurR,TibiaR)+2 muscles");
    CHECK(ix.select("subtree(FemurL)").size() == 2, "select subtree(FemurL) ->2");
    CHECK(ix.select("crossing(TibiaR)").size() == 2, "select crossing(TibiaR) ->2");
    CHECK(ix.select("side=R AND crossing(TibiaR)").size() == 2, "AND right knee muscles ->2");
    CHECK(ix.select("subtree(FemurR) OR subtree(FemurL)").size() == 4, "OR both thighs ->4");
    CHECK(ix.select("body=Pelvis").size() == 2, "select body=Pelvis ->2 muscles");

    // explicit group
    std::vector<EntityId> g{ ix.muscleByName("R_Vastus"), ix.muscleByName("L_Rectus_Femoris") };
    ix.addGroup("mygrp", g);
    CHECK(ix.select("group=mygrp").size() == 2, "select group=mygrp ->2");
    CHECK(ix.select("group=mygrp AND side=R").size() == 1, "group AND side=R ->1");

    // ---- generational staleness ----
    EntityId tibiaR = ix.boneByName("TibiaR");
    CHECK(ix.isValid(tibiaR), "TibiaR handle valid before edit");
    // rename slot 2 (TibiaR) -> different bone; rebuild must bump its generation
    m.skeleton[2].id = "TalusR";
    ix.build(m);
    CHECK(!ix.isValid(tibiaR), "old TibiaR handle stale after rename");
    CHECK(ix.isValid(ix.boneByName("TalusR")), "new TalusR handle valid");

    std::printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nALL CHECKS PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
