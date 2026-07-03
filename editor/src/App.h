#pragma once
#include "MassModel.h"
#include "Camera.h"
#include "Renderer.h"
#include "SimBridge.h"
#include "TrainBridge.h"
#include <vector>
#include <string>
#include <deque>
#include <map>

struct GLFWwindow;

namespace mass {

// per-node visual mesh (OBJ triangle soup) in rest world space
struct MeshData { std::vector<V3> pos, nrm; unsigned gpuId = 0; std::string objName; };

enum class SelType { None, Body, Joint, Muscle, Waypoint, Light };
struct Selection {
    SelType type = SelType::None;
    int index = -1;   // node or muscle index
    int sub   = -1;   // waypoint index (when Waypoint)
    void clear(){ type = SelType::None; index = -1; sub = -1; }
};

class App {
public:
    bool init(GLFWwindow* win);
    void frame();           // one UI+render frame
    void shutdown();
    void loadProjectPath(const std::string& path); // load a specific .mass

private:
    GLFWwindow* mWin = nullptr;
    Model mModel;
    Camera mCam;
    Renderer mRen;
    Selection mSel;
    std::string mProjectPath;      // current .mass path
    std::string mStatus = "Pronto";
    bool mDrawGrid = true;
    bool mShowMuscles = true;
    bool mShowBones = true;
    bool mShowJoints = false;
    bool mShowWaypoints = false;
    bool mShowLightMarkers = true;
    bool mShowMesh = true;   // render OBJ body meshes instead of boxes

    std::vector<MeshData> mMeshes;  // per-node visual meshes (OBJ), rest world space
    void loadMeshes();
    void syncMeshes();   // reload meshes when the model's obj/type/count changes

    // live simulation
    SimBridge mSim;
    bool mSimActive = false;
    float mActivation = 0.0f;
    std::string mDataRoot = ".";
    std::map<std::string, Transform> mLivePose;
    void toggleSim();
    M4 liveBodyMatrix(const Node& n) const;   // live if simulating, else rest
    V3 liveWaypoint(const Waypoint& w) const;

    // viewport rect (for picking + gizmo)
    float mVpX=0, mVpY=0, mVpW=1, mVpH=1;
    int mGizmoOp = 7;              // ImGuizmo TRANSLATE default
    int mGizmoMode = 1;           // WORLD

    // undo/redo (full-model snapshots)
    std::deque<Model> mUndo, mRedo;
    void snapshot();              // push current state to undo
    void undo(); void redo();

    // scene
    void drawScene();
    void drawGizmo();
    V3 worldOfWaypoint(const Waypoint& w) const;
    M4 nodeBodyMatrix(const Node& n) const;
    V3 lightHandle(const Light& L) const;   // 3D position of a light's handle

    // picking
    void pickAt(double mx, double my);

    // anatomy atlas (.osim)
    std::vector<AtlasEntry> mAtlas;
    char mAtlasFilter[64] = "";

    // panels
    void drawMenuBar();
    void drawToolbar();
    void drawTree();
    void drawProperties();
    void drawValidation();
    void drawAtlas();
    void drawTrain();
    void drawLights();

    // scene lighting
    int mSelLight = -1;
    void seedLightsIfEmpty();

    // training telemetry bridge (asio)
    TrainBridge mTrain;
    void startTraining();

    // anatomy actions
    void importOsim();
    void applyAtlas(const AtlasEntry& a);
    void mirrorSelectedMuscle();

    // actions
    void newModel();
    void openMass();
    void saveMass(bool as);
    void bootstrap();
    void exportLegacy();
    void addBody();
    void removeSelected();
    void duplicateSelectedMuscle();

    // helpers
    Node* selNode();
    Muscle* selMuscle();
};

} // namespace mass
