#include "Bootstrap.h"
#include <tinyxml.h>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdio>

namespace mass {

// ---------- parsing helpers ----------
static std::vector<double> splitDoubles(const std::string& s) {
    std::vector<double> out;
    std::stringstream ss(s);
    double d;
    while (ss >> d) out.push_back(d);
    return out;
}
static Vec3 parseVec3(const char* s, Vec3 def = {0,0,0}) {
    if (!s) return def; auto v = splitDoubles(s);
    return v.size() >= 3 ? Vec3{v[0], v[1], v[2]} : def;
}
static Vec4 parseVec4(const char* s, Vec4 def = {0.2,0.2,0.2,1.0}) {
    if (!s) return def; auto v = splitDoubles(s);
    return v.size() >= 4 ? Vec4{v[0], v[1], v[2], v[3]} : def;
}
static Mat3 parseMat3(const char* s) {
    Mat3 m{1,0,0, 0,1,0, 0,0,1};
    if (!s) return m; auto v = splitDoubles(s);
    for (size_t i = 0; i < 9 && i < v.size(); i++) m[i] = v[i];
    return m;
}
static Transform parseTransform(TiXmlElement* parent) {
    Transform t;
    if (!parent) return t;
    TiXmlElement* tr = parent->FirstChildElement("Transformation");
    if (!tr) return t;
    t.linear = parseMat3(tr->Attribute("linear"));
    t.translation = parseVec3(tr->Attribute("translation"));
    return t;
}

// ---------- metadata.txt ----------
struct MetaFile {
    bool use_muscle = true; int con_hz = 30, sim_hz = 600;
    std::string skel_file, muscle_file, bvh_file; bool bvh_cyclic = true;
    Vec4 reward{0.75,0.1,0.0,0.15};
};
static MetaFile parseMetadata(const std::string& path) {
    MetaFile mf; std::ifstream f(path); std::string line;
    while (std::getline(f, line)) {
        std::stringstream ss(line); std::string key; ss >> key;
        if (key == "use_muscle") { std::string v; ss >> v; mf.use_muscle = (v == "true"); }
        else if (key == "con_hz") ss >> mf.con_hz;
        else if (key == "sim_hz") ss >> mf.sim_hz;
        else if (key == "skel_file") ss >> mf.skel_file;
        else if (key == "muscle_file") ss >> mf.muscle_file;
        else if (key == "bvh_file") { std::string c; ss >> mf.bvh_file >> c; mf.bvh_cyclic = (c == "true"); }
        else if (key == "reward_param") ss >> mf.reward[0] >> mf.reward[1] >> mf.reward[2] >> mf.reward[3];
    }
    return mf;
}

// ---------- import ----------
static void loadSkeleton(const std::string& path, Model& m, std::string* err) {
    TiXmlDocument doc;
    if (!doc.LoadFile(path.c_str())) { if (err) *err = "cannot load skeleton " + path; return; }
    TiXmlElement* skel = doc.FirstChildElement("Skeleton");
    if (!skel) { if (err) *err = "no <Skeleton> in " + path; return; }
    if (skel->Attribute("name")) m.meta.name = skel->Attribute("name");

    for (TiXmlElement* node = skel->FirstChildElement("Node"); node; node = node->NextSiblingElement("Node")) {
        Node nd;
        nd.id = node->Attribute("name") ? node->Attribute("name") : "";
        nd.parent = node->Attribute("parent") ? node->Attribute("parent") : "";
        if (nd.parent == "None") nd.parent = "";
        if (node->Attribute("endeffector"))
            nd.endeffector = std::string(node->Attribute("endeffector")) == "True";

        TiXmlElement* body = node->FirstChildElement("Body");
        if (body) {
            nd.body.type = body->Attribute("type") ? body->Attribute("type") : "Box";
            if (body->Attribute("mass")) nd.body.mass = std::stod(body->Attribute("mass"));
            if (body->Attribute("obj")) nd.body.obj = body->Attribute("obj");
            if (body->Attribute("size")) nd.body.size = parseVec3(body->Attribute("size"));
            if (body->Attribute("radius")) nd.body.radius = std::stod(body->Attribute("radius"));
            if (body->Attribute("height")) nd.body.height = std::stod(body->Attribute("height"));
            if (body->Attribute("contact")) nd.body.contact = std::string(body->Attribute("contact")) == "On";
            nd.body.color = parseVec4(body->Attribute("color"));
            nd.body.t = parseTransform(body);
        }
        TiXmlElement* joint = node->FirstChildElement("Joint");
        if (joint) {
            nd.joint.type = joint->Attribute("type") ? joint->Attribute("type") : "Ball";
            if (joint->Attribute("bvh")) nd.joint.bvh = joint->Attribute("bvh");
            if (joint->Attribute("axis")) nd.joint.axis = parseVec3(joint->Attribute("axis"), {1,0,0});
            if (nd.joint.type == "Revolute") {
                if (joint->Attribute("lower")) nd.joint.lower[0] = std::stod(joint->Attribute("lower"));
                if (joint->Attribute("upper")) nd.joint.upper[0] = std::stod(joint->Attribute("upper"));
            } else {
                if (joint->Attribute("lower")) nd.joint.lower = parseVec3(joint->Attribute("lower"), {-2,-2,-2});
                if (joint->Attribute("upper")) nd.joint.upper = parseVec3(joint->Attribute("upper"), {2,2,2});
            }
            nd.joint.t = parseTransform(joint);
        }
        m.skeleton.push_back(std::move(nd));
    }
}

static void loadMuscles(const std::string& path, Model& m, std::string* err) {
    TiXmlDocument doc;
    if (!doc.LoadFile(path.c_str())) { if (err) *err = "cannot load muscles " + path; return; }
    TiXmlElement* root = doc.FirstChildElement("Muscle");
    if (!root) { if (err) *err = "no <Muscle> in " + path; return; }
    for (TiXmlElement* unit = root->FirstChildElement("Unit"); unit; unit = unit->NextSiblingElement("Unit")) {
        Muscle x;
        x.name = unit->Attribute("name") ? unit->Attribute("name") : "";
        if (unit->Attribute("f0")) x.f0 = std::stod(unit->Attribute("f0"));
        if (unit->Attribute("lm")) x.lm = std::stod(unit->Attribute("lm"));
        if (unit->Attribute("lt")) x.lt = std::stod(unit->Attribute("lt"));
        if (unit->Attribute("pen_angle")) x.pen_angle = std::stod(unit->Attribute("pen_angle"));
        if (unit->Attribute("lmax")) x.lmax = std::stod(unit->Attribute("lmax"));
        // infer side from name prefix (L_/R_)
        if (x.name.rfind("L_", 0) == 0 || x.name.rfind("l_", 0) == 0) x.side = "L";
        else if (x.name.rfind("R_", 0) == 0 || x.name.rfind("r_", 0) == 0) x.side = "R";
        for (TiXmlElement* wp = unit->FirstChildElement("Waypoint"); wp; wp = wp->NextSiblingElement("Waypoint")) {
            Waypoint w;
            w.body = wp->Attribute("body") ? wp->Attribute("body") : "";
            w.p = parseVec3(wp->Attribute("p"));
            x.waypoints.push_back(w);
        }
        m.muscles.push_back(std::move(x));
    }
}

std::optional<Model> BootstrapFromLegacy(const std::string& metadata_path,
                                         const std::string& data_root,
                                         std::string* err) {
    MetaFile mf = parseMetadata(metadata_path);
    Model m;
    m.training.use_muscle = mf.use_muscle;
    m.training.con_hz = mf.con_hz;
    m.training.sim_hz = mf.sim_hz;
    m.training.reward_param = mf.reward;

    auto full = [&](const std::string& rel) {
        if (rel.empty()) return std::string();
        // metadata paths look like "/data/human.xml"; data_root is the project root
        return data_root + rel;
    };
    if (!mf.skel_file.empty()) loadSkeleton(full(mf.skel_file), m, err);
    if (mf.use_muscle && !mf.muscle_file.empty()) loadMuscles(full(mf.muscle_file), m, err);
    if (!mf.bvh_file.empty()) {
        Motion mo;
        // name = filename stem
        std::string b = mf.bvh_file;
        size_t slash = b.find_last_of("/\\");
        size_t dot = b.find_last_of('.');
        mo.name = b.substr(slash == std::string::npos ? 0 : slash + 1,
                           (dot == std::string::npos ? b.size() : dot) - (slash == std::string::npos ? 0 : slash + 1));
        mo.bvh = mf.bvh_file;
        mo.cyclic = mf.bvh_cyclic;
        m.motions.push_back(mo);
        m.training.default_motion = mo.name;
    }
    return m;
}

// ---------- export ----------
static std::string fmt(double d) {
    char buf[64]; std::snprintf(buf, sizeof(buf), "%.6g", d); return buf;
}
static std::string fmt(const Vec3& v) { return fmt(v[0]) + " " + fmt(v[1]) + " " + fmt(v[2]); }
static std::string fmt(const Vec4& v) { return fmt(v[0]) + " " + fmt(v[1]) + " " + fmt(v[2]) + " " + fmt(v[3]); }
static std::string fmt(const Mat3& m) {
    std::string s; for (int i = 0; i < 9; i++) { s += fmt(m[i]); if (i < 8) s += " "; } return s;
}

static bool writeSkeleton(const Model& m, const std::string& path, std::string* err) {
    std::ofstream f(path);
    if (!f) { if (err) *err = "cannot write " + path; return false; }
    f << "<Skeleton name=\"" << m.meta.name << "\">\n";
    for (const auto& n : m.skeleton) {
        f << "    <Node name=\"" << n.id << "\" parent=\"" << (n.parent.empty() ? "None" : n.parent) << "\" ";
        if (n.endeffector) f << "endeffector=\"True\" ";
        f << ">\n";
        // body
        f << "        <Body type=\"" << n.body.type << "\" mass=\"" << fmt(n.body.mass) << "\" ";
        if (n.body.type == "Box") f << "size=\"" << fmt(n.body.size) << "\" ";
        if (n.body.type == "Sphere") f << "radius=\"" << fmt(n.body.radius) << "\" ";
        if (n.body.type == "Capsule") f << "radius=\"" << fmt(n.body.radius) << "\" height=\"" << fmt(n.body.height) << "\" ";
        f << "contact=\"" << (n.body.contact ? "On" : "Off") << "\" color=\"" << fmt(n.body.color) << "\" ";
        if (!n.body.obj.empty()) f << "obj=\"" << n.body.obj << "\" ";
        f << ">\n";
        f << "            <Transformation linear=\"" << fmt(n.body.t.linear) << "\" translation=\"" << fmt(n.body.t.translation) << " \"/>\n";
        f << "        </Body>\n";
        // joint
        f << "        <Joint type=\"" << n.joint.type << "\" ";
        if (!n.joint.bvh.empty()) f << "bvh=\"" << n.joint.bvh << "\" ";
        if (n.joint.type == "Revolute") f << "axis=\"" << fmt(n.joint.axis) << "\" lower=\"" << fmt(n.joint.lower[0]) << "\" upper=\"" << fmt(n.joint.upper[0]) << "\" ";
        else if (n.joint.type == "Ball") f << "lower=\"" << fmt(n.joint.lower) << "\" upper=\"" << fmt(n.joint.upper) << "\" ";
        f << ">\n";
        f << "            <Transformation linear=\"" << fmt(n.joint.t.linear) << "\" translation=\"" << fmt(n.joint.t.translation) << " \"/>\n";
        f << "        </Joint>\n";
        f << "    </Node>\n";
    }
    f << "</Skeleton>\n";
    return true;
}

static bool writeMuscles(const Model& m, const std::string& path, std::string* err) {
    std::ofstream f(path);
    if (!f) { if (err) *err = "cannot write " + path; return false; }
    f << "<Muscle>\n";
    for (const auto& x : m.muscles) {
        f << "    <Unit name=\"" << x.name << "\" f0=\"" << fmt(x.f0) << "\" lm=\"" << fmt(x.lm)
          << "\" lt=\"" << fmt(x.lt) << "\" pen_angle=\"" << fmt(x.pen_angle) << "\" lmax=\"" << fmt(x.lmax) << "\">\n";
        for (const auto& w : x.waypoints)
            f << "        <Waypoint body=\"" << w.body << "\" p=\"" << fmt(w.p) << " \" />\n";
        f << "    </Unit>\n";
    }
    f << "</Muscle>\n";
    return true;
}

static bool writeMetadata(const Model& m, const std::string& path, std::string* err) {
    std::ofstream f(path);
    if (!f) { if (err) *err = "cannot write " + path; return false; }
    f << "use_muscle " << (m.training.use_muscle ? "true" : "false") << "\n";
    f << "con_hz " << m.training.con_hz << "\n";
    f << "sim_hz " << m.training.sim_hz << "\n";
    f << "skel_file /data/human.xml\n";
    f << "muscle_file /data/muscle284.xml\n";
    // default motion
    std::string bvh = "/data/motion/walk.bvh"; bool cyclic = true;
    for (const auto& mo : m.motions)
        if (mo.name == m.training.default_motion) { bvh = mo.bvh; cyclic = mo.cyclic; }
    f << "bvh_file " << bvh << " " << (cyclic ? "true" : "false") << "\n";
    f << "reward_param " << fmt(m.training.reward_param[0]) << " " << fmt(m.training.reward_param[1])
      << " " << fmt(m.training.reward_param[2]) << " " << fmt(m.training.reward_param[3]) << "\n";
    return true;
}

// ---------- OpenSim .osim muscle atlas import ----------
static const char* childText(TiXmlElement* e, const char* name) {
    if (!e) return nullptr;
    TiXmlElement* c = e->FirstChildElement(name);
    return (c && c->GetText()) ? c->GetText() : nullptr;
}
// Recursively collect elements that look like muscles (have <max_isometric_force>).
static void collectMuscles(TiXmlElement* e, std::vector<AtlasEntry>& out) {
    for (TiXmlElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement()) {
        if (c->FirstChildElement("max_isometric_force")) {
            AtlasEntry a;
            if (c->Attribute("name")) a.name = c->Attribute("name");
            if (auto t = childText(c, "max_isometric_force")) a.f0 = std::stod(t);
            if (auto t = childText(c, "optimal_fiber_length")) a.lm = std::stod(t);
            if (auto t = childText(c, "tendon_slack_length")) a.lt = std::stod(t);
            if (auto t = childText(c, "pennation_angle_at_optimal")) a.pen_angle = std::stod(t);
            // first/last path point parent frames = origin/insertion
            TiXmlElement* gp = c->FirstChildElement("GeometryPath");
            if (gp) {
                TiXmlElement* pps = gp->FirstChildElement("PathPointSet");
                TiXmlElement* objs = pps ? pps->FirstChildElement("objects") : nullptr;
                if (objs) {
                    TiXmlElement* first = objs->FirstChildElement();
                    TiXmlElement* last = first;
                    for (TiXmlElement* p = first; p; p = p->NextSiblingElement()) last = p;
                    auto frame = [](TiXmlElement* pp) -> std::string {
                        if (!pp) return "";
                        const char* t = childText(pp, "socket_parent_frame");
                        if (!t) t = childText(pp, "body");
                        return t ? t : "";
                    };
                    a.origin_body = frame(first);
                    a.insertion_body = frame(last);
                }
            }
            out.push_back(a);
        }
        collectMuscles(c, out); // recurse (muscles live under ForceSet/objects)
    }
}

std::vector<AtlasEntry> ImportOsimMuscles(const std::string& osim_path, std::string* err) {
    std::vector<AtlasEntry> out;
    TiXmlDocument doc;
    if (!doc.LoadFile(osim_path.c_str())) { if (err) *err = "cannot load " + osim_path; return out; }
    TiXmlElement* root = doc.RootElement();
    if (!root) { if (err) *err = "empty osim"; return out; }
    collectMuscles(root, out);
    if (out.empty() && err) *err = "nenhum musculo encontrado no .osim";
    return out;
}

bool ExportToLegacy(const Model& m, const std::string& out_dir, std::string* err) {
    std::string d = out_dir;
    if (!d.empty() && d.back() != '/' && d.back() != '\\') d += "/";
    if (!writeSkeleton(m, d + "human.xml", err)) return false;
    if (!writeMuscles(m, d + "muscle284.xml", err)) return false;
    if (!writeMetadata(m, d + "metadata.txt", err)) return false;
    return true;
}

} // namespace mass
