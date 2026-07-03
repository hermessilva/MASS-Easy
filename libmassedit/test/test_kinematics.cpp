// FK unit test: rotate a joint, verify the subtree + muscle waypoints follow
// rigidly, the parent stays put, and rotations round-trip. Deterministic coords.
#define _USE_MATH_DEFINES
#include "../Kinematics.h"
#include "../DofMap.h"
#include <cstdio>
#include <cmath>
#include <string>

using namespace mass;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_fail; } \
                              else { std::printf("ok  : %s\n", msg); } } while (0)

static bool eq(double a, double b) { return std::fabs(a - b) < 1e-9; }
static bool eqv(const Vec3& p, double x, double y, double z) { return eq(p[0],x)&&eq(p[1],y)&&eq(p[2],z); }

static Node bone(const std::string& id, const std::string& parent, Vec3 bodyT, Vec3 jointT) {
    Node n; n.id = id; n.parent = parent;
    n.body.t.translation = bodyT;
    n.joint.t.translation = jointT;
    return n;
}

static Model makeArm() {
    Model m;
    // Torso(root) -> ArmR(shoulder@0,0,0) -> ForeArmR(elbow@2,0,0) -> HandR(wrist@3,0,0)
    m.skeleton.push_back(bone("Torso",   "",        {0,0,0}, {0,0,0}));
    m.skeleton.push_back(bone("ArmR",    "Torso",   {1,0,0}, {0,0,0}));
    m.skeleton.push_back(bone("ForeArmR","ArmR",    {2.5,0,0}, {2,0,0}));
    m.skeleton.push_back(bone("HandR",   "ForeArmR",{3.5,0,0}, {3,0,0}));
    // one muscle with a waypoint anchored to the forearm at (2.5,0,0)
    Muscle mu; mu.name = "M";
    Waypoint w; w.body = "ForeArmR"; w.p = {2.5,0,0}; mu.waypoints.push_back(w);
    m.muscles.push_back(mu);
    return m;
}

int main() {
    // ---- primitive: rotate elbow +90deg about Z ----
    {
        Model m = makeArm();
        Index ix; ix.build(m);
        bool ok = Kinematics::rotateJoint(m, ix, "ForeArmR", Vec3{0,0,1}, M_PI/2);
        CHECK(ok, "rotateJoint returns true");

        const Vec3& fore = m.skeleton[2].body.t.translation;
        const Vec3& hand = m.skeleton[3].body.t.translation;
        const Vec3& arm  = m.skeleton[1].body.t.translation;
        const Vec3& wp   = m.muscles[0].waypoints[0].p;

        CHECK(eqv(fore, 2, 0.5, 0), "forearm body -> (2,0.5,0)");
        CHECK(eqv(hand, 2, 1.5, 0), "hand body -> (2,1.5,0)");
        CHECK(eqv(wp,   2, 0.5, 0), "forearm waypoint follows -> (2,0.5,0)");
        CHECK(eqv(arm,  1, 0,   0), "arm (parent) unchanged");
        // elbow pivot itself unchanged
        CHECK(eqv(m.skeleton[2].joint.t.translation, 2, 0, 0), "elbow pivot unchanged");
        // hand's wrist joint (3,0,0) -> (2,1,0)
        CHECK(eqv(m.skeleton[3].joint.t.translation, 2, 1, 0), "wrist joint -> (2,1,0)");
    }

    // ---- round-trip: +90 then -90 restores ----
    {
        Model m = makeArm();
        Index ix; ix.build(m);
        Kinematics::rotateJoint(m, ix, "ForeArmR", Vec3{0,0,1},  M_PI/2);
        Kinematics::rotateJoint(m, ix, "ForeArmR", Vec3{0,0,1}, -M_PI/2);
        CHECK(eqv(m.skeleton[3].body.t.translation, 3.5, 0, 0), "hand restored after round-trip");
        CHECK(eqv(m.muscles[0].waypoints[0].p, 2.5, 0, 0), "waypoint restored after round-trip");
    }

    // ---- chain order: shoulder then elbow (proximal first) ----
    {
        Model m = makeArm();
        Index ix; ix.build(m);
        // rotate shoulder +90 about Z: whole arm swings up
        Kinematics::rotateJoint(m, ix, "ArmR", Vec3{0,0,1}, M_PI/2);
        // arm(1,0,0) about shoulder(0,0,0) -> (0,1,0); elbow(2,0,0)->(0,2,0)
        CHECK(eqv(m.skeleton[1].body.t.translation, 0, 1, 0), "shoulder rot: arm -> (0,1,0)");
        CHECK(eqv(m.skeleton[2].joint.t.translation, 0, 2, 0), "shoulder rot: elbow pivot -> (0,2,0)");
        // now flex elbow +90 about Z, pivot is the moved elbow (0,2,0)
        Kinematics::rotateJoint(m, ix, "ForeArmR", Vec3{0,0,1}, M_PI/2);
        // forearm body was at (0,2.5,0) after shoulder; about (0,2): d=(0,0.5)->(-0.5,0)->(-0.5,2)
        CHECK(eqv(m.skeleton[2].body.t.translation, -0.5, 2, 0), "elbow flex after shoulder -> (-0.5,2,0)");
    }

    // ---- DofMap ----
    {
        Model m = makeArm();
        Index ix; ix.build(m);
        DofMap dm;
        Dof d; d.name = "elbow_R.flex"; d.childBone = "ForeArmR";
        d.axis = {0,0,1}; d.sign = 1.0; d.loRad = 0.0; d.hiRad = M_PI/2;
        dm.add(d);
        CHECK(dm.size() == 1, "dofmap has 1 dof");
        Vec3 ax = dm.worldAxis(m, ix, "elbow_R.flex");
        CHECK(eqv(ax, 0, 0, 1), "world axis resolves to (0,0,1) at rest");
        bool ok = dm.applyValue(m, ix, "elbow_R.flex", 1.0);  // full flex = +90
        CHECK(ok, "applyValue(1.0) ok");
        CHECK(eqv(m.skeleton[3].body.t.translation, 2, 1.5, 0), "dof full flex: hand -> (2,1.5,0)");
    }

    std::printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nALL CHECKS PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
