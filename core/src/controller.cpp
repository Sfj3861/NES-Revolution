#include "nesrev/controller.hpp"

namespace nesrev {

void StandardController::set_state(const ControllerState& state) noexcept {
    state_ = state;
    if (strobe_) {
        latch();
    }
}

void StandardController::write_strobe(const std::uint8_t value) noexcept {
    const bool next_strobe = (value & 1U) != 0;
    if (next_strobe || (strobe_ && !next_strobe)) {
        latch();
    }
    strobe_ = next_strobe;
    if (strobe_) {
        shift_index_ = 0;
    }
}

std::uint8_t StandardController::read() noexcept {
    if (strobe_) {
        return static_cast<std::uint8_t>(state_.pressed(Button::A) ? 1U : 0U);
    }

    if (shift_index_ >= 8) {
        return 1U;
    }

    const auto value = static_cast<std::uint8_t>((latched_ >> shift_index_) & 1U);
    ++shift_index_;
    return value;
}

void StandardController::latch() noexcept {
    latched_ = state_.latched_byte();
    shift_index_ = 0;
}

} // namespace nesrev
