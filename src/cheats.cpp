// cheats.cpp - noclip, hitboxes and TPS bypass.
//
// Noclip and TPS changes are cheats: while either is on, finishing a level does not count
// (the run is treated like a test run, so nothing is saved).
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>

#include <algorithm>
#include <array>

#include "settings.hpp"
#include "stats.hpp"

using namespace geode::prelude;

namespace {

// A lag spike can hand us a huge frame time; never run more than this many steps for one frame.
constexpr int MAX_STEPS_PER_FRAME = 1000;

// While the TPS bypass runs several steps in one frame, only the last one needs to draw hitboxes.
bool g_skipDraw = false;
// Set when the game's own postUpdate ran during the current update (so we don't draw twice).
bool g_postUpdateRan = false;
// True while the scheduler hook below is feeding the game short ticks.
bool g_schedTicking = false;

// Draws the game's own hitboxes, whatever the mode. The game normally only does this in
// practice mode (PlayLayer::shouldDebugDraw is "debug draw on AND practice mode"), so we turn
// the drawing on and ask for it directly.
void drawGameHitboxes(PlayLayer* pl) {
    auto& stats = vbot::Stats::get();
    if (!pl->m_objectLayer) return;

    // The draw node should already exist; make one if the game never did.
    if (!pl->m_debugDrawNode) {
        pl->m_debugDrawNode = CCDrawNode::create();
        pl->m_objectLayer->addChild(pl->m_debugDrawNode, 1000);
        stats.hitboxNodeCreated = true;
    }
    if (!pl->m_debugDrawPoints) {
        pl->m_debugDrawPoints = new std::array<CCPoint, 400>();
    }

    auto* node = pl->m_debugDrawNode;
    pl->m_isDebugDrawEnabled = true;
    node->setVisible(true);
    node->m_bUseArea = false; // the game's draw-area culling can hide everything outside practice mode
    pl->updateDebugDraw();
    stats.hitboxVertices = static_cast<int>(node->m_nBufferCount);
}

} // namespace

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
//
// A higher TPS makes physics more precise. It does NOT make the game run faster, so the level
// looks the same; the Cheats tab shows the measured steps per second to prove it is working.

class $modify(VbTpsLayer, GJBaseGameLayer) {
    struct Fields {
        double leftover = 0.0;   // real time not turned into a physics step yet
        bool stepping = false;   // true while we are running our own physics steps
        bool announced = false;  // the "TPS bypass is on" toast was shown for this level
    };

    void update(float dt) {
        auto& cfg = vbot::Config::get();
        auto* pl = PlayLayer::get();
        bool inLevel = pl && static_cast<GJBaseGameLayer*>(pl) == static_cast<GJBaseGameLayer*>(this);

        if (!inLevel) {
            GJBaseGameLayer::update(dt);
            return;
        }

        g_postUpdateRan = false;
        auto& stats = vbot::Stats::get();
        if (!(cfg.tpsBypass && cfg.tps > vbot::NORMAL_TPS)) stats.bypassIgnored = false; // retry after a settings change
        bool active = cfg.tpsBypass && cfg.tps > vbot::NORMAL_TPS && !stats.bypassIgnored;

        if (!active) {
            m_fields->leftover = 0.0;
            m_fields->announced = false;
            GJBaseGameLayer::update(dt);
            vbot::Stats::get().tick(dt, 1);
            if (cfg.hitboxes && !g_postUpdateRan) drawGameHitboxes(pl);
            return;
        }

        if (cfg.tpsScheduler) {
            // Scheduler mode: VbScheduler below is already ticking the whole game at 1/TPS, so each
            // update() here is one short tick. Just let the game run it.
            m_fields->stepping = g_schedTicking;
            GJBaseGameLayer::update(dt);
            m_fields->stepping = false;
            return;
        }

        if (!m_fields->announced) {
            m_fields->announced = true;
            Notification::create(fmt::format("Vbot: TPS bypass on, {} TPS", cfg.tps),
                                 NotificationIcon::Info, 1.5f)->show();
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
            g_skipDraw = (i != count - 1);
            GJBaseGameLayer::update(static_cast<float>(step));
        }
        g_skipDraw = false;
        m_fields->stepping = false;

        // Safety net: we asked for steps of 1/TPS. If the game still ran steps close to 1/240,
        // it ignored us, and every extra call would just make the game run faster. Switch the
        // bypass off for this session instead of speeding the game up.
        if (count > 0 && stats.lastStepDt > 1.5 / static_cast<double>(cfg.tps)) {
            stats.bypassIgnored = true;
            m_fields->leftover = 0.0;
            m_fields->announced = false;
            Notification::create("Vbot: this game build ignored the TPS change, bypass turned off",
                                 NotificationIcon::Warning, 2.5f)->show();
        }

        vbot::Stats::get().tick(dt, static_cast<uint32_t>(count));
        if (cfg.hitboxes && !g_postUpdateRan) drawGameHitboxes(pl);
    }

    double getModifiedDelta(float dt) {
        if (m_fields->stepping) {
            // Time warp slows physics down; speeding it up is ignored (the game caps it at 1).
            GJBaseGameLayer::getModifiedDelta(dt); // keep the game's own bookkeeping in sync
            return static_cast<double>(dt) * std::min(m_gameState.m_timeWarp, 1.f);
        }
        return GJBaseGameLayer::getModifiedDelta(dt);
    }
};

// --- TPS bypass, scheduler mode (CCScheduler) ---------------------------------------------------
//
// The scheduler is what calls update() on everything each frame. In this mode we run the scheduler
// several times per real frame with a short time (1/TPS), so the game sees what it would see at
// an FPS equal to the TPS. The game's own update() then takes one step of that length.
// Cocos' scheduler update is cheap when little is scheduled, but animations and music keep their
// real speed because the total time we feed in per real second is still one second.

class $modify(VbScheduler, CCScheduler) {
    struct Fields {
        double leftover = 0.0;
        bool announced = false;
    };

    void update(float dt) {
        auto& cfg = vbot::Config::get();
        auto& stats = vbot::Stats::get();
        auto* pl = PlayLayer::get();

        bool active = pl && cfg.tpsBypass && cfg.tpsScheduler && cfg.tps > vbot::NORMAL_TPS &&
                      !stats.bypassIgnored;
        // Don't speed the pause menu up: only tick fast while the level is really running.
        if (active) {
            auto* scene = CCDirector::get()->getRunningScene();
            if (scene && scene->getChildByType<PauseLayer>(0)) active = false;
        }
        if (!active) {
            m_fields->leftover = 0.0;
            m_fields->announced = false;
            CCScheduler::update(dt);
            return;
        }

        if (!m_fields->announced) {
            m_fields->announced = true;
            Notification::create(fmt::format("Vbot: TPS bypass on, {} TPS", cfg.tps),
                                 NotificationIcon::Info, 1.5f)->show();
        }

        double step = 1.0 / static_cast<double>(cfg.tps);
        m_fields->leftover += static_cast<double>(dt);
        int count = static_cast<int>(m_fields->leftover / step);
        if (count > MAX_STEPS_PER_FRAME) {
            count = MAX_STEPS_PER_FRAME;
            m_fields->leftover = 0.0;
        } else {
            m_fields->leftover -= count * step;
        }

        g_schedTicking = true;
        for (int i = 0; i < count; i++) {
            g_skipDraw = (i != count - 1);
            CCScheduler::update(static_cast<float>(step));
        }
        g_skipDraw = false;
        g_schedTicking = false;

        stats.tick(dt, static_cast<uint32_t>(count));

        if (count > 0 && stats.lastStepDt > 1.5 / static_cast<double>(cfg.tps)) {
            stats.bypassIgnored = true;
            Notification::create("Vbot: this game build ignored the TPS change, bypass turned off",
                                 NotificationIcon::Warning, 2.5f)->show();
        }
        if (cfg.hitboxes && !g_postUpdateRan) drawGameHitboxes(pl);
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

    // Hitboxes: after the game's own per-frame work, draw them.
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        g_postUpdateRan = true;
        if (g_skipDraw || !vbot::Config::get().hitboxes) return;
        drawGameHitboxes(this);
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
