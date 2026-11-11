#include "nesrev/input_queue.hpp"

#include <stdexcept>

namespace nesrev {

void InputQueue::push(const Button button, const bool pressed,
                      const HostTimestampNs timestamp_ns) {
    if (button >= Button::Count) {
        throw std::invalid_argument("InputQueue received an invalid button");
    }

    std::lock_guard lock(mutex_);
    events_.push_back(InputEvent{button, pressed, timestamp_ns, next_sequence_++});
}

std::optional<InputEvent> InputQueue::pop() {
    std::lock_guard lock(mutex_);
    if (events_.empty()) {
        return std::nullopt;
    }

    const auto event = events_.front();
    events_.pop_front();
    return event;
}

std::size_t InputQueue::size() const {
    std::lock_guard lock(mutex_);
    return events_.size();
}

void ControllerState::apply(const InputEvent& event) noexcept {
    const auto index = static_cast<std::size_t>(event.button);
    if (index < buttons_.size()) {
        buttons_[index] = event.pressed;
    }
}

bool ControllerState::pressed(const Button button) const noexcept {
    const auto index = static_cast<std::size_t>(button);
    return index < buttons_.size() && buttons_[index];
}

std::uint8_t ControllerState::latched_byte() const noexcept {
    std::uint8_t result = 0;
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        if (buttons_[i]) {
            result |= static_cast<std::uint8_t>(1U << i);
        }
    }
    return result;
}

} // namespace nesrev
