// Groom test: the PBD guide solver (root pinned, gravity, inextensible segments
// settle to a vertical hang) and GroomParams persistence in .mass.
#include "../Groom.h"
#include "../MassModel.h"
#include <cstdio>
#include <cmath>
#include <string>

using namespace mass;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_fail; } \
                              else { std::printf("ok  : %s\n", msg); } } while (0)
static bool near(double a, double b, double tol) { return std::fabs(a - b) < tol; }

int main() {
    // ---- PBD dynamic tier ----
    {
        HairSim sim;
        const int segs = 4; const double L = 0.25;
        sim.init(Vec3{0,0,0}, Vec3{1,0,0}, segs, L);   // starts horizontal along +X
        CHECK(sim.links() == segs, "guide has 4 links");
        CHECK(near(sim.segLength(0), L, 1e-9), "initial segment length");

        for (int i = 0; i < 1000; i++)
            sim.step(1.0/120.0, Vec3{0,-9.8,0}, Vec3{0,0,0}, 0.1, 20);

        // root pinned
        const Vec3& root = sim.points()[0];
        CHECK(near(root[0],0,1e-9) && near(root[1],0,1e-9) && near(root[2],0,1e-9), "root stays pinned");
        // inextensible: every segment keeps rest length
        bool inext = true;
        for (int i = 0; i < sim.links(); i++) if (!near(sim.segLength(i), L, 1e-3)) inext = false;
        CHECK(inext, "all segments inextensible after settling");
        // settles to a vertical hang: tip ~ (0,-1.0,0)
        Vec3 tip = sim.tip();
        CHECK(near(tip[0], 0.0, 0.05), "tip x settled ~0");
        CHECK(near(tip[1], -segs*L, 0.05), "tip hangs down ~ -1.0");
    }

    // ---- gravity vs wind: strong lateral wind pushes the tip out in +X ----
    {
        HairSim sim; sim.init(Vec3{0,0,0}, Vec3{0,-1,0}, 4, 0.25);  // start hanging down
        for (int i = 0; i < 1000; i++)
            sim.step(1.0/120.0, Vec3{0,-9.8,0}, Vec3{40,0,0}, 0.1, 20);  // hard wind +X
        CHECK(sim.tip()[0] > 0.1, "wind deflects the tip in +X");
    }

    // ---- GroomParams persistence ----
    {
        Model m;
        GroomParams g; g.name = "scalp"; g.region = "Head"; g.length = 0.4;
        g.guides = 200; g.segments = 30; g.dynamic = true; g.stiffness = 0.7;
        m.grooms.push_back(g);
        std::string err, tmp = "groom_tmp.mass";
        CHECK(m.SaveMass(tmp, &err), "save grooms");
        auto m2 = Model::LoadMass(tmp, &err);
        CHECK(m2.has_value(), "reload");
        if (m2) {
            CHECK(m2->grooms.size() == 1, "one groom persisted");
            CHECK(m2->grooms[0].name == "scalp", "groom name preserved");
            CHECK(near(m2->grooms[0].length, 0.4, 1e-9), "groom length preserved");
            CHECK(m2->grooms[0].guides == 200, "groom guides preserved");
            CHECK(m2->grooms[0].dynamic == true, "groom dynamic preserved");
        }
        std::remove(tmp.c_str());
    }

    std::printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nALL CHECKS PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
