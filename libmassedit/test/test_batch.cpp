// Batch unit test: lengthen a "femur" and confirm the bone scales along its
// axis while the child (tibia/talus) subtree keeps its length and slides down,
// waypoints follow correctly, and the L/R symmetric variant scales both legs.
#include "../Batch.h"
#include <cstdio>
#include <cmath>
#include <string>

using namespace mass;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_fail; } \
                              else { std::printf("ok  : %s\n", msg); } } while (0)
static bool eq(double a, double b) { return std::fabs(a - b) < 1e-9; }
static bool eqv(const Vec3& p, double x, double y, double z) { return eq(p[0],x)&&eq(p[1],y)&&eq(p[2],z); }

static Node bone(const std::string& id, const std::string& parent, Vec3 body, Vec3 joint, Vec3 size) {
    Node n; n.id = id; n.parent = parent;
    n.body.type = "Box"; n.body.size = size;
    n.body.t.translation = body; n.joint.t.translation = joint;
    return n;
}
static void addWp(Model& m, int muscle, const std::string& body, Vec3 p) {
    Waypoint w; w.body = body; w.p = p; m.muscles[muscle].waypoints.push_back(w);
}

// Pelvis(hip@0,0,0) -> FemurR(body@0,-0.5,0, knee@0,-1,0) -> TibiaR(body@0,-1.5,0,
// ankle@0,-2,0) -> TalusR(body@0,-2,0). Optional left leg mirrors on +X? keep at 0.
static Model makeLeg(bool withLeft) {
    Model m;
    m.skeleton.push_back(bone("Pelvis", "",       {0,0,0},    {0,0,0},  {0.2,0.2,0.2}));
    m.skeleton.push_back(bone("FemurR", "Pelvis", {0,-0.5,0}, {0,0,0},  {0.1,1.0,0.1}));
    m.skeleton.push_back(bone("TibiaR", "FemurR", {0,-1.5,0}, {0,-1,0}, {0.1,1.0,0.1}));
    m.skeleton.push_back(bone("TalusR", "TibiaR", {0,-2,0},   {0,-2,0}, {0.1,0.1,0.1}));
    if (withLeft) {
        m.skeleton.push_back(bone("FemurL", "Pelvis", {0,-0.5,0}, {0,0,0},  {0.1,1.0,0.1}));
        m.skeleton.push_back(bone("TibiaL", "FemurL", {0,-1.5,0}, {0,-1,0}, {0.1,1.0,0.1}));
    }
    Muscle mu; mu.name = "M"; m.muscles.push_back(mu);
    addWp(m, 0, "FemurR", {0,-0.5,0});   // on the femur (scales)
    addWp(m, 0, "TibiaR", {0,-1.5,0});   // on the tibia (rigid follow)
    return m;
}

int main() {
    // ---- lengthen femur x2 along Y ----
    {
        Model m = makeLeg(false);
        Index ix; ix.build(m);
        bool ok = Batch::scaleBone(m, ix, "FemurR", Vec3{0,1,0}, 2.0);
        CHECK(ok, "scaleBone returns true");

        // femur body center doubled about hip: -0.5 -> -1.0
        CHECK(eqv(m.skeleton[1].body.t.translation, 0,-1.0,0), "femur center -> (0,-1,0)");
        // femur Box size along Y doubled: 1.0 -> 2.0
        CHECK(eq(m.skeleton[1].body.size[1], 2.0), "femur size.y -> 2.0");
        // knee (tibia joint) doubled: -1 -> -2
        CHECK(eqv(m.skeleton[2].joint.t.translation, 0,-2.0,0), "knee -> (0,-2,0)");
        // tibia keeps its length: body was -1.5, delta -1 -> -2.5 ; ankle -2 -> -3
        CHECK(eqv(m.skeleton[2].body.t.translation, 0,-2.5,0), "tibia body slides to (0,-2.5,0)");
        CHECK(eqv(m.skeleton[3].joint.t.translation, 0,-3.0,0), "ankle -> (0,-3,0)");
        CHECK(eq(m.skeleton[2].body.size[1], 1.0), "tibia size unchanged (rigid)");
        // waypoints: femur wp scales (-0.5 -> -1), tibia wp rigid (-1.5 -> -2.5)
        CHECK(eqv(m.muscles[0].waypoints[0].p, 0,-1.0,0), "femur waypoint scales -> (0,-1,0)");
        CHECK(eqv(m.muscles[0].waypoints[1].p, 0,-2.5,0), "tibia waypoint rigid -> (0,-2.5,0)");
    }

    // ---- translateSubtree ----
    {
        Model m = makeLeg(false);
        Index ix; ix.build(m);
        Batch::translateSubtree(m, ix, "FemurR", Vec3{0,0,1});
        CHECK(eqv(m.skeleton[1].body.t.translation, 0,-0.5,1), "subtree translated +Z");
        CHECK(eqv(m.skeleton[3].joint.t.translation, 0,-2,1), "talus joint translated +Z");
        CHECK(eqv(m.muscles[0].waypoints[1].p, 0,-1.5,1), "tibia waypoint translated +Z");
    }

    // ---- counterpart naming ----
    CHECK(Batch::counterpartName("FemurR") == "FemurL", "FemurR -> FemurL");
    CHECK(Batch::counterpartName("FemurL") == "FemurR", "FemurL -> FemurR");
    CHECK(Batch::counterpartName("R_Biceps") == "L_Biceps", "R_Biceps -> L_Biceps");
    CHECK(Batch::counterpartName("Pelvis") == "", "Pelvis has no side");

    // ---- symmetric scale hits both legs ----
    {
        Model m = makeLeg(true);
        Index ix; ix.build(m);
        int n = Batch::scaleBoneSymmetric(m, ix, "FemurR", Vec3{0,1,0}, 2.0);
        CHECK(n == 2, "symmetric scaled 2 bones");
        CHECK(eq(m.skeleton[ix.boneSlot("FemurR")].body.size[1], 2.0), "right femur scaled");
        CHECK(eq(m.skeleton[ix.boneSlot("FemurL")].body.size[1], 2.0), "left femur scaled");
        CHECK(eqv(m.skeleton[ix.boneSlot("TibiaL")].joint.t.translation, 0,-2.0,0), "left knee slid to -2");
    }

    std::printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nALL CHECKS PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
