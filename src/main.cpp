// main.cpp - Vbot: frame counter, input recorder, replay injection, restart handling.
// (The pause-menu button lives in pause_hooks.cpp / macro_button.hpp.)
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "macro.hpp"
#include "settings.hpp"
#include "stats.hpp"

using namespace geode::prelude;

// --- Physics-step hooks: count frames, record inputs, inject replay inputs ---------------------

class $modify(VbBaseGameLayer, GJBaseGameLayer) {
    // Runs once per physics step. We use it as the frame counter and as the moment to inject inputs.
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto& s = vbot::State::get();

        if (!PlayLayer::get()) {
            GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
            return;
        }

        vbot::Stats::get().lastStepDt = dt;
        vbot::Stats::get().procCounter++; // for the steps-per-second readout on the Cheats tab

        if (s.mode == vbot::Mode::Replaying) {
            auto const& inputs = s.macro.inputs;
            while (s.nextInput < inputs.size() && inputs[s.nextInput].frame <= s.frame) {
                auto const& in = inputs[s.nextInput++];
                // Accurate macros: put the player back exactly where it was when the input happened.
                if (in.hasPos) {
                    if (auto* p = in.player2 ? m_player2 : m_player1) {
                        p->setPosition({in.x, in.y});
                        p->m_yVelocity = in.yVel;
                    }
                }
                // Game buttons: 1 = jump, 2 = left, 3 = right. vb::Button is 0-based.
                this->handleButton(in.down, static_cast<int>(in.button) + 1, !in.player2);
            }
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        s.frame++;
    }

    // Every player input goes through here.
    void handleButton(bool down, int button, bool isPlayer1) {
        auto& s = vbot::State::get();
        if (s.mode == vbot::Mode::Recording && PlayLayer::get() && button >= 1 && button <= 3) {
            vb::Input input{s.frame, static_cast<vb::Button>(button - 1), down, !isPlayer1};
            if (vbot::Config::get().accurate) {
                if (auto* p = isPlayer1 ? m_player1 : m_player2) {
                    input.hasPos = true;
                    input.x = p->getPositionX();
                    input.y = p->getPositionY();
                    input.yVel = static_cast<float>(p->m_yVelocity);
                }
            }
            s.macro.inputs.push_back(input);
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

// --- Checkpoints and restarts -----------------------------------------------------------------
// Recording keeps going across deaths until you tap Record again. When you respawn from a
// checkpoint, the macro is cut back to that checkpoint's frame and recording continues from there.
// Restarting from the very beginning (no checkpoint) starts a fresh macro.

namespace {
std::vector<uint32_t> g_checkpointFrames; // frame counter when each checkpoint was stored
}

class $modify(VbPlayLayer, PlayLayer) {
    void storeCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::storeCheckpoint(checkpoint);
        g_checkpointFrames.push_back(vbot::State::get().frame);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        auto& s = vbot::State::get();

        size_t count = m_checkpointArray ? m_checkpointArray->count() : 0;
        if (count < g_checkpointFrames.size()) g_checkpointFrames.resize(count);

        if (count == 0 || g_checkpointFrames.size() < count) {
            // Fresh start from the beginning.
            g_checkpointFrames.clear();
            s.frame = 0;
            s.nextInput = 0;
            if (s.mode == vbot::Mode::Recording) s.macro.inputs.clear();
            return;
        }

        // Respawn from the last checkpoint: rewind to the frame it was stored on.
        uint32_t cp = g_checkpointFrames.back();
        s.frame = cp;

        auto& inputs = s.macro.inputs;
        if (s.mode == vbot::Mode::Recording) {
            std::erase_if(inputs, [cp](vb::Input const& in) { return in.frame >= cp; });

            // The game releases every button on respawn, so close any press that is still open.
            bool held[3][2] = {};
            for (auto const& in : inputs) held[static_cast<int>(in.button)][in.player2] = in.down;
            for (int b = 0; b < 3; b++) {
                for (int p = 0; p < 2; p++) {
                    if (held[b][p]) inputs.push_back({cp, static_cast<vb::Button>(b), false, p == 1});
                }
            }
        }

        // Replay: carry on from the first input at or after this frame.
        auto it = std::lower_bound(inputs.begin(), inputs.end(), cp,
                                   [](vb::Input const& in, uint32_t f) { return in.frame < f; });
        s.nextInput = static_cast<size_t>(it - inputs.begin());
    }
};
