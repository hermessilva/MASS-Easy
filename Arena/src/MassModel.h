#pragma once
// In-memory representation of a unified MASS musculoskeletal project (.mass).
// Holds skeleton + muscles + motions + training config + anatomical metadata.
// Load/Save as JSON; Bootstrap/Export bridge the legacy human.xml/muscle284.xml.
#include <string>
#include <vector>
#include <array>
#include <optional>

namespace mass {

using Vec3 = std::array<double, 3>;
using Vec4 = std::array<double, 4>;
using Mat3 = std::array<double, 9>; // row-major

struct Transform {
    Mat3 linear{1,0,0, 0,1,0, 0,0,1};
    Vec3 translation{0,0,0};
};

// ---- Skeleton ----
struct Body {
    std::string type = "Box";   // Box | Sphere | Capsule
    double mass = 1.0;
    Vec3 size{0.1,0.1,0.1};      // Box
    double radius = 0.05;        // Sphere/Capsule
    double height = 0.1;         // Capsule
    std::string obj;             // visual mesh file (optional, "" = none)
    bool contact = false;
    Vec4 color{0.2,0.2,0.2,1.0};
    Transform t;                 // world transform of the body
};

struct Joint {
    std::string type = "Ball";   // Free | Ball | Revolute | Weld | Planar
    std::string bvh;             // mapped BVH channel name (optional)
    Vec3 lower{-2,-2,-2};        // Ball/Revolute limits (Revolute uses [0])
    Vec3 upper{ 2, 2, 2};
    Vec3 axis{1,0,0};            // Revolute
    Transform t;                 // world transform of the joint
};

struct Node {
    std::string id;              // unique name
    std::string parent;          // parent id, "" = root (None)
    bool endeffector = false;    // reward end-effector flag (human.xml endeffector="True")
    Body body;
    Joint joint;
};

// ---- Muscles ----
struct Waypoint {
    std::string body;            // anchoring body id
    Vec3 p{0,0,0};               // world position (as stored in muscle284.xml)
};

struct Muscle {
    std::string name;
    // runtime Hill parameters (exported to muscle284.xml)
    double f0 = 1000.0;          // max isometric force (N)
    double lm = 1.0;             // optimal fiber length (normalized in MASS)
    double lt = 0.2;             // tendon slack length
    double pen_angle = 0.0;      // pennation angle (rad)
    double lmax = -0.1;          // MASS lmax field
    std::vector<Waypoint> waypoints;
    // ---- anatomical metadata (editor-only, not exported to training) ----
    std::string latin;
    std::string pt;              // Portuguese name
    std::string group;           // functional group (e.g. knee_extensor)
    std::string side;            // "L" | "R" | "" (midline)
    std::string antagonist;      // name of antagonist muscle
    double pcsa_cm2 = 0.0;       // physiological cross-sectional area
};

// ---- Motion ----
struct Motion {
    std::string name;
    std::string bvh;             // relative path under data/
    bool cyclic = true;
};

// ---- Training ----
struct Training {
    bool use_muscle = true;
    int con_hz = 30;
    int sim_hz = 600;
    Vec4 reward_param{0.75, 0.1, 0.0, 0.15};
    std::string default_motion;  // motion name used for metadata bvh_file
};

struct Meta {
    std::string name = "human";
    std::string unit = "m";
    double specific_tension_N_cm2 = 60.0;
};

// Scene light (editor-only, persisted in .mass, not exported to training).
struct Light {
    std::string name = "Luz";
    int type = 0;                    // 0 = directional, 1 = point
    Vec3 dir{0.4, 0.9, 0.5};         // direction (directional) or position (point)
    Vec3 color{1, 1, 1};
    double intensity = 1.0;
    bool enabled = true;
};

// Reference muscle parameters imported from an OpenSim .osim (atlas).
struct AtlasEntry {
    std::string name;
    double f0 = 0, lm = 0, lt = 0, pen_angle = 0;
    std::string origin_body, insertion_body;
};

struct Model {
    int version = 1;
    Meta meta;
    std::vector<Node> skeleton;
    std::vector<Muscle> muscles;
    std::vector<Motion> motions;
    Training training;
    std::vector<Light> lights;       // scene lighting (editor)
    double ambient = 0.25;           // global ambient term

    // lookups
    Node* findNode(const std::string& id);
    const Node* findNode(const std::string& id) const;
    Muscle* findMuscle(const std::string& name);

    // JSON project (.mass)
    static std::optional<Model> LoadMass(const std::string& path, std::string* err = nullptr);
    bool SaveMass(const std::string& path, std::string* err = nullptr) const;
};

} // namespace mass
