// Atlas test: write a minimal OpenSim .osim, parse it, and validate/sync a small
// model against it (origin/insertion match, f0 deviation, side inference).
#include "../Atlas.h"
#include "../MassModel.h"
#include "../Index.h"
#include <cstdio>
#include <fstream>
#include <string>

using namespace mass;
using json = nlohmann::json;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_fail; } \
                              else { std::printf("ok  : %s\n", msg); } } while (0)

static const char* OSIM =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<OpenSimDocument Version=\"40000\">\n"
"  <Model name=\"t\">\n"
"    <ForceSet><objects>\n"
"      <Thelen2003Muscle name=\"bic_r\">\n"
"        <max_isometric_force>500</max_isometric_force>\n"
"        <optimal_fiber_length>0.12</optimal_fiber_length>\n"
"        <tendon_slack_length>0.2</tendon_slack_length>\n"
"        <pennation_angle_at_optimal>0.1</pennation_angle_at_optimal>\n"
"        <GeometryPath><PathPointSet><objects>\n"
"          <PathPoint name=\"p1\"><socket_parent_frame>/bodyset/scapula_r</socket_parent_frame></PathPoint>\n"
"          <PathPoint name=\"p2\"><socket_parent_frame>/bodyset/radius_r</socket_parent_frame></PathPoint>\n"
"        </objects></PathPointSet></GeometryPath>\n"
"      </Thelen2003Muscle>\n"
"      <Thelen2003Muscle name=\"tri_r\">\n"
"        <max_isometric_force>800</max_isometric_force>\n"
"        <optimal_fiber_length>0.10</optimal_fiber_length>\n"
"        <tendon_slack_length>0.19</tendon_slack_length>\n"
"        <pennation_angle_at_optimal>0.0</pennation_angle_at_optimal>\n"
"        <GeometryPath><PathPointSet><objects>\n"
"          <PathPoint name=\"p1\"><socket_parent_frame>/bodyset/scapula_r</socket_parent_frame></PathPoint>\n"
"          <PathPoint name=\"p2\"><socket_parent_frame>/bodyset/ulna_r</socket_parent_frame></PathPoint>\n"
"        </objects></PathPointSet></GeometryPath>\n"
"      </Thelen2003Muscle>\n"
"    </objects></ForceSet>\n"
"  </Model>\n"
"</OpenSimDocument>\n";

static Muscle muscle(const std::string& name, std::vector<std::string> bodies, double f0) {
    Muscle m; m.name = name; m.f0 = f0;
    for (auto& b : bodies) { Waypoint w; w.body = b; m.waypoints.push_back(w); }
    return m;
}
static int countIssue(const json& f, const std::string& issue) {
    int n = 0; for (auto& e : f) if (e["issue"] == issue) n++; return n;
}

int main() {
    // ---- normalize joins across naming conventions ----
    CHECK(Atlas::normalize("R_Bic") == Atlas::normalize("bic_r"), "R_Bic == bic_r");
    CHECK(Atlas::normalize("FemurR") == Atlas::normalize("femur_r"), "FemurR == femur_r");

    // write + load the .osim
    std::string path = "atlas_tmp.osim";
    { std::ofstream o(path); o << OSIM; }
    Atlas atlas; std::string err;
    CHECK(atlas.loadOsim(path, &err), "loadOsim ok");
    CHECK(atlas.size() == 2, "atlas has 2 muscles");
    const AtlasMuscle* bic = atlas.find("R_Bic");
    CHECK(bic != nullptr, "find R_Bic via normalized name");
    if (bic) {
        CHECK(bic->f0 == 500, "bic f0 == 500");
        CHECK(Atlas::normalize(bic->originBody) == Atlas::normalize("ScapulaR"), "bic origin = scapula");
        CHECK(Atlas::normalize(bic->insertionBody) == Atlas::normalize("RadiusR"), "bic insertion = radius");
    }

    // ---- model: R_Bic (good bodies, f0 off), R_Tri (bad origin), X_Unknown ----
    Model m;
    m.muscles.push_back(muscle("R_Bic", {"ScapulaR", "RadiusR"}, 1000));   // f0 1000 vs 500 -> deviation
    m.muscles.push_back(muscle("R_Tri", {"FemurR", "UlnaR"}, 800));        // origin FemurR vs scapula
    m.muscles.push_back(muscle("X_Unknown", {"Pelvis", "FemurR"}, 100));   // not in atlas
    Index ix; ix.build(m);

    json f = Atlas::validate(m, ix, atlas);
    CHECK(countIssue(f, "f0_deviation") == 1, "one f0 deviation (R_Bic)");
    CHECK(countIssue(f, "origin_mismatch") == 1, "one origin mismatch (R_Tri)");
    CHECK(countIssue(f, "not_in_atlas") == 1, "one not_in_atlas (X_Unknown)");

    // ---- sync: fill side + hill from atlas ----
    int changed = Atlas::sync(m, atlas, /*fillHill*/true, /*dryRun*/false);
    CHECK(changed == 2, "sync changed 2 matched muscles");
    CHECK(m.muscles[0].f0 == 500, "R_Bic f0 synced to 500");
    CHECK(m.muscles[0].side == "R", "R_Bic side inferred R");
    CHECK(m.muscles[2].f0 == 100, "X_Unknown untouched (not in atlas)");

    std::remove(path.c_str());
    std::printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nALL CHECKS PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
