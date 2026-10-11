// cheats.cpp - noclip, hitboxes and TPS bypass.
//
// Noclip and TPS changes are cheats: while either is on, finishing a level does not count
// (the run is treated like a test run, so nothing is saved).
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>

#include "settings.hpp"

using namespace geode::prelude;

namespace {
// A lag spike can hand us a huge frame time; never run more than this many steps for one frame.
constexpr int MAX_STEPS_PER_FRAME = 1000;
}

// --- TPS bypass (GJBaseGameLayer) --------------------------------------------------------------
//
// How the game steps physics: update(dt) asks getModifiedDelta(dt) for a time, then splits that
// time into 1/240 s steps. So just changing the number can't give a higher tick rate.
//
// Our way: when the bypass is on and TPS is above 240, we take over update(). We turn the real
// frame time into N short steps of exactly 1/TPS and call the game's own update once per step.
// While we do that, getModifiedDelta hands the short step straight through, so the game runs
// one step of exactly 1/TPS per call.
//
// TPS below 240 isn't supported: the game always splits a step longer than 1/240 back into
// 1/240 pieces. At exactly 240 (or with the bypass off) the original code runs untouched.

class $modify(VbTpsLayer, GJBaseGameLayer) {
    struct Fields {
        double leftover = 0.0;  // real time not turned into a physics step yet
        bool stepping = false;  // true while we are running our own physics steps
    };

    void update(float dt) {
        auto& cfg = vbot::Config::get();
        auto* pl = PlayLayer::get();
        bool active = cfg.tpsBypass && cfg.tps > vbot::NORMAL_TPS && pl &&
                      static_cast<GJBaseGameLayer*>(pl) == static_cast<GJBaseGameLayer*>(this);

        if (!active) {
            m_fields->leftover = 0.0;
            GJBaseGameLayer::update(dt);
            return;
        }

        double step = 1.0 / static_cast<double>(cfg.tps);
        m_fields->leftover += static_cast<double>(dt);

        int count = static_cast<int>(m_fields->leftover / step);
        if (count > MAX_STEPS_PER_FRAME) {
            count = MAX_STEPS_PER_FRAME;
            m_fields->leftover = 0.0; // drop the backlog instead of freezing the game
        } else {
            m_fields->leftover -= count * step;
        }

        m_fields->stepping = true;
        for (int i = 0; i < count; i++) {
            GJBaseGameLayer::update(static_cast<float>(step));
        }
        m_fields->stepping = false;
    }

    double getModifiedDelta(float dt) {
        if (m_fields->stepping) {
            // Time warp slows physics down; speeding it up is ignored (the game caps it at 1).
            return static_cast<double>(dt) * std::min(m_gameState.m_timeWarp, 1.f);
        }
        return GJBaseGameLayer::getModifiedDelta(dt);
    }
};

// --- noclip, hitboxes and safe completion (PlayLayer) ------------------------------------------

class $modify(VbCheatPlayLayer, PlayLayer) {
    // Noclip: ignore deaths. The anticheat spike is the one thing that still kills,
    // so the game's own cheat protection keeps working.
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (vbot::Config::get().noclip && object != m_anticheatSpike) return;
        PlayLayer::destroyPlayer(player, object);
    }

    // Hitboxes: the game only draws its hitboxes in practice mode, so after every update we turn
    // its debug drawing on and ask it to draw, whatever the mode.
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!vbot::Config::get().hitboxes || !m_debugDrawNode) return;

        m_isDebugDrawEnabled = true;
        m_debugDrawNode->setVisible(true);
        this->updateDebugDraw();
    }

    // Cheats on: treat the completion as a test run so it isn't saved.
    void levelComplete() {
        if (vbot::Config::get().cheatsActive()) {
            m_isTestMode = true;
            Notification::create("Vbot: cheats are on, this completion won't count",
                                 NotificationIcon::Warning)->show();
        }
        PlayLayer::levelComplete();
    }
};
