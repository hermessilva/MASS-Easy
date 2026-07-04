// Query + uid test: load the real human.mass, migrate uids, and check the JSON
// query facade + uid persistence round-trip.
#include "../Query.h"
#include "../MassModel.h"
#include <cstdio>
#include <string>
#include <unordered_set>

using namespace mass;
using json = nlohmann::json;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_fail; } \
                              else { std::printf("ok  : %s\n", msg); } } while (0)

int main(int argc, char** argv) {
    std::string path = (argc > 1) ? argv[1] : "../../../../data/human.mass";
    std::string err;
    auto m = Model::LoadMass(path, &err);
    if (!m) { std::printf("cannot load %s: %s\n", path.c_str(), err.c_str()); return 2; }

    // ---- uid migration ----
    int assigned = m->assignUids();
    std::printf("assigned %d uids\n", assigned);
    CHECK(assigned == (int)(m->skeleton.size() + m->muscles.size()), "all elements got a uid");

    std::unordered_set<std::string> uids;
    for (auto& n : m->skeleton) uids.insert(n.uid);
    for (auto& mu : m->muscles) uids.insert(mu.uid);
    CHECK((int)uids.size() == assigned, "all uids unique");
    CHECK(m->assignUids() == 0, "second assignUids is a no-op");

    Index ix; ix.build(*m);

    // ---- uid lookups ----
    std::string pelvisUid = m->skeleton[0].uid;
    EntityId e = ix.boneByUid(pelvisUid);
    CHECK(e.valid(), "boneByUid resolves");
    CHECK(m->skeleton[e.slot].id == m->skeleton[0].id, "uid maps to right bone");
    CHECK(ix.resolve("Pelvis").valid(), "resolve by name works");
    CHECK(ix.resolve(pelvisUid).valid(), "resolve by uid works");

    // ---- describe ----
    json d = Query::describeModel(*m, ix);
    CHECK(d["counts"]["bones"] == 23, "describe: 23 bones");
    CHECK(d["counts"]["muscles"] == 284, "describe: 284 muscles");

    // ---- getNode ----
    json pelvis = Query::getNode(*m, ix, "Pelvis");
    CHECK(!pelvis.is_null(), "getNode(Pelvis) not null");
    CHECK(pelvis["parent"] == "", "Pelvis is root");
    CHECK(pelvis["muscles"].size() == 112, "Pelvis: 112 muscles listed");
    json pelvisByUid = Query::getNode(*m, ix, pelvisUid);
    CHECK(pelvisByUid["id"] == "Pelvis", "getNode by uid returns Pelvis");

    // ---- getMuscle ----
    std::string mname = m->muscles[0].name;
    json mu = Query::getMuscle(*m, ix, mname);
    CHECK(!mu.is_null(), "getMuscle not null");
    CHECK(!std::string(mu["origin"]).empty(), "muscle has an origin bone");

    // ---- select ----
    json sel = Query::select(*m, ix, "side=R");
    std::printf("select side=R -> %zu entries\n", sel.size());
    CHECK(sel.size() > 0, "select side=R non-empty");
    CHECK(sel[0].contains("uid") && sel[0].contains("name"), "select entries carry uid+name");

    // ---- uid persistence round-trip ----
    std::string tmp = "roundtrip_uids.mass";
    CHECK(m->SaveMass(tmp, &err), "save with uids");
    auto m2 = Model::LoadMass(tmp, &err);
    CHECK(m2.has_value(), "reload");
    if (m2) {
        CHECK(m2->skeleton[0].uid == pelvisUid, "bone uid preserved across save/load");
        CHECK(m2->assignUids() == 0, "reloaded model needs no migration");
    }
    std::remove(tmp.c_str());

    std::printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nALL CHECKS PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
