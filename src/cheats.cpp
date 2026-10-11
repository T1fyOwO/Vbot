// cheats.cpp - noclip, hitboxes and TPS bypass.
//
// Noclip and TPS changes are cheats: while either is on, finishing a level does not count
// (the run is treated like a test run, so nothing is saved).
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <cmath>

#include "settings.hpp"

using namespace geode::prelude;

// --- TPS bypass + hitboxes (GJBaseGameLayer) ---------------------------------------------------

class $modify(VbCheatBaseLayer, GJBaseGameLayer) {
    // EXPERIMENTAL. At the normal 240 TPS (or with the bypass off) this calls the original game
    // code untouched, so normal play can't be affected. For any other value it re-does the
    // fixed-step math: how much of the frame's time is turned into physics steps.
    double getModifiedDelta(float dt) {
        auto& cfg = vbot::Config::get();
        if (!cfg.tpsBypass || cfg.tps == vbot::NORMAL_TPS || !PlayLayer::get()) {
            return GJBaseGameLayer::getModifiedDelta(dt);
        }

        double warp = std::min(m_gameState.m_timeWarp, 1.f);
        double step = warp / static_cast<double>(cfg.tps);
        double total = static_cast<double>(dt) + m_extraDelta;
        double steps = std::round(total / step);
        double used = steps * step;
        m_extraDelta = total - used;
        return used;
    }

    // Turns on the game's own hitbox drawing.
    void updateDebugDraw() {
        if (vbot::Config::get().hitboxes && PlayLayer::get()) {
            m_isDebugDrawEnabled = true;
        }
        GJBaseGameLayer::updateDebugDraw();
    }
};

// --- noclip, hitbox setup and safe completion (PlayLayer) --------------------------------------

class $modify(VbCheatPlayLayer, PlayLayer) {
    // Noclip: ignore deaths. The anticheat spike is the one thing that still kills,
    // so the game's own cheat protection keeps working.
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (vbot::Config::get().noclip && object != m_anticheatSpike) return;
        PlayLayer::destroyPlayer(player, object);
    }

    // Keeps the hitbox drawing on when the level starts or restarts.
    void updateDebugDrawSettings() {
        PlayLayer::updateDebugDrawSettings();
        if (vbot::Config::get().hitboxes) {
            m_isDebugDrawEnabled = true;
            if (m_debugDrawNode) m_debugDrawNode->setVisible(true);
        }
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
