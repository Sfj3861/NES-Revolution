#pragma once

#include "nesrev/input_queue.hpp"

#include <cstdint>

namespace nesrev {

// Models the standard NES controller serial shift register.
// The emulated CPU decides when to strobe and read; host input is not allowed
// to bypass this hardware-visible protocol.
class StandardController {
public:
    void set_state(const ControllerState& state) noexcept;
    void write_strobe(std::uint8_t value) noexcept;
    [[nodiscard]] std::uint8_t read() noexcept;

private:
    ControllerState state_{};
    std::uint8_t latched_{0};
    std::uint8_t shift_index_{0};
    bool strobe_{false};

    void latch() noexcept;
};

} // namespace nesrev
