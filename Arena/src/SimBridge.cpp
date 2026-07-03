#include "SimBridge.h"
#include "Bootstrap.h"

#include "Character.h"
#include "BVH.h"
#include "Muscle.h"
#include "DARTHelper.h"
#include "dart/dart.hpp"
#include "dart/collision/bullet/bullet.hpp"

#include <chrono>
#include <cmath>

namespace mass {

SimBridge::~SimBridge() { stop(); }

void SimBridge::configure(const std::string& dataRoot, const std::string& tmpDir) {
    mDataRoot = dataRoot; mTmpDir = tmpDir;
}
void SimBridge::setModel(const Model& m) {
    std::lock_guard<std::mutex> lk(mMx);
    mPending = m;
    mRebuild.store(true);
}
void SimBridge::start() {
    if (mRunning.load()) return;
    mStop.store(false);
    mRunning.store(true);   // set before spawn to avoid startup race
    mThread = std::thread(&SimBridge::threadMain, this);
}
void SimBridge::stop() {
    mStop.store(true);
    if (mThread.joinable()) mThread.join();
    mRunning.store(false);
}
std::map<std::string, Transform> SimBridge::pose() {
    std::lock_guard<std::mutex> lk(mMx); return mPose;
}
std::string SimBridge::status() {
    std::lock_guard<std::mutex> lk(mMx); return mStatus;
}

static Transform toTransform(const Eigen::Isometry3d& T) {
    Transform t;
    const Eigen::Matrix3d& R = T.linear();
    t.linear = { R(0,0),R(0,1),R(0,2), R(1,0),R(1,1),R(1,2), R(2,0),R(2,1),R(2,2) };
    t.translation = { T.translation().x(), T.translation().y(), T.translation().z() };
    return t;
}

void SimBridge::threadMain() {
    using namespace dart::dynamics;
    using namespace dart::simulation;

    MASS::Character* character = nullptr;
    WorldPtr world;
    bool useMuscle = false;
    double dt = 1.0 / 600.0;
    double conDt = 1.0 / 30.0;
    double t = 0.0;

    auto setStatus = [&](const std::string& s){ std::lock_guard<std::mutex> lk(mMx); mStatus = s; };
    auto publish = [&](){
        if (!character) return;
        auto skel = character->GetSkeleton();
        std::map<std::string, Transform> snap;
        for (size_t i = 0; i < skel->getNumBodyNodes(); i++) {
            BodyNode* bn = skel->getBodyNode(i);
            snap[bn->getName()] = toTransform(bn->getTransform());
        }
        std::lock_guard<std::mutex> lk(mMx); mPose = std::move(snap);
    };

    auto rebuild = [&]() {
        Model m;
        { std::lock_guard<std::mutex> lk(mMx); m = mPending; }
        // export to temp legacy files
        std::string err;
        if (!ExportToLegacy(m, mTmpDir, &err)) { setStatus("tmp export failed: " + err); return false; }
        if (character) { delete character; character = nullptr; }
        try {
            character = new MASS::Character();
            character->LoadSkeleton(mTmpDir + "/human.xml", false);
            useMuscle = m.training.use_muscle && !m.muscles.empty();
            if (useMuscle) character->LoadMuscles(mTmpDir + "/muscle284.xml");
            // BVH
            std::string bvh;
            for (auto& mo : m.motions) if (mo.name == m.training.default_motion) bvh = mo.bvh;
            if (bvh.empty() && !m.motions.empty()) bvh = m.motions.front().bvh;
            bool cyclic = true;
            if (!bvh.empty()) character->LoadBVH(mDataRoot + bvh, cyclic);
            double kp = 300.0; character->SetPDParameters(kp, std::sqrt(2 * kp));

            dt = 1.0 / std::max(1, m.training.sim_hz);
            conDt = 1.0 / std::max(1, m.training.con_hz);

            world = std::make_shared<World>();
            world->setGravity(Eigen::Vector3d(0, -9.8, 0));
            world->setTimeStep(dt);
            world->getConstraintSolver()->setCollisionDetector(
                dart::collision::BulletCollisionDetector::create());
            world->addSkeleton(character->GetSkeleton());
            auto ground = MASS::BuildFromFile(mDataRoot + "/data/ground.xml", false);
            if (ground) world->addSkeleton(ground);

            t = 0.0;
            character->Reset();
            auto pv = character->GetTargetPosAndVel(t, conDt);
            character->GetSkeleton()->setPositions(pv.first);
            character->GetSkeleton()->setVelocities(pv.second);
            character->GetSkeleton()->computeForwardKinematics(true, false, false);
            setStatus("simulating");
        } catch (const std::exception& e) {
            setStatus(std::string("build failed: ") + e.what());
            if (character) { delete character; character = nullptr; }
            return false;
        }
        return true;
    };

    auto clockLast = std::chrono::steady_clock::now();
    double accumulator = 0.0;

    while (!mStop.load()) {
        if (mRebuild.exchange(false)) { rebuild(); clockLast = std::chrono::steady_clock::now(); accumulator = 0.0; }
        if (!character) { std::this_thread::sleep_for(std::chrono::milliseconds(30)); clockLast = std::chrono::steady_clock::now(); continue; }

        // real elapsed time drives the sim (stable, real-time), clamped against stalls
        auto now = std::chrono::steady_clock::now();
        double realDt = std::chrono::duration<double>(now - clockLast).count();
        clockLast = now;
        if (realDt > 0.1) realDt = 0.1;

        if (mReset.exchange(false)) {
            t = 0.0; accumulator = 0.0;
            auto pv = character->GetTargetPosAndVel(t, conDt);
            character->GetSkeleton()->setPositions(pv.first);
            character->GetSkeleton()->setVelocities(pv.second);
            character->GetSkeleton()->computeForwardKinematics(true, false, false);
        }

        if (!mPaused.load()) {
            try {
                if (mMode.load() == Kinematic) {
                    t += realDt;   // real-time playback
                    auto pv = character->GetTargetPosAndVel(t, conDt);
                    Eigen::VectorXd p = pv.first;
                    // treadmill: keep root over the origin so it walks in place (not off the floor).
                    // DART FreeJoint dofs: [0..2]=rotation, [3..5]=translation(x,y,z). Zero x,z.
                    if (p.rows() >= 6) { p[3] = 0.0; p[5] = 0.0; }
                    character->GetSkeleton()->setPositions(p);
                    character->GetSkeleton()->computeForwardKinematics(true, false, false);
                } else { // Dynamic — fixed-timestep accumulator for stability
                    accumulator += realDt;
                    int steps = 0;
                    while (accumulator >= dt && steps < 20) {
                        if (useMuscle) {
                            float a = mActivation.load();
                            for (auto muscle : character->GetMuscles()) {
                                muscle->activation = a;
                                muscle->Update();
                                muscle->ApplyForceToBody();
                            }
                        }
                        world->step();
                        accumulator -= dt; steps++;
                    }
                    t = world->getTime();
                }
            } catch (...) { setStatus("step failed (NaN?) - paused"); mPaused.store(true); }
        }
        mSimTime.store(t);
        publish();
        std::this_thread::sleep_for(std::chrono::milliseconds(4)); // ~ up to 250 fps of sim publish
    }

    if (character) delete character;
    setStatus("stopped");
    mRunning.store(false);
}

} // namespace mass
