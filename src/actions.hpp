// actions.hpp - what the Record / Replay / Render buttons do.
#pragma once
#include <Geode/Geode.hpp>

#include "macro.hpp"
#include "render_guard.hpp"

namespace vbot {

inline std::filesystem::path macroPath() {
    return geode::Mod::get()->getSaveDir() / "last.vb";
}

inline void toast(std::string const& text, geode::NotificationIcon icon = geode::NotificationIcon::Info) {
    geode::Notification::create(text, icon)->show();
}

inline void toggleRecord() {
    auto& s = State::get();

    if (s.mode == Mode::Recording) {
        s.mode = Mode::Idle;
        s.macro.fps = 240;
        auto bytes = vb::encode(s.macro);
        auto res = geode::utils::file::writeBinary(macroPath(), bytes);
        if (res) {
            toast(fmt::format("Saved {} inputs ({} bytes)", s.macro.inputs.size(), bytes.size()),
                  geode::NotificationIcon::Success);
        } else {
            toast("Couldn't save the macro", geode::NotificationIcon::Error);
        }
        return;
    }

    s.macro.inputs.clear();
    s.mode = Mode::Recording;
    toast("Recording. Restart the level to begin a clean attempt.");
}

inline void armReplay() {
    auto& s = State::get();

    auto data = geode::utils::file::readBinary(macroPath());
    if (!data) {
        toast("No saved macro found. Record one first.", geode::NotificationIcon::Error);
        return;
    }
    auto macro = vb::decode(data.unwrap());
    if (!macro) {
        toast("That macro file is corrupted.", geode::NotificationIcon::Error);
        return;
    }

    s.macro = std::move(*macro);
    s.mode = Mode::Replaying;
    s.nextInput = 0;
    toast(fmt::format("Loaded {} inputs. Restart the level to replay.", s.macro.inputs.size()),
          geode::NotificationIcon::Success);
}

inline void startRender() {
    if (!canRender()) return;
    // Batch 3 will start the FFmpeg render here.
    toast("FFmpeg API found. Rendering is coming in the next batch.");
}

} // namespace vbot
