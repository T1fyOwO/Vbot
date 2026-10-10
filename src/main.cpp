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
    void processCommands(float dt) {
        auto& s = vbot::State::get();
