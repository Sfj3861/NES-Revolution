#pragma once

#include "nesrev/controller.hpp"
#include "nesrev/cpu6502.hpp"
#include "nesrev/ppu2c02.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace nesrev {

// Owns and coordinates the portable NES hardware core. A platform frontend
// supplies ROM bytes and input state, then requests complete video frames.
class NesMachine {
public:
    NesMachine() noexcept;

    [[nodiscard]] bool load_rom(std::span<const std::uint8_t> bytes);
    void unload() noexcept;
    [[nodiscard]] bool run_frame() noexcept;

    void set_controller1(const ControllerState& state) noexcept;
    void set_controller2(const ControllerState& state) noexcept;

    [[nodiscard]] bool loaded() const noexcept { return loaded_; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] const CpuState& cpu_state() const noexcept { return cpu_.state(); }
    [[nodiscard]] const std::array<std::uint32_t, Ppu2C02::Width * Ppu2C02::Height>&
    framebuffer() const noexcept { return ppu_.framebuffer(); }

private:
    CpuBus bus_;
    Ppu2C02 ppu_;
    Cpu6502 cpu_;
    StandardController controller1_;
    StandardController controller2_;
    bool loaded_{false};
    std::string error_;
};

} // namespace nesrev
