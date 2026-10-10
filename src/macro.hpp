// macro.hpp - shared recorder/replayer state for Vbot.
#pragma once
#include "vb.hpp"
#include <cstddef>
#include <cstdint>

namespace vbot {

enum class Mode { Idle, Recording, Replaying };

struct State {
    Mode mode = Mode::Idle;
    uint32_t frame = 0;     // physics step counter, reset every time the level restarts
    size_t nextInput = 0;   // replay cursor into macro.inputs
    vb::Macro macro;        // the macro being recorded or replayed

    static State& get() {
        static State s;
        return s;
    }
};

} // namespace vbot
