// Completion test: a hand with no fingers gets phalanx chains, then we "close"
// a finger (flex all its joints) and confirm the tip moves — i.e. the generated
// structure is real and animatable. Also checks list_gaps before/after.
#define _USE_MATH_DEFINES
#include "../Complete.h"
#include "../Kinematics.h"
#include <cstdio>
#include <cmath>
#include <string>

using namespace mass;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_fail; } \
                              else { std::printf("ok  : %s\n", msg); } } while (0)
static bool eq(double a, double b) { return std::fabs(a - b) < 1e-9; }
static bool eqv(const Vec3& p, double x, double y, double z) { return eq(p[0],x)&&eq(p[1],y)&&eq(p[2],z); }

int main() {
    Model m;
    // a bare right hand at the origin (root), plus a left hand for symmetry
    Node hr; hr.id = "HandR"; hr.body.type = "Box"; hr.body.t.translation = {0,0,0};
    Node hl; hl.id = "HandL"; hl.body.type = "Box"; hl.body.t.translation = {0,0,0};
    m.skeleton.push_back(hr);
    m.skeleton.push_back(hl);

    // ---- gaps before ----
    {
        Index ix; ix.build(m);
        auto gaps = Complete::listGaps(m, ix);
        CHECK(gaps.size() == 2, "list_gaps: both hands missing fingers");
    }

    // ---- generate 2 fingers x 2 phalanges on the right hand, deterministic geometry ----
    FingerConfig cfg;
    cfg.count = 2; cfg.phalanges = 2;
    cfg.forward = {1,0,0}; cfg.spread = {0,0,1};
    cfg.spacing = 0.1; cfg.length = 0.5; cfg.thickness = 0.05;
    auto created = Complete::generateFingers(m, "HandR", cfg);
    CHECK(created.size() == 4, "generated 4 phalanx bones");
    CHECK(created[0] == "HandR_f1p1", "naming: HandR_f1p1");
    CHECK((int)m.skeleton.size() == 6, "skeleton grew by 4");

    Index ix; ix.build(m);
    // finger fan centered on palm: span=0.1, offsets -0.05 (f1) and +0.05 (f2)
    // f1 p1 joint at (0,0,-0.05), body (0.25,0,-0.05); p2 joint (0.5,0,-0.05)
    CHECK(eqv(m.skeleton[ix.boneSlot("HandR_f1p1")].joint.t.translation, 0,0,-0.05), "f1p1 joint pos");
    CHECK(eqv(m.skeleton[ix.boneSlot("HandR_f1p2")].joint.t.translation, 0.5,0,-0.05), "f1p2 joint pos");
    CHECK(ix.parentOf(ix.boneSlot("HandR_f1p2")) == ix.boneSlot("HandR_f1p1"), "p2 parent p1");
    CHECK(ix.parentOf(ix.boneSlot("HandR_f1p1")) == ix.boneSlot("HandR"), "p1 parent hand");
    CHECK(ix.subtreeBones("HandR").size() == 5, "hand subtree = hand + 4 phalanges");
    CHECK(m.skeleton[ix.boneSlot("HandR_f1p1")].joint.type == "Revolute", "phalanx joint is Revolute");

    // ---- gaps after: right hand now articulates, left still bare ----
    {
        auto gaps = Complete::listGaps(m, ix);
        CHECK(gaps.size() == 1, "list_gaps: only left hand missing now");
        CHECK(gaps[0]["bone"] == "HandL", "remaining gap is HandL");
    }

    // ---- "close" finger 1: flex its proximal joint +90 about spread (Z) ----
    // pivot f1p1 joint (0,0,-0.05); p2 body was (0.75,0,-0.05):
    //   d=(0.75,0,0) about Z+90 (x,y)->(-y,x): (0.75,0)->(0,0.75) => (0,0.75,-0.05)
    Kinematics::rotateJoint(m, ix, "HandR_f1p1", Vec3{0,0,1}, M_PI/2);
    CHECK(eqv(m.skeleton[ix.boneSlot("HandR_f1p2")].body.t.translation, 0,0.75,-0.05),
          "finger tip moved on flex (animatable)");
    // finger 2 untouched
    CHECK(eqv(m.skeleton[ix.boneSlot("HandR_f2p1")].body.t.translation, 0.25,0,0.05),
          "other finger unaffected");

    // ---- symmetric generation fills the left hand too ----
    {
        Model m2;
        Node a; a.id = "HandR"; a.body.type="Box"; a.body.t.translation={0,0,0};
        Node b; b.id = "HandL"; b.body.type="Box"; b.body.t.translation={0,0,0};
        m2.skeleton.push_back(a); m2.skeleton.push_back(b);
        auto all = Complete::generateFingersSymmetric(m2, "HandR", cfg);
        CHECK(all.size() == 8, "symmetric generated 8 bones (both hands)");
        Index ix2; ix2.build(m2);
        CHECK(ix2.subtreeBones("HandL").size() == 5, "left hand now articulates");
        CHECK(Complete::listGaps(m2, ix2).size() == 0, "no gaps after symmetric completion");
    }

    std::printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nALL CHECKS PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
