// MCP server test: JSON-RPC dispatch over the real model + the single-writer
// McpQueue co-edit path (many threads submit, the owner thread drains).
#include "../Mcp.h"
#include "../MassModel.h"
#include "../Index.h"
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include <future>

using namespace mass;
using json = nlohmann::json;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++g_fail; } \
                              else { std::printf("ok  : %s\n", msg); } } while (0)

static json call(const std::string& tool, json args, int id = 1) {
    return { {"jsonrpc","2.0"}, {"id", id}, {"method","tools/call"},
             {"params", { {"name", tool}, {"arguments", std::move(args)} }} };
}

int main(int argc, char** argv) {
    std::string path = (argc > 1) ? argv[1] : "../../../../data/human.mass";
    std::string err;
    auto mo = Model::LoadMass(path, &err);
    if (!mo) { std::printf("cannot load %s: %s\n", path.c_str(), err.c_str()); return 2; }
    Model m = std::move(*mo);
    m.assignUids();
    Index ix; ix.build(m);
    McpServer server;

    // ---- initialize ----
    json init = server.handle({ {"jsonrpc","2.0"}, {"id",0}, {"method","initialize"} }, m, ix);
    CHECK(init["result"]["serverInfo"]["name"] == "mass-mcp", "initialize serverInfo");

    // ---- tools/list ----
    json tl = server.handle({ {"jsonrpc","2.0"}, {"id",0}, {"method","tools/list"} }, m, ix);
    bool hasScale = false;
    for (auto& t : tl["result"]["tools"]) if (t["name"] == "scale_bone") hasScale = true;
    CHECK(hasScale, "tools/list includes scale_bone");

    // ---- describe_model ----
    json d = server.handle(call("describe_model", json::object()), m, ix);
    CHECK(d["result"]["content"][0]["json"]["counts"]["bones"] == 23, "describe: 23 bones via MCP");

    // ---- scale_bone (mutating): knee should move ----
    Vec3 kneeBefore = m.skeleton[ix.boneSlot("TibiaR")].joint.t.translation;
    json sc = server.handle(call("scale_bone", { {"bone","FemurR"}, {"axis",{0,1,0}}, {"factor",1.5} }), m, ix);
    CHECK(sc["result"]["content"][0]["json"]["scaled"] == 1, "scale_bone scaled 1");
    Vec3 kneeAfter = m.skeleton[ix.boneSlot("TibiaR")].joint.t.translation;
    bool moved = kneeBefore[0]!=kneeAfter[0] || kneeBefore[1]!=kneeAfter[1] || kneeBefore[2]!=kneeAfter[2];
    CHECK(moved, "scale_bone moved the knee (child follows)");

    // ---- generate_fingers (mutating, rebuilds index) ----
    int bonesBefore = (int)m.skeleton.size();
    json gf = server.handle(call("generate_fingers", { {"hand","HandR"}, {"count",4}, {"phalanges",3} }), m, ix);
    CHECK(gf["result"]["content"][0]["json"]["created"].size() == 12, "generate_fingers created 12 bones");
    CHECK((int)m.skeleton.size() == bonesBefore + 12, "skeleton grew by 12");
    CHECK(ix.boneSlot("HandR_f1p1") >= 0, "index rebuilt: new bone queryable");

    // ---- select ----
    json se = server.handle(call("select", { {"expr","side=R"} }), m, ix);
    CHECK(se["result"]["content"][0]["json"].size() > 0, "select side=R via MCP non-empty");

    // ---- unknown method / tool errors ----
    json e1 = server.handle({ {"jsonrpc","2.0"}, {"id",9}, {"method","bogus"} }, m, ix);
    CHECK(e1["error"]["code"] == -32601, "unknown method -> error");
    json e2 = server.handle(call("no_such_tool", json::object()), m, ix);
    CHECK(e2["error"]["code"] == -32601, "unknown tool -> error");

    // ---- McpQueue: many threads submit, owner thread drains (single-writer) ----
    {
        McpQueue q;
        const int N = 16;
        const int expectBones = (int)m.skeleton.size();  // model was mutated by earlier calls
        std::vector<std::future<json>> futs;
        // deterministic contract check: submit a burst, then drain once
        for (int i = 0; i < N; i++) futs.push_back(q.submit(call("describe_model", json::object(), i)));
        CHECK(q.pending() == (size_t)N, "queue holds all submitted requests");
        int processed = q.drain(server, m, ix);
        CHECK(processed == N, "drain processed all");
        bool allOk = true;
        for (auto& f : futs) {
            json r = f.get();
            if (r["result"]["content"][0]["json"]["counts"]["bones"] != expectBones) allOk = false;
        }
        CHECK(allOk, "all drained futures resolved correctly");
        CHECK(q.pending() == 0, "queue empty after drain");
    }

    // concurrency: submit from real worker threads, drain on this (owner) thread
    {
        McpQueue q;
        std::atomic<int> submitted{0};
        std::vector<std::thread> ths;
        std::mutex fmx; std::vector<std::future<json>> futs;
        for (int i = 0; i < 8; i++)
            ths.emplace_back([&]() {
                auto f = q.submit(call("describe_model", json::object()));
                { std::lock_guard<std::mutex> lk(fmx); futs.push_back(std::move(f)); }
                submitted++;
            });
        for (auto& t : ths) t.join();
        CHECK(submitted == 8, "8 worker threads submitted");
        int processed = q.drain(server, m, ix);
        CHECK(processed == 8, "owner drained 8 from workers");
        for (auto& f : futs) f.get();  // no throw = all fulfilled
        CHECK(true, "all worker futures fulfilled");
    }

    std::printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nALL CHECKS PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}
