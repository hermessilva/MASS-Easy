#include "App.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cstring>
#include <cmath>
#include <cctype>

namespace mass {

// small helpers for editing arrays
static bool editVec3(const char* label, Vec3& v, float step = 0.01f) {
    float f[3] = {(float)v[0], (float)v[1], (float)v[2]};
    if (ImGui::DragFloat3(label, f, step)) { v = {f[0], f[1], f[2]}; return true; }
    return false;
}
static bool editText(const char* label, std::string& s) {
    char buf[256]; std::strncpy(buf, s.c_str(), sizeof(buf)-1); buf[sizeof(buf)-1]=0;
    if (ImGui::InputText(label, buf, sizeof(buf))) { s = buf; return true; }
    return false;
}

void App::drawMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) newModel();
            if (ImGui::MenuItem("Open .mass...", "Ctrl+O")) openMass();
            if (ImGui::MenuItem("Save", "Ctrl+S")) saveMass(false);
            if (ImGui::MenuItem("Save As...")) saveMass(true);
            ImGui::Separator();
            if (ImGui::MenuItem("Import from metadata.txt (bootstrap)")) bootstrap();
            if (ImGui::MenuItem("Export for training (human.xml/muscle284.xml/metadata)")) exportLegacy();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) redo();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Anatomy")) {
            if (ImGui::MenuItem("Import OpenSim atlas (.osim)...")) importOsim();
            if (ImGui::MenuItem("Mirror muscle L<->R")) mirrorSelectedMuscle();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Floor", nullptr, &mDrawGrid);
            ImGui::MenuItem("Muscles", nullptr, &mShowMuscles);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void App::drawToolbar() {
    ImGui::Begin("Tools");
    ImGui::Text("Gizmo:");
    if (ImGui::RadioButton("Move", mGizmoOp == 7)) mGizmoOp = 7;    // TRANSLATE
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", mGizmoOp == 56)) mGizmoOp = 56; // ROTATE
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", mGizmoOp == 896)) mGizmoOp = 896;  // SCALE
    if (ImGui::RadioButton("World", mGizmoMode == 1)) mGizmoMode = 1;
    ImGui::SameLine();
    if (ImGui::RadioButton("Local", mGizmoMode == 0)) mGizmoMode = 0;
    ImGui::Separator();
    if (ImGui::Button("+ Body")) addBody();
    ImGui::SameLine();
    if (ImGui::Button("Remove")) removeSelected();
    if (ImGui::Button("Duplicate muscle")) duplicateSelectedMuscle();
    ImGui::SameLine();
    if (ImGui::Button("Mirror L<->R")) mirrorSelectedMuscle();

    ImGui::SeparatorText("DART simulation (live)");
    if (ImGui::Button(mSimActive ? "Stop" : "Simulate")) toggleSim();
    if (mSimActive) {
        ImGui::SameLine();
        if (ImGui::Button("Reset")) mSim.requestReset();
        int md = mSim.mode();
        if (ImGui::RadioButton("Kinematic (BVH)", md == SimBridge::Kinematic)) mSim.setMode(SimBridge::Kinematic);
        ImGui::SameLine();
        if (ImGui::RadioButton("Dynamic (physics)", md == SimBridge::Dynamic)) mSim.setMode(SimBridge::Dynamic);
        bool paused = mSim.paused();
        if (ImGui::Checkbox("Pause", &paused)) mSim.setPaused(paused);
        if (ImGui::SliderFloat("Muscle activation", &mActivation, 0.0f, 1.0f)) mSim.setActivation(mActivation);
        ImGui::Text("t = %.2fs  |  %s", mSim.simTime(), mSim.status().c_str());
    }
    ImGui::End();
}

void App::drawTree() {
    ImGui::Begin("Scene");
    if (ImGui::CollapsingHeader("Visibility", ImGuiTreeNodeFlags_DefaultOpen)) {
        struct { const char* label; bool* v; } items[] = {
            {"Bones##vis", &mShowBones}, {"Muscles##vis", &mShowMuscles},
            {"Joints##vis", &mShowJoints}, {"Waypoints##vis", &mShowWaypoints},
            {"Lights##vis", &mShowLightMarkers}, {"Floor##vis", &mDrawGrid},
            {"Meshes##vis", &mShowMesh}, {"Muscle vol##vis", &mMuscleVolume},
            {"Skin##vis", &mShowSkin},
        };
        const int n = (int)(sizeof(items)/sizeof(items[0]));
        ImGuiStyle& st = ImGui::GetStyle();
        float rightX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        for (int i = 0; i < n; i++) {
            ImGui::Checkbox(items[i].label, items[i].v);
            if (i + 1 < n) {
                float nextW = ImGui::GetFrameHeight() + st.ItemInnerSpacing.x
                            + ImGui::CalcTextSize(items[i+1].label, nullptr, true).x;
                float lastX2 = ImGui::GetItemRectMax().x;
                if (lastX2 + st.ItemSpacing.x + nextW < rightX) ImGui::SameLine();
            }
        }
    }
    if (ImGui::CollapsingHeader("Skeleton", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < (int)mModel.skeleton.size(); i++) {
            const Node& n = mModel.skeleton[i];
            bool sel = ((mSel.type==SelType::Body||mSel.type==SelType::Joint) && mSel.index==i);
            ImGui::PushID(i);
            if (ImGui::Selectable((n.id + "  [" + n.joint.type + "]").c_str(), sel))
                mSel = {SelType::Body, i, -1};
            if (ImGui::IsItemClicked(1)) mSel = {SelType::Joint, i, -1};
            ImGui::PopID();
        }
    }
    if (ImGui::CollapsingHeader("Muscles")) {
        ImGui::Text("%d muscles", (int)mModel.muscles.size());
        ImGui::BeginChild("mlist", ImVec2(0, 300));
        for (int i = 0; i < (int)mModel.muscles.size(); i++) {
            bool sel = ((mSel.type==SelType::Muscle||mSel.type==SelType::Waypoint) && mSel.index==i);
            ImGui::PushID(1000+i);
            if (ImGui::Selectable(mModel.muscles[i].name.c_str(), sel))
                mSel = {SelType::Muscle, i, -1};
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

void App::drawProperties() {
    ImGui::Begin("Properties");
    if (Node* n = selNode()) {
        ImGui::TextColored(ImVec4(1,0.8f,0.2f,1), "Node: %s", n->id.c_str());
        editText("id", n->id);
        editText("parent", n->parent);
        ImGui::Checkbox("end-effector", &n->endeffector);
        ImGui::SeparatorText("Body");
        const char* btypes[] = {"Box","Sphere","Capsule"};
        int bcur = 0; for (int k=0;k<3;k++) if (n->body.type==btypes[k]) bcur=k;
        if (ImGui::Combo("body type", &bcur, btypes, 3)) n->body.type = btypes[bcur];
        float mass = (float)n->body.mass;
        if (ImGui::DragFloat("mass (kg)", &mass, 0.1f, 0.01f, 500.0f)) n->body.mass = mass;
        if (n->body.type == "Box") editVec3("size", n->body.size);
        else { float r=(float)n->body.radius; if(ImGui::DragFloat("radius",&r,0.005f)) n->body.radius=r; }
        ImGui::Checkbox("contact", &n->body.contact);
        float col[4]={(float)n->body.color[0],(float)n->body.color[1],(float)n->body.color[2],(float)n->body.color[3]};
        if (ImGui::ColorEdit4("color", col)) n->body.color={col[0],col[1],col[2],col[3]};
        editText("obj", n->body.obj);
        editVec3("body pos", n->body.t.translation);
        ImGui::SeparatorText("Joint");
        const char* types[] = {"Free","Ball","Revolute","Weld","Planar"};
        int cur = 1;
        for (int k=0;k<5;k++) if (n->joint.type==types[k]) cur=k;
        if (ImGui::Combo("joint type", &cur, types, 5)) n->joint.type = types[cur];
        editText("bvh", n->joint.bvh);
        if (n->joint.type=="Ball") { editVec3("lower", n->joint.lower); editVec3("upper", n->joint.upper); }
        else if (n->joint.type=="Revolute") {
            editVec3("axis", n->joint.axis, 0.001f);
            float lo=(float)n->joint.lower[0], hi=(float)n->joint.upper[0];
            if(ImGui::DragFloat("lower",&lo,0.01f)) n->joint.lower[0]=lo;
            if(ImGui::DragFloat("upper",&hi,0.01f)) n->joint.upper[0]=hi;
        }
        editVec3("joint pos", n->joint.t.translation);
    } else if (Muscle* mu = selMuscle()) {
        ImGui::TextColored(ImVec4(1,1,0.3f,1), "Muscle: %s", mu->name.c_str());
        editText("name", mu->name);
        ImGui::SeparatorText("Hill");
        float f0=(float)mu->f0, lm=(float)mu->lm, lt=(float)mu->lt, pa=(float)mu->pen_angle;
        if(ImGui::DragFloat("f0 (N)",&f0,1.0f,0.0f,5000.0f)) mu->f0=f0;
        if(ImGui::DragFloat("lm",&lm,0.001f)) mu->lm=lm;
        if(ImGui::DragFloat("lt",&lt,0.001f)) mu->lt=lt;
        if(ImGui::DragFloat("pen_angle",&pa,0.001f)) mu->pen_angle=pa;
        float pcsa=(float)mu->pcsa_cm2;
        if(ImGui::DragFloat("PCSA (cm2)",&pcsa,0.1f,0.0f,300.0f)) {
            mu->pcsa_cm2=pcsa; mu->f0 = pcsa * mModel.meta.specific_tension_N_cm2; // f0 = sigma*PCSA
        }
        ImGui::SeparatorText("Anatomy");
        editText("latin", mu->latin);
        editText("pt", mu->pt);
        editText("group", mu->group);
        editText("side (L/R)", mu->side);
        editText("antagonist", mu->antagonist);
        ImGui::SeparatorText("Waypoints");
        for (int k=0;k<(int)mu->waypoints.size();k++) {
            ImGui::PushID(k);
            bool selW=(mSel.type==SelType::Waypoint && mSel.sub==k);
            if (ImGui::Selectable(("wp "+std::to_string(k)+" @"+mu->waypoints[k].body).c_str(), selW))
                mSel={SelType::Waypoint, mSel.index, k};
            ImGui::PopID();
        }
    } else {
        ImGui::TextDisabled("Nothing selected.");
        ImGui::TextWrapped("Click a bone, a joint (right-click in the tree) or a muscle.");
    }
    ImGui::End();
}

void App::drawValidation() {
    ImGui::Begin("Anatomy check");
    int problems = 0;
    for (const auto& mu : mModel.muscles) {
        if (mu.waypoints.size() < 2) { ImGui::TextColored(ImVec4(1,0.4f,0.3f,1), "%s: <2 waypoints", mu.name.c_str()); problems++; }
        for (const auto& w : {mu.waypoints.front(), mu.waypoints.back()}) {
            if (mModel.findNode(w.body) == nullptr) {
                ImGui::TextColored(ImVec4(1,0.4f,0.3f,1), "%s: body '%s' missing", mu.name.c_str(), w.body.c_str()); problems++;
            }
        }
        double fmax = mu.pcsa_cm2 * mModel.meta.specific_tension_N_cm2;
        if (mu.pcsa_cm2 > 0 && std::fabs(fmax - mu.f0) > 1.0)
            { ImGui::TextColored(ImVec4(0.9f,0.8f,0.2f,1), "%s: f0 != sigma*PCSA", mu.name.c_str()); problems++; }
    }
    if (problems == 0) ImGui::TextColored(ImVec4(0.3f,1,0.4f,1), "No problems detected.");
    ImGui::End();
}

void App::drawAtlas() {
    ImGui::Begin("Atlas (OpenSim)");
    if (ImGui::Button("Import .osim...")) importOsim();
    ImGui::SameLine();
    ImGui::TextDisabled("%d refs", (int)mAtlas.size());
    ImGui::InputText("filter", mAtlasFilter, sizeof(mAtlasFilter));
    ImGui::Separator();
    ImGui::BeginChild("atlaslist");
    std::string flt = mAtlasFilter;
    for (auto& c : flt) c = (char)tolower(c);
    for (int i = 0; i < (int)mAtlas.size(); i++) {
        const AtlasEntry& a = mAtlas[i];
        if (!flt.empty()) {
            std::string ln = a.name; for (auto& c : ln) c = (char)tolower(c);
            if (ln.find(flt) == std::string::npos) continue;
        }
        ImGui::PushID(i);
        ImGui::Text("%s", a.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("f0=%.0f pen=%.2f", a.f0, a.pen_angle);
        ImGui::SameLine();
        if (ImGui::SmallButton("apply")) applyAtlas(a);
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
}

void App::drawTrain() {
    ImGui::Begin("Training (PPO)");
    ImGui::Text("Telemetry: %s", mTrain.status().c_str());
    ImGui::Text("Client: %s", mTrain.clientConnected() ? "CONNECTED" : "waiting...");
    ImGui::Separator();
    Telemetry t = mTrain.latest();
    ImGui::Text("Iteration : %d", t.iteration);
    ImGui::Text("Reward    : %.3f", t.reward);
    ImGui::Text("Loss A/C/M: %.3f / %.4f / %.3f", t.loss_actor, t.loss_critic, t.loss_muscle);
    auto rh = mTrain.rewardHistory();
    if (!rh.empty())
        ImGui::PlotLines("reward", rh.data(), (int)rh.size(), 0, nullptr, 0.0f, 1.0f, ImVec2(0, 120));
    ImGui::Separator();
    if (!mTrain.trainingRunning()) {
        if (ImGui::Button("Export to data + train")) startTraining();
        ImGui::TextDisabled("(overwrites data/human.xml, muscle284.xml, metadata.txt)");
    } else {
        if (ImGui::Button("Stop training")) mTrain.stopTraining();
    }
    ImGui::End();
}

void App::drawSkinPanel() {
    ImGui::Begin("Skin");
    ImGui::TextWrapped("Continuous skin generated by inference (metaballs + marching cubes) over the anatomy.");
    ImGui::Checkbox("Show skin", &mShowSkin);
    ImGui::SeparatorText("Parameters (inference)");
    ImGui::SliderFloat("Thickness (m)", &mSkinParams.inflate, 0.0f, 0.06f, "%.3f");
    ImGui::SliderFloat("Fusion smoothness", &mSkinParams.smooth, 0.0f, 0.15f, "%.3f");
    ImGui::SliderFloat("Body scale", &mSkinParams.bodyScale, 0.8f, 1.6f, "%.2f");
    ImGui::SliderFloat("Resolution (m/cell)", &mSkinParams.cell, 0.008f, 0.04f, "%.3f");
    ImGui::TextDisabled("smaller = more detail, slower");
    ImGui::Separator();
    if (ImGui::Button("Generate skin", ImVec2(-1, 0))) regenerateSkin();
    ImGui::TextDisabled("Tip: turn off Bones/Muscles to see only the skin.");
    ImGui::End();
}

void App::drawLights() {
    ImGui::Begin("Lights");
    float amb = (float)mModel.ambient;
    if (ImGui::SliderFloat("Ambient", &amb, 0.0f, 2.0f)) mModel.ambient = amb;
    ImGui::Separator();
    if (ImGui::Button("+ Directional")) {
        Light l; l.name = "Directional " + std::to_string(mModel.lights.size());
        l.type = 0; l.dir = {0.3, 0.8, 0.4}; mModel.lights.push_back(l);
        mSelLight = (int)mModel.lights.size()-1; mSel = {SelType::Light, mSelLight, -1};
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Point")) {
        Light l; l.name = "Point " + std::to_string(mModel.lights.size());
        l.type = 1; l.dir = {0.0, 2.0, 1.5}; mModel.lights.push_back(l);
        mSelLight = (int)mModel.lights.size()-1; mSel = {SelType::Light, mSelLight, -1};
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove") && mSelLight >= 0 && mSelLight < (int)mModel.lights.size()) {
        mModel.lights.erase(mModel.lights.begin() + mSelLight); mSelLight = -1;
    }
    ImGui::Separator();
    for (int i = 0; i < (int)mModel.lights.size(); i++) {
        ImGui::PushID(i);
        Light& L = mModel.lights[i];
        bool en = L.enabled;
        if (ImGui::Checkbox("##en", &en)) L.enabled = en;
        ImGui::SameLine();
        if (ImGui::Selectable((L.name + (L.type ? "  (point)" : "  (dir)")).c_str(), mSelLight == i)) {
            mSelLight = i; mSel = {SelType::Light, i, -1};
        }
        ImGui::PopID();
    }
    if (mSelLight >= 0 && mSelLight < (int)mModel.lights.size()) {
        Light& L = mModel.lights[mSelLight];
        ImGui::SeparatorText("Light properties");
        char buf[128]; std::strncpy(buf, L.name.c_str(), sizeof(buf)-1); buf[sizeof(buf)-1]=0;
        if (ImGui::InputText("name", buf, sizeof(buf))) L.name = buf;
        int ty = L.type;
        const char* tys[] = {"Directional", "Point"};
        if (ImGui::Combo("type", &ty, tys, 2)) L.type = ty;
        ImGui::Checkbox("enabled", &L.enabled);
        float col[3] = {(float)L.color[0], (float)L.color[1], (float)L.color[2]};
        if (ImGui::ColorEdit3("color", col)) L.color = {col[0], col[1], col[2]};
        float inten = (float)L.intensity;
        if (ImGui::SliderFloat("intensity", &inten, 0.0f, 4.0f)) L.intensity = inten;
        float d[3] = {(float)L.dir[0], (float)L.dir[1], (float)L.dir[2]};
        if (ImGui::DragFloat3(L.type ? "position" : "direction", d, 0.02f)) L.dir = {d[0], d[1], d[2]};
    }
    ImGui::End();
}

// ---- main frame ----
void App::frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    // fullscreen dockspace host
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGuiWindowFlags host = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoBackground;  // let the passthru central node reveal the 3D backbuffer
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::Begin("DockHost", nullptr, host);
    ImGui::PopStyleVar(3);
    drawMenuBar();
    ImGuiID dockId = ImGui::GetID("MassDock");
    ImGui::DockSpace(dockId, ImVec2(0,0), ImGuiDockNodeFlags_PassthruCentralNode);

    // build a sensible default layout on first run
    static bool builtLayout = false;
    if (!builtLayout) {
        builtLayout = true;
        ImGui::DockBuilderRemoveNode(dockId);
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockId, ImGui::GetMainViewport()->WorkSize);
        ImGuiID left, right, center = dockId;
        left  = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left,  0.20f, nullptr, &center);
        right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.26f, nullptr, &center);
        ImGuiID leftBottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.55f, nullptr, &left);
        ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.5f, nullptr, &right);
        ImGui::DockBuilderDockWindow("Scene", left);
        ImGui::DockBuilderDockWindow("Properties", leftBottom);
        ImGui::DockBuilderDockWindow("Tools", right);
        ImGui::DockBuilderDockWindow("Training (PPO)", rightBottom);
        ImGui::DockBuilderDockWindow("Atlas (OpenSim)", rightBottom);
        ImGui::DockBuilderDockWindow("Anatomy check", rightBottom);
        ImGui::DockBuilderDockWindow("Lights", leftBottom);
        ImGui::DockBuilderDockWindow("Skin", rightBottom);
        ImGui::DockBuilderDockWindow("Viewport", center);
        ImGui::DockBuilderFinish(dockId);
    }
    ImGui::End();

    drawToolbar();
    drawTree();
    drawProperties();
    drawValidation();
    drawAtlas();
    drawTrain();
    drawLights();
    drawSkinPanel();

    // viewport window: render 3D to FBO, then show as an image (no compositing issues)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::Begin("Viewport");
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    if (size.x < 1) size.x = 1; if (size.y < 1) size.y = 1;
    mVpX = pos.x; mVpY = pos.y; mVpW = size.x; mVpH = size.y;

    drawScene();  // renders into the offscreen texture at (mVpW x mVpH)
    ImGui::Image((ImTextureID)(intptr_t)mRen.targetTexture(), size, ImVec2(0,1), ImVec2(1,0));

    bool hovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();
    if (hovered && !ImGuizmo::IsUsing()) {
        bool orbiting = ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !ImGuizmo::IsOver();
        // pivot orbit/zoom around the selected element's world point
        if (mSel.type != SelType::None && (io.MouseWheel != 0 || orbiting))
            mCam.target = selectionCenter();
        if (io.MouseWheel != 0) mCam.zoom(io.MouseWheel);
        if (orbiting)
            mCam.orbit(io.MouseDelta.x, io.MouseDelta.y);
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
            mCam.pan(io.MouseDelta.x, io.MouseDelta.y);
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGuizmo::IsOver()) {
            ImVec2 dr = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
            if (std::fabs(dr.x) < 3 && std::fabs(dr.y) < 3)
                pickAt(io.MousePos.x, io.MousePos.y);
        }
    }
    drawGizmo();
    ImGui::End();
    ImGui::PopStyleVar();

    // keyboard shortcuts
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) undo();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) redo();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) saveMass(false);
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) openMass();
    if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_1) || ImGui::IsKeyPressed(ImGuiKey_Keypad1)))
        resetView();  // reset camera + pose (top-row or numpad 1)
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) removeSelected();

    // status bar
    ImGui::SetNextWindowBgAlpha(0.6f);
    if (ImGui::BeginViewportSideBar("##status", vp, ImGuiDir_Down, ImGui::GetFrameHeight(),
            ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoDocking)) {
        if (ImGui::BeginMenuBar()) { ImGui::Text("%s", mStatus.c_str()); ImGui::EndMenuBar(); }
        ImGui::End();
    }

    // ---- render (3D already rendered to FBO during the Viewport window) ----
    ImGui::Render();
    int w, h; glfwGetFramebufferSize(mWin, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace mass
