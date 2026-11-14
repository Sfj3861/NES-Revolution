#include "nesrev/nes_machine.hpp"

#include "nesrev/cartridge.hpp"

namespace nesrev {

NesMachine::NesMachine() noexcept
    : ppu_(bus_.mapper0()), cpu_(bus_) {
    bus_.attach_ppu(&ppu_);
    bus_.attach_apu(&apu_);
    bus_.attach_controllers(&controller1_, &controller2_);
}

bool NesMachine::load_rom(const std::span<const std::uint8_t> bytes) {
    unload();

    const auto parsed = parse_cartridge(bytes);
    if (!parsed) {
        error_ = parsed.error;
        return false;
    }
    if (!bus_.insert_cartridge(parsed.image)) {
        error_ = "Unsupported cartridge: this build currently supports NROM (mapper 0) only";
        return false;
    }

    ppu_.reset();
    cpu_.reset();
    loaded_ = true;
    error_.clear();
    return true;
}

void NesMachine::unload() noexcept {
    bus_.eject_cartridge();
    ppu_.reset();
    loaded_ = false;
    error_.clear();
}

bool NesMachine::run_frame() noexcept {
    if (!loaded_) {
        error_ = "No ROM is loaded";
        return false;
    }

    // At most 200,000 instructions per frame. This catches a stalled/faulted
    // CPU without imposing a wall-clock dependency on the portable core.
    for (std::uint32_t instructions = 0; instructions < 200000U; ++instructions) {
        if (!cpu_.step()) {
            error_ = "CPU encountered an unsupported opcode";
            loaded_ = false;
            return false;
        }
        if (ppu_.consume_frame_ready()) {
            return true;
        }
    }

    error_ = "Frame execution limit reached";
    loaded_ = false;
    return false;
}

std::vector<float> NesMachine::take_audio_samples() {
    return apu_.take_samples();
}

void NesMachine::set_controller1(const ControllerState& state) noexcept {
    controller1_.set_state(state);
}

void NesMachine::set_controller2(const ControllerState& state) noexcept {
    controller2_.set_state(state);
}

} // namespace nesrev
