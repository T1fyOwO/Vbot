// actions.hpp - what Record / Replay / Render do, plus where files live.
//
// Files go in Geode's config folder:
//   config/t1fy.vbot/Macros/<name>.vb
//   config/t1fy.vbot/Renders/<name>.mp4   (renderer comes in the next build)
#pragma once
#include <Geode/Geode.hpp>

#include <algorithm>

#include "macro.hpp"
#include "render_guard.hpp"
#include "settings.hpp"
#include "util.hpp"

namespace vbot {

// --- folders ----------------------------------------------------------------------------------

inline std::filesystem::path ensureDir(std::filesystem::path dir) {
    auto res = geode::utils::file::createDirectoryAll(dir);
    (void)res; // if this fails, the later read or write reports the real problem
    return dir;
}

inline std::filesystem::path macrosDir() {
    return ensureDir(geode::Mod::get()->getConfigDir() / "Macros");
}

inline std::filesystem::path rendersDir() {
    return ensureDir(geode::Mod::get()->getConfigDir() / "Renders");
}

inline std::filesystem::path macroPath(std::string const& name) {
    return macrosDir() / (util::sanitizeName(name) + ".vb");
}

// Names (without .vb) of every saved macro, sorted.
inline std::vector<std::string> listMacros() {
    std::vector<std::string> names;
    auto res = geode::utils::file::readDirectory(macrosDir());
    if (!res) return names;
    for (auto const& p : res.unwrap()) {
        if (p.extension() == ".vb") {
            names.push_back(geode::utils::string::pathToString(p.stem()));
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

// --- feedback ---------------------------------------------------------------------------------

inline void toast(std::string const& text, geode::NotificationIcon icon = geode::NotificationIcon::Info) {
    geode::Notification::create(text, icon)->show();
}

// --- macro actions ----------------------------------------------------------------------------

inline void toggleRecord() {
    auto& s = State::get();
    auto& cfg = Config::get();

    if (s.mode == Mode::Recording) {
        s.mode = Mode::Idle;
        s.macro.fps = static_cast<uint32_t>(cfg.effectiveTps());
        auto bytes = vb::encode(s.macro);
        auto path = macroPath(cfg.macroName);
        auto res = geode::utils::file::writeBinary(path, bytes);
        if (res) {
            toast(fmt::format("Saved {} inputs to {}.vb", s.macro.inputs.size(),
                              util::sanitizeName(cfg.macroName)),
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
    auto& cfg = Config::get();

    auto data = geode::utils::file::readBinary(macroPath(cfg.macroName));
    if (!data) {
        toast(fmt::format("No macro named {} found.", util::sanitizeName(cfg.macroName)),
              geode::NotificationIcon::Error);
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

    if (static_cast<int>(s.macro.fps) != cfg.effectiveTps()) {
        toast(fmt::format("Warning: recorded at {} TPS, but you are at {} TPS.", s.macro.fps,
                          cfg.effectiveTps()),
              geode::NotificationIcon::Warning);
    } else {
        toast(fmt::format("Loaded {} inputs. Restart the level to replay.", s.macro.inputs.size()),
              geode::NotificationIcon::Success);
    }
}

inline void stopAll() {
    State::get().mode = Mode::Idle;
}

inline void startRender() {
    if (!canRender()) return;
    // The renderer arrives in the next build.
    toast("FFmpeg API found. Rendering comes in the next build.");
}

} // namespace vbot
