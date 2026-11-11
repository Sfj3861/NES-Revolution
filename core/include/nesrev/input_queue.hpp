#pragma once

#include "nesrev/types.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>

namespace nesrev {

enum class Button : std::uint8_t {
    A, B, Select, Start, Up, Down, Left, Right,
    Count
};

struct InputEvent {
    Button button{Button::A};
    bool pressed{false};
    HostTimestampNs timestamp_ns{0};
    std::uint64_t sequence{0};
};

// Host events are captured independently of the emulated CPU's controller
// reads. A mutex-protected queue is intentionally used in this first,
// correctness-focused implementation; a lock-free queue requires profiling
// and a carefully specified overflow policy before it is justified.
class InputQueue {
public:
    void push(Button button, bool pressed, HostTimestampNs timestamp_ns);
    [[nodiscard]] std::optional<InputEvent> pop();
    [[nodiscard]] std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::deque<InputEvent> events_;
    std::uint64_t next_sequence_{1};
};

class ControllerState {
public:
    void apply(const InputEvent& event) noexcept;
    [[nodiscard]] bool pressed(Button button) const noexcept;
    [[nodiscard]] std::uint8_t latched_byte() const noexcept;

private:
    std::array<bool, static_cast<std::size_t>(Button::Count)> buttons_{};
};

} // namespace nesrev
