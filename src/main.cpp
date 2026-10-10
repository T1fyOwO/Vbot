// main.cpp - Vbot batch 1: input recorder, replayer, and pause-menu buttons.
//
// Controls (in the pause menu, bottom-left):
//   Record : start/stop recording. Every level restart clears the macro and records the new attempt.
//            Stopping saves it to <save dir>/last.vb
//   Replay : loads last.vb and arms replay. Restart the level to play it back from frame 0.
//   Render : checks FFmpeg API is installed (popup if not). Actual rendering comes in batch 3.
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

#include "macro.hpp"
#include "render_guard.hpp"

using namespace geode::prelude;

namespace {

std::filesystem::path macroPath() {
    return Mod::get()->getSaveDir() / "last.vb";
}

void toast(std::string const& text, NotificationIcon icon = NotificationIcon::Info) {
    Notification::create(text, icon)->show();
}

} // namespace

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

// --- Pause menu buttons -----------------------------------------------------------------------

class $modify(VbPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto* menu = CCMenu::create();
        menu->setPosition({70.f, 40.f});

        auto add = [&](char const* label, SEL_MenuHandler cb, float y) {
            auto* btn = CCMenuItemSpriteExtra::create(ButtonSprite::create(label), this, cb);
            btn->setPosition({0.f, y});
            menu->addChild(btn);
        };
        add("Record", menu_selector(VbPauseLayer::onVbRecord), 80.f);
        add("Replay", menu_selector(VbPauseLayer::onVbReplay), 40.f);
        add("Render", menu_selector(VbPauseLayer::onVbRender), 0.f);

        this->addChild(menu);
    }

    void onVbRecord(CCObject*) {
        auto& s = vbot::State::get();

        if (s.mode == vbot::Mode::Recording) {
            s.mode = vbot::Mode::Idle;
            s.macro.fps = 240;
            auto bytes = vb::encode(s.macro);
            auto res = file::writeBinary(macroPath(), bytes);
            if (res) {
                toast(fmt::format("Saved {} inputs ({} bytes)", s.macro.inputs.size(), bytes.size()),
                      NotificationIcon::Success);
            } else {
                toast("Couldn't save the macro", NotificationIcon::Error);
            }
            return;
        }

        s.macro.inputs.clear();
        s.mode = vbot::Mode::Recording;
        toast("Recording. Restart the level to begin a clean attempt.");
    }

    void onVbReplay(CCObject*) {
        auto& s = vbot::State::get();

        auto data = file::readBinary(macroPath());
        if (!data) {
            toast("No saved macro found. Record one first.", NotificationIcon::Error);
            return;
        }
        auto macro = vb::decode(data.unwrap());
        if (!macro) {
            toast("That macro file is corrupted.", NotificationIcon::Error);
            return;
        }

        s.macro = std::move(*macro);
        s.mode = vbot::Mode::Replaying;
        s.nextInput = 0;
        toast(fmt::format("Loaded {} inputs. Restart the level to replay.", s.macro.inputs.size()),
              NotificationIcon::Success);
    }

    void onVbRender(CCObject*) {
        if (!vbot::canRender()) return;
        // Batch 3 will start the FFmpeg render here.
        toast("FFmpeg API found. Rendering is coming in the next batch.");
    }
};
