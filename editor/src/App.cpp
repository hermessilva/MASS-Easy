#include "App.h"
#include "Bootstrap.h"
#include "Dialog.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace mass {

static std::string deriveRoot(const std::string& fileInData);

// ---- lifecycle ----
bool App::init(GLFWwindow* win) {
    mWin = win;
    if (!mRen.init()) { std::fprintf(stderr, "renderer init failed\n"); return false; }
    newModel();
    mTrain.start(8765);   // telemetry server
    return true;
}
void App::shutdown() { mTrain.stop(); mRen.shutdown(); }

void App::startTraining() {
    // export current model to the real training data dir, then launch training
    std::string err;
    if (!ExportToLegacy(mModel, mDataRoot + "/data", &err)) { mStatus = "export p/ data falhou: " + err; return; }
    std::string cmd = "powershell -ExecutionPolicy Bypass -File \"" + mDataRoot + "\\scripts\\train.ps1\"";
    mTrain.launchTraining(cmd);
    mStatus = "Treino disparado (modelo exportado p/ data)";
}

// ---- OBJ loader (positions in world*100 -> *0.01; per-vertex normals) ----
static bool loadObj(const std::string& path, MeshData& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::vector<V3> verts, norms;
    std::string line;
    const float S = 0.01f;
    while (std::getline(f, line)) {
        if (line.size() < 2) continue;
        if (line[0] == 'v' && line[1] == ' ') {
            float x,y,z; if (std::sscanf(line.c_str()+2, "%f %f %f", &x,&y,&z)==3) verts.push_back({x*S,y*S,z*S});
        } else if (line[0] == 'v' && line[1] == 'n') {
            float x,y,z; if (std::sscanf(line.c_str()+3, "%f %f %f", &x,&y,&z)==3) norms.push_back({x,y,z});
        } else if (line[0] == 'f' && line[1] == ' ') {
            // parse polygon vertices: tokens "v/vt/vn"
            std::vector<int> vi, ni;
            std::stringstream ss(line.substr(2));
            std::string tok;
            while (ss >> tok) {
                int a=0,b=0,c=0;
                // formats: a, a/b, a//c, a/b/c
                size_t p1 = tok.find('/');
                if (p1 == std::string::npos) { a = std::stoi(tok); }
                else {
                    a = std::stoi(tok.substr(0,p1));
                    size_t p2 = tok.find('/', p1+1);
                    if (p2 == std::string::npos) { /* a/b */ }
                    else if (p2 > p1+1) c = std::stoi(tok.substr(p2+1));
                    else c = std::stoi(tok.substr(p2+1)); // a//c
                }
                vi.push_back(a); ni.push_back(c);
            }
            // triangulate fan
            for (size_t k = 1; k + 1 < vi.size(); k++) {
                int idx[3] = {0, (int)k, (int)k+1};
                V3 tp[3], tn[3]; bool haveN = true;
                for (int t=0;t<3;t++) {
                    int v = vi[idx[t]]; if (v<0) v = (int)verts.size()+v+1;
                    if (v<1 || v>(int)verts.size()) { haveN=false; break; }
                    tp[t] = verts[v-1];
                    int nn = ni[idx[t]];
                    if (nn>0 && nn<=(int)norms.size()) tn[t] = norms[nn-1];
                    else haveN = false;
                }
                if (tp[0].x==0&&tp[0].y==0&&tp[0].z==0 && !haveN) {}
                // compute face normal if missing
                if (!haveN) {
                    V3 fn = normalize(cross(tp[1]-tp[0], tp[2]-tp[0]));
                    tn[0]=tn[1]=tn[2]=fn;
                }
                for (int t=0;t<3;t++){ out.pos.push_back(tp[t]); out.nrm.push_back(tn[t]); }
            }
        }
    }
    return !out.pos.empty();
}

void App::loadMeshes() {
    mRen.freeMeshes();
    mMeshes.clear();
    mMeshes.resize(mModel.skeleton.size());
    for (size_t i = 0; i < mModel.skeleton.size(); i++) {
        const Node& n = mModel.skeleton[i];
        mMeshes[i].objName = n.body.obj;
        if (n.body.obj.empty()) continue;
        if (loadObj(mDataRoot + "/data/OBJ/" + n.body.obj, mMeshes[i]))
            mMeshes[i].gpuId = mRen.uploadMesh(mMeshes[i].pos, mMeshes[i].nrm);
    }
}

// Reload meshes whenever the model's obj files or node count change (live edit).
void App::syncMeshes() {
    bool dirty = (mMeshes.size() != mModel.skeleton.size());
    for (size_t i = 0; !dirty && i < mModel.skeleton.size(); i++)
        if (mMeshes[i].objName != mModel.skeleton[i].body.obj) dirty = true;
    if (dirty) loadMeshes();
}

void App::seedLightsIfEmpty() {
    if (!mModel.lights.empty()) return;
    Light key;  key.name = "Principal"; key.type = 0; key.dir = {0.4, 0.9, 0.5};
    key.color = {1.0, 0.97, 0.9}; key.intensity = 1.1;
    Light fill; fill.name = "Preenchimento"; fill.type = 0; fill.dir = {-0.5, 0.4, -0.4};
    fill.color = {0.6, 0.7, 1.0}; fill.intensity = 0.6;
    Light back; back.name = "Contra"; back.type = 0; back.dir = {0.0, 0.3, -0.9};
    back.color = {0.8, 0.8, 0.9}; back.intensity = 0.4;
    mModel.lights = { key, fill, back };
    mModel.ambient = 0.4;
}

void App::newModel() {
    mModel = Model();
    seedLightsIfEmpty();
    mMeshes.clear();
    mSel.clear();
    mUndo.clear(); mRedo.clear();
    mProjectPath.clear();
    mStatus = "Novo modelo vazio";
}

void App::loadProjectPath(const std::string& path) {
    std::string err;
    auto m = Model::LoadMass(path, &err);
    if (m) { mModel = *m; mProjectPath = path; mDataRoot = deriveRoot(path);
             seedLightsIfEmpty(); loadMeshes(); mSel.clear(); mUndo.clear(); mRedo.clear();
             mStatus = "Aberto: " + path; }
    else mStatus = "Erro ao abrir: " + err;
}

// ---- undo ----
void App::snapshot() {
    mUndo.push_back(mModel);
    if (mUndo.size() > 100) mUndo.pop_front();
    mRedo.clear();
}
void App::undo() {
    if (mUndo.empty()) return;
    mRedo.push_back(mModel);
    mModel = mUndo.back(); mUndo.pop_back();
    mSel.clear(); mStatus = "Desfeito";
}
void App::redo() {
    if (mRedo.empty()) return;
    mUndo.push_back(mModel);
    mModel = mRedo.back(); mRedo.pop_back();
    mSel.clear(); mStatus = "Refeito";
}

// ---- helpers ----
Node* App::selNode() {
    if ((mSel.type == SelType::Body || mSel.type == SelType::Joint) &&
        mSel.index >= 0 && mSel.index < (int)mModel.skeleton.size())
        return &mModel.skeleton[mSel.index];
    return nullptr;
}
Muscle* App::selMuscle() {
    if ((mSel.type == SelType::Muscle || mSel.type == SelType::Waypoint) &&
        mSel.index >= 0 && mSel.index < (int)mModel.muscles.size())
        return &mModel.muscles[mSel.index];
    return nullptr;
}
M4 App::nodeBodyMatrix(const Node& n) const { return fromTransform(n.body.t); }
V3 App::worldOfWaypoint(const Waypoint& w) const { return V3(w.p); }
V3 App::lightHandle(const Light& L) const {
    if (L.type == 1) return V3(L.dir);                       // point: position itself
    V3 center{0, 1.2f, 0};
    return center + normalize(V3(L.dir)) * 1.8f;             // directional: handle offset along dir
}

static std::string deriveRoot(const std::string& fileInData) {
    std::string dir = fileInData;
    size_t s = dir.find_last_of("/\\"); if (s != std::string::npos) dir = dir.substr(0, s);
    // if dir ends with "data", root is its parent
    size_t s2 = dir.find_last_of("/\\");
    std::string base = (s2 != std::string::npos) ? dir.substr(s2 + 1) : dir;
    if (base == "data" && s2 != std::string::npos) return dir.substr(0, s2);
    return dir;
}

M4 App::liveBodyMatrix(const Node& n) const {
    if (mSimActive) {
        auto it = mLivePose.find(n.id);
        if (it != mLivePose.end()) return fromTransform(it->second);
    }
    return fromTransform(n.body.t);
}
V3 App::liveWaypoint(const Waypoint& w) const {
    if (mSimActive) {
        const Node* n = mModel.findNode(w.body);
        auto it = mLivePose.find(w.body);
        if (n && it != mLivePose.end()) {
            M4 delta = mul(fromTransform(it->second), rigidInverse(fromTransform(n->body.t)));
            return mulPoint(delta, V3(w.p));
        }
    }
    return V3(w.p);
}

void App::toggleSim() {
    if (mSimActive) {
        mSim.stop();
        mSimActive = false;
        mLivePose.clear();
        mStatus = "Simulacao parada (modo edicao)";
    } else {
        if (mModel.skeleton.empty()) { mStatus = "Carregue um modelo primeiro"; return; }
        std::string tmp = mDataRoot + "/build/editor_tmp";
#ifdef _WIN32
        std::string mk = "cmd /c mkdir \"" + tmp + "\" 2>nul";
        std::system(mk.c_str());
#endif
        mSim.configure(mDataRoot, tmp);
        mSim.setModel(mModel);
        mSim.setMode(SimBridge::Kinematic);
        mSim.setActivation(mActivation);
        mSim.setPaused(false);
        mSim.start();
        mSimActive = true;
        mStatus = "Simulacao iniciada";
    }
}

// ---- scene ----
void App::drawScene() {
    if (mVpW < 1 || mVpH < 1) return;
    syncMeshes();  // keep visuals in sync with the model's obj/type edits
    // render into the offscreen FBO at 2x (supersampling AA), shown via ImGui::Image
    const float ss = 2.0f;
    mRen.beginTarget((int)(mVpW * ss), (int)(mVpH * ss));

    float aspect = mVpW / mVpH;
    M4 vp = mul(mCam.proj(aspect), mCam.view());
    mRen.begin(vp, mCam.eye());

    // scene lights -> renderer
    std::vector<Renderer::LightGPU> gpu;
    for (const auto& L : mModel.lights) {
        if (!L.enabled) continue;
        gpu.push_back({ L.type, V3(L.dir),
            V3{(float)(L.color[0]*L.intensity), (float)(L.color[1]*L.intensity), (float)(L.color[2]*L.intensity)} });
    }
    mRen.setLights(gpu, (float)mModel.ambient);

    if (mSimActive && mSim.running()) mLivePose = mSim.pose();

    // checkerboard floor: 0.5 m tiles, dark, to scale (10 m -> 20 divs = 0.5 m)
    if (mDrawGrid) mRen.checkerGround(10.0f, 20, {0.16f,0.16f,0.19f}, {0.09f,0.09f,0.11f});

    auto clamp01 = [](double v){ return v<0?0.0f:(v>1?1.0f:(float)v); };

    // skeleton (solid bodies + edge definition)
    for (int i = 0; i < (int)mModel.skeleton.size(); i++) {
        const Node& n = mModel.skeleton[i];
        M4 bm = liveBodyMatrix(n);
        bool selBody = (mSel.type == SelType::Body && mSel.index == i);
        bool selJoint = (mSel.type == SelType::Joint && mSel.index == i);
        V3 base = { clamp01(n.body.color[0]), clamp01(n.body.color[1]), clamp01(n.body.color[2]) };
        if (base.x + base.y + base.z < 0.05f) base = {0.30f, 0.40f, 0.95f}; // default blue
        base = { base.x*0.6f+0.15f, base.y*0.6f+0.18f, base.z*0.6f+0.35f }; // brighten toward reference blue
        V3 col = selBody ? V3{1.0f, 0.75f, 0.15f} : base;
        V3 he = n.body.type == "Box"
            ? V3{(float)n.body.size[0]*0.5f, (float)n.body.size[1]*0.5f, (float)n.body.size[2]*0.5f}
            : V3{(float)n.body.radius, (float)n.body.radius, (float)n.body.radius};
        if (mShowBones) {
            const MeshData* md = (i < (int)mMeshes.size()) ? &mMeshes[i] : nullptr;
            if (mShowMesh && md && md->gpuId != 0) {
                // mesh authored in rest world space -> follow live body via delta (transform on GPU)
                M4 delta = mul(bm, rigidInverse(fromTransform(n.body.t)));
                V3 mcol = selBody ? V3{1.0f, 0.75f, 0.2f} : V3{0.86f, 0.80f, 0.72f}; // bone/ivory
                mRen.drawMeshGPU(md->gpuId, delta, mcol);
            } else {
                if (n.body.type == "Sphere") mRen.solidSphere(mulPoint(bm,{0,0,0}), (float)n.body.radius, col);
                else mRen.solidBox(bm, he, col);
                if (selBody) mRen.wireBox(bm, he, {1.0f,0.9f,0.3f}, 1.0f);
            }
        }
        // joint marker
        if (mShowJoints) {
            M4 jm = fromTransform(n.joint.t);
            mRen.axes(jm, selJoint ? 0.12f : 0.05f);
            if (selJoint) mRen.point(mulPoint(jm, {0,0,0}), {1.0f,0.8f,0.2f});
        }
    }

    // muscles
    if (mShowMuscles) {
        for (int i = 0; i < (int)mModel.muscles.size(); i++) {
            const Muscle& mu = mModel.muscles[i];
            bool selM = ((mSel.type == SelType::Muscle || mSel.type == SelType::Waypoint) && mSel.index == i);
            V3 mcol = selM ? V3{1.0f, 1.0f, 0.3f} : V3{0.85f, 0.15f, 0.2f};
            for (size_t k = 1; k < mu.waypoints.size(); k++)
                mRen.line(liveWaypoint(mu.waypoints[k-1]), liveWaypoint(mu.waypoints[k]), mcol, 0.9f);
            if (mShowWaypoints)
                for (size_t k = 0; k < mu.waypoints.size(); k++) {
                    bool selW = (mSel.type == SelType::Waypoint && mSel.index == i && mSel.sub == (int)k);
                    mRen.point(liveWaypoint(mu.waypoints[k]), selW ? V3{0.2f,1.0f,1.0f} : V3{1.0f,0.6f,0.2f});
                }
        }
    }

    // light markers (visible + clickable icons)
    for (int i = 0; mShowLightMarkers && i < (int)mModel.lights.size(); i++) {
        const Light& L = mModel.lights[i];
        bool sel = (mSel.type == SelType::Light && mSel.index == i);
        V3 c = sel ? V3{1.0f, 1.0f, 0.2f}
                   : V3{ (float)L.color[0], (float)L.color[1], (float)L.color[2] };
        if (!L.enabled) c = c * 0.35f;
        V3 h = lightHandle(L);
        mRen.solidSphere(h, sel ? 0.10f : 0.07f, c);       // emissive bulb
        // radiating rays
        const float r = 0.16f;
        mRen.line(h, h + V3{r,0,0}, c);  mRen.line(h, h + V3{-r,0,0}, c);
        mRen.line(h, h + V3{0,r,0}, c);  mRen.line(h, h + V3{0,-r,0}, c);
        mRen.line(h, h + V3{0,0,r}, c);  mRen.line(h, h + V3{0,0,-r}, c);
        if (L.type == 0) { // directional: line from handle toward the model center (aim)
            V3 center{0, 1.2f, 0};
            mRen.line(h, center, c, 0.5f);
        }
    }

    mRen.flush();
    mRen.endTarget();
}

// ---- picking ----
void App::pickAt(double mx, double my) {
    float aspect = mVpW / mVpH;
    float nx = ((float)(mx - mVpX) / mVpW) * 2.0f - 1.0f;
    float ny = 1.0f - ((float)(my - mVpY) / mVpH) * 2.0f;
    Ray ray = mCam.rayFromNDC(nx, ny, aspect);

    float best = 1e30f;
    Selection hit;

    // lights (handles) — highest priority
    for (int i = 0; i < (int)mModel.lights.size(); i++) {
        float t = raySphere(ray, lightHandle(mModel.lights[i]), 0.14f);
        if (t > 0 && t < best) { best = t; hit = {SelType::Light, i, -1}; }
    }

    // waypoints (small spheres)
    for (int i = 0; i < (int)mModel.muscles.size() && mShowMuscles; i++) {
        const Muscle& mu = mModel.muscles[i];
        for (int k = 0; k < (int)mu.waypoints.size(); k++) {
            float t = raySphere(ray, worldOfWaypoint(mu.waypoints[k]), 0.02f);
            if (t > 0 && t < best) { best = t; hit = {SelType::Waypoint, i, k}; }
        }
    }
    // bodies (OBB)
    for (int i = 0; i < (int)mModel.skeleton.size(); i++) {
        const Node& n = mModel.skeleton[i];
        V3 he = n.body.type == "Box"
            ? V3{(float)n.body.size[0]*0.5f, (float)n.body.size[1]*0.5f, (float)n.body.size[2]*0.5f}
            : V3{(float)n.body.radius,(float)n.body.radius,(float)n.body.radius};
        float t = rayOBB(ray, nodeBodyMatrix(n), he);
        if (t > 0 && t < best) { best = t; hit = {SelType::Body, i, -1}; }
    }
    // muscle segments (thin) — only if nothing closer
    if (hit.type == SelType::None && mShowMuscles) {
        float bestDist = 0.03f;
        for (int i = 0; i < (int)mModel.muscles.size(); i++) {
            const Muscle& mu = mModel.muscles[i];
            for (size_t k = 1; k < mu.waypoints.size(); k++) {
                float tt;
                float d = raySegmentDist(ray, worldOfWaypoint(mu.waypoints[k-1]), worldOfWaypoint(mu.waypoints[k]), tt);
                if (d < bestDist) { bestDist = d; hit = {SelType::Muscle, i, -1}; }
            }
        }
    }
    mSel = hit;
    if (hit.type == SelType::Light) mSelLight = hit.index;
}

// ---- gizmo ----
void App::drawGizmo() {
    if (mSel.type == SelType::None) return;
    if (mSimActive) return; // no structural editing while simulating
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetRect(mVpX, mVpY, mVpW, mVpH);
    float aspect = mVpW / mVpH;
    M4 view = mCam.view();
    M4 proj = mCam.proj(aspect);

    if (Node* n = selNode()) {
        bool isJoint = (mSel.type == SelType::Joint);
        Transform& t = isJoint ? n->joint.t : n->body.t;
        M4 model = fromTransform(t);
        static M4 before; static bool wasUsing = false;
        if (ImGuizmo::Manipulate(view.data(), proj.data(),
                (ImGuizmo::OPERATION)mGizmoOp, (ImGuizmo::MODE)mGizmoMode, model.data())) {
            // write back column-major model -> Transform (row-major Mat3 + Vec3)
            t.linear = { model.m[0],model.m[4],model.m[8],
                         model.m[1],model.m[5],model.m[9],
                         model.m[2],model.m[6],model.m[10] };
            t.translation = { model.m[12], model.m[13], model.m[14] };
        }
        if (ImGuizmo::IsUsing() && !wasUsing) { snapshot(); }
        wasUsing = ImGuizmo::IsUsing();
    } else if (mSel.type == SelType::Light) {
        if (mSel.index >= 0 && mSel.index < (int)mModel.lights.size()) {
            Light& L = mModel.lights[mSel.index];
            V3 h = lightHandle(L);
            M4 model; model.m[12]=h.x; model.m[13]=h.y; model.m[14]=h.z;
            static bool wasUsingL = false;
            if (ImGuizmo::Manipulate(view.data(), proj.data(),
                    ImGuizmo::TRANSLATE, ImGuizmo::WORLD, model.data())) {
                V3 np{model.m[12], model.m[13], model.m[14]};
                if (L.type == 1) { L.dir = {np.x, np.y, np.z}; }
                else { V3 d = normalize(np - V3{0,1.2f,0}); L.dir = {d.x, d.y, d.z}; }
            }
            if (ImGuizmo::IsUsing() && !wasUsingL) snapshot();
            wasUsingL = ImGuizmo::IsUsing();
        }
    } else if (mSel.type == SelType::Waypoint) {
        if (Muscle* mu = selMuscle()) {
            if (mSel.sub >= 0 && mSel.sub < (int)mu->waypoints.size()) {
                Waypoint& w = mu->waypoints[mSel.sub];
                M4 model; model.m[12]=(float)w.p[0]; model.m[13]=(float)w.p[1]; model.m[14]=(float)w.p[2];
                static bool wasUsing = false;
                if (ImGuizmo::Manipulate(view.data(), proj.data(),
                        ImGuizmo::TRANSLATE, ImGuizmo::WORLD, model.data())) {
                    w.p = { model.m[12], model.m[13], model.m[14] };
                }
                if (ImGuizmo::IsUsing() && !wasUsing) snapshot();
                wasUsing = ImGuizmo::IsUsing();
            }
        }
    }
}

// ---- actions ----
void App::openMass() {
    std::string p = openFileDialog();
    if (p.empty()) return;
    std::string err;
    auto m = Model::LoadMass(p, &err);
    if (m) { mModel = *m; mProjectPath = p; mDataRoot = deriveRoot(p);
             seedLightsIfEmpty(); loadMeshes(); mSel.clear(); mUndo.clear(); mRedo.clear();
             mStatus = "Aberto: " + p; }
    else mStatus = "Erro ao abrir: " + err;
}
void App::saveMass(bool as) {
    std::string p = mProjectPath;
    if (as || p.empty()) p = saveFileDialog();
    if (p.empty()) return;
    std::string err;
    if (mModel.SaveMass(p, &err)) { mProjectPath = p; mStatus = "Salvo: " + p; }
    else mStatus = "Erro ao salvar: " + err;
}
void App::bootstrap() {
    std::string meta = openFileDialog("metadata.txt\0*.txt\0All\0*.*\0");
    if (meta.empty()) return;
    // data_root = parent of the folder containing metadata (metadata paths are /data/..)
    std::string dir = meta;
    size_t s = dir.find_last_of("/\\"); if (s != std::string::npos) dir = dir.substr(0, s); // .../data
    size_t s2 = dir.find_last_of("/\\"); std::string root = (s2 != std::string::npos) ? dir.substr(0, s2) : ".";
    std::string err;
    auto m = BootstrapFromLegacy(meta, root, &err);
    if (m) { mModel = *m; mDataRoot = root; seedLightsIfEmpty(); loadMeshes(); mSel.clear(); mUndo.clear(); mRedo.clear();
             mStatus = "Bootstrap OK: " + std::to_string(mModel.skeleton.size()) + " nodes, "
                       + std::to_string(mModel.muscles.size()) + " musculos"; }
    else mStatus = "Erro bootstrap: " + err;
}
void App::exportLegacy() {
    std::string dir = pickFolderDialog();
    if (dir.empty()) return;
    std::string err;
    if (ExportToLegacy(mModel, dir, &err)) mStatus = "Exportado para: " + dir;
    else mStatus = "Erro export: " + err;
}
void App::addBody() {
    snapshot();
    Node n;
    n.id = "NewBody" + std::to_string(mModel.skeleton.size());
    if (!mModel.skeleton.empty()) n.parent = mModel.skeleton.front().id;
    n.joint.type = "Ball";
    mModel.skeleton.push_back(n);
    mSel = {SelType::Body, (int)mModel.skeleton.size()-1, -1};
    mStatus = "Body adicionado";
}
void App::removeSelected() {
    if (mSel.type == SelType::Body || mSel.type == SelType::Joint) {
        if (mSel.index < 0 || mSel.index >= (int)mModel.skeleton.size()) return;
        snapshot();
        mModel.skeleton.erase(mModel.skeleton.begin() + mSel.index);
        mSel.clear(); mStatus = "Node removido";
    } else if (mSel.type == SelType::Muscle) {
        if (mSel.index < 0 || mSel.index >= (int)mModel.muscles.size()) return;
        snapshot();
        mModel.muscles.erase(mModel.muscles.begin() + mSel.index);
        mSel.clear(); mStatus = "Musculo removido";
    } else if (mSel.type == SelType::Waypoint) {
        if (Muscle* mu = selMuscle()) {
            if (mu->waypoints.size() > 2 && mSel.sub >= 0) {
                snapshot();
                mu->waypoints.erase(mu->waypoints.begin() + mSel.sub);
                mSel = {SelType::Muscle, mSel.index, -1}; mStatus = "Waypoint removido";
            }
        }
    }
}
void App::importOsim() {
    std::string p = openFileDialog("OpenSim model (*.osim)\0*.osim\0All\0*.*\0");
    if (p.empty()) return;
    std::string err;
    mAtlas = ImportOsimMuscles(p, &err);
    mStatus = mAtlas.empty() ? ("Atlas: " + err)
                             : ("Atlas carregado: " + std::to_string(mAtlas.size()) + " musculos de referencia");
}
void App::applyAtlas(const AtlasEntry& a) {
    if (Muscle* mu = selMuscle()) {
        snapshot();
        mu->f0 = a.f0;                 // physiological max isometric force
        mu->pen_angle = a.pen_angle;   // pennation angle (rad)
        if (mu->latin.empty()) mu->latin = a.name;
        mStatus = "Atlas aplicado a " + mu->name + " (f0=" + std::to_string((int)a.f0) + "N)";
    } else mStatus = "Selecione um musculo primeiro";
}
// mirror a body/muscle name across sides: FemurR<->FemurL, R_x<->L_x
static std::string mirrorName(const std::string& s) {
    if (s.size() >= 2 && s.rfind("R_", 0) == 0) return "L_" + s.substr(2);
    if (s.size() >= 2 && s.rfind("L_", 0) == 0) return "R_" + s.substr(2);
    if (!s.empty() && s.back() == 'R') return s.substr(0, s.size()-1) + "L";
    if (!s.empty() && s.back() == 'L') return s.substr(0, s.size()-1) + "R";
    return s;
}
void App::mirrorSelectedMuscle() {
    Muscle* mu = selMuscle();
    if (!mu) { mStatus = "Selecione um musculo para espelhar"; return; }
    snapshot();
    Muscle mir = *mu;
    mir.name = mirrorName(mu->name);
    mir.side = (mu->side == "R") ? "L" : (mu->side == "L" ? "R" : mu->side);
    if (!mu->antagonist.empty()) mir.antagonist = mirrorName(mu->antagonist);
    for (auto& w : mir.waypoints) { w.p[0] = -w.p[0]; w.body = mirrorName(w.body); }
    // replace existing mirror if present, else append
    if (Muscle* existing = mModel.findMuscle(mir.name)) *existing = mir;
    else mModel.muscles.push_back(mir);
    mStatus = "Espelhado -> " + mir.name;
}
void App::duplicateSelectedMuscle() {
    if (Muscle* mu = selMuscle()) {
        snapshot();
        Muscle copy = *mu; copy.name += "_copy";
        mModel.muscles.push_back(copy);
        mSel = {SelType::Muscle, (int)mModel.muscles.size()-1, -1};
        mStatus = "Musculo duplicado";
    }
}

} // namespace mass
