// vb.hpp - Vbot macro format (.vb) v1 and v2, header-only, no dependencies.
//
// Layout:
//   "VBOT"            4 bytes magic
//   version           1 byte  (currently 1)
//   fps               varint
//   event count       varint
//   events            count x varint, each = (delta << 4) | (button << 2) | (down << 1) | player
//     delta  = frames since previous event (first event: since frame 0)
//     button = 0 jump, 1 left, 2 right
//     down   = 1 press, 0 release
//     player = 0 player 1, 1 player 2
//
// Version 2 ("accurate" macros) is the same, plus a flags byte right after the version byte.
//   flags bit 0 = every event is followed by the player's state when it happened:
//     x, y, y-velocity as 3 little-endian float32 (12 bytes). Replay snaps the player back to it,
//     which keeps a long macro from drifting. Macros without it are still written as version 1,
//     so older Vbot versions can open them, and Vbot reads both versions.
//
// Small deltas (< 8) fit in a single byte per input.
// A final deflate/zstd pass over the whole file can be added later.
#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <vector>

namespace vb {

enum class Button : uint8_t { Jump = 0, Left = 1, Right = 2 };

struct Input {
    uint32_t frame = 0;
    Button button = Button::Jump;
    bool down = false;
    bool player2 = false;

    // Optional player state at the moment of the input (version 2 macros).
    bool hasPos = false;
    float x = 0.f, y = 0.f, yVel = 0.f;

    bool operator==(Input const& o) const {
        return frame == o.frame && button == o.button && down == o.down && player2 == o.player2 &&
               hasPos == o.hasPos && (!hasPos || (x == o.x && y == o.y && yVel == o.yVel));
    }
};

struct Macro {
    uint32_t fps = 240;
    std::vector<Input> inputs; // must be sorted by frame
};

namespace detail {
inline void putVarint(std::vector<uint8_t>& out, uint64_t v) {
    while (v >= 0x80) {
        out.push_back(static_cast<uint8_t>(v | 0x80));
        v >>= 7;
    }
    out.push_back(static_cast<uint8_t>(v));
}

inline bool getVarint(std::vector<uint8_t> const& in, size_t& pos, uint64_t& v) {
    v = 0;
    for (int shift = 0; shift <= 63; shift += 7) {
        if (pos >= in.size()) return false;
        uint8_t b = in[pos++];
        v |= static_cast<uint64_t>(b & 0x7F) << shift;
        if (!(b & 0x80)) return true;
    }
    return false;
}

inline void putFloat(std::vector<uint8_t>& out, float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    for (int i = 0; i < 4; i++) out.push_back(static_cast<uint8_t>(u >> (8 * i)));
}

inline bool getFloat(std::vector<uint8_t> const& in, size_t& pos, float& f) {
    if (in.size() - pos < 4) return false;
    uint32_t u = 0;
    for (int i = 0; i < 4; i++) u |= static_cast<uint32_t>(in[pos++]) << (8 * i);
    std::memcpy(&f, &u, 4);
    return true;
}
} // namespace detail

inline std::vector<uint8_t> encode(Macro const& m) {
    // Only use version 2 when some input actually carries position data.
    bool accurate = false;
    for (auto const& i : m.inputs) accurate = accurate || i.hasPos;

    std::vector<uint8_t> out{'V', 'B', 'O', 'T', static_cast<uint8_t>(accurate ? 2 : 1)};
    if (accurate) out.push_back(1); // flags: position data
    detail::putVarint(out, m.fps);
    detail::putVarint(out, m.inputs.size());
    uint32_t prev = 0;
    for (auto const& i : m.inputs) {
        uint64_t delta = i.frame - prev;
        prev = i.frame;
        uint64_t packed = (delta << 4) | (static_cast<uint64_t>(i.button) << 2) |
                          (static_cast<uint64_t>(i.down) << 1) | static_cast<uint64_t>(i.player2);
        detail::putVarint(out, packed);
        if (accurate) {
            // An input without data (e.g. a release Vbot added itself) is stored as x = NaN.
            detail::putFloat(out, i.hasPos ? i.x : std::numeric_limits<float>::quiet_NaN());
            detail::putFloat(out, i.hasPos ? i.y : 0.f);
            detail::putFloat(out, i.hasPos ? i.yVel : 0.f);
        }
    }
    return out;
}

inline std::optional<Macro> decode(std::vector<uint8_t> const& in) {
    if (in.size() < 5 || in[0] != 'V' || in[1] != 'B' || in[2] != 'O' || in[3] != 'T' ||
        (in[4] != 1 && in[4] != 2))
        return std::nullopt;
    size_t pos = 5;
    bool withPos = false;
    if (in[4] == 2) {
        if (pos >= in.size()) return std::nullopt;
        uint8_t flags = in[pos++];
        if (flags & ~1u) return std::nullopt; // unknown flags: written by a newer Vbot
        withPos = flags & 1;
    }
    uint64_t fps, count;
    if (!detail::getVarint(in, pos, fps) || !detail::getVarint(in, pos, count)) return std::nullopt;
    // every event takes at least 1 byte, so this rejects absurd counts early
    if (count > in.size() - pos) return std::nullopt;

    Macro m;
    m.fps = static_cast<uint32_t>(fps);
    m.inputs.reserve(count);
    uint32_t frame = 0;
    for (uint64_t n = 0; n < count; n++) {
        uint64_t packed;
        if (!detail::getVarint(in, pos, packed)) return std::nullopt;
        uint8_t button = (packed >> 2) & 3;
        if (button > 2) return std::nullopt;
        frame += static_cast<uint32_t>(packed >> 4);
        Input input{frame, static_cast<Button>(button), static_cast<bool>((packed >> 1) & 1),
                    static_cast<bool>(packed & 1)};
        if (withPos) {
            if (!detail::getFloat(in, pos, input.x) || !detail::getFloat(in, pos, input.y) ||
                !detail::getFloat(in, pos, input.yVel))
                return std::nullopt;
            input.hasPos = !std::isnan(input.x);
            if (!input.hasPos) input.x = input.y = input.yVel = 0.f;
        }
        m.inputs.push_back(input);
    }
    return m;
}

} // namespace vb
