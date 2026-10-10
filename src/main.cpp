// main.cpp - Vbot: frame counter, input recorder, replay injection, restart handling.
// (The pause-menu button lives in pause_hooks.cpp / macro_button.hpp.)
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "macro.hpp"

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

        if (s.mode == vbot::Mode::Replaying) {
            auto const& inputs = s.macro.inputs;
            while (s.nextInput < inputs.size() && inputs[s.nextInput].frame <= s.frame) {
                auto const& in = inputs[s.nextInput++];
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
            s.macro.inputs.push_back({
                s.frame,
                static_cast<vb::Button>(button - 1),
                down,
                !isPlayer1,
            });
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

// --- Level restart: frame 0 again -------------------------------------------------------------

class $modify(VbPlayLayer, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();
        auto& s = vbot::State::get();
        s.frame = 0;
        s.nextInput = 0;
        if (s.mode == vbot::Mode::Recording) s.macro.inputs.clear();
    }
};
