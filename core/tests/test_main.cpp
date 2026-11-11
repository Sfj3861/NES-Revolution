#include "nesrev/cartridge.hpp"
#include "nesrev/controller.hpp"
#include "nesrev/input_queue.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    using namespace nesrev;

    // Minimal iNES NROM-128 image: header + 16 KiB PRG + 8 KiB CHR.
    std::vector<std::uint8_t> rom(16 + 16 * 1024 + 8 * 1024, 0);
    rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A;
    rom[4] = 1; rom[5] = 1;

    const auto parsed = parse_cartridge(rom);
    assert(static_cast<bool>(parsed));
    assert(parsed.image.mapper_id == 0);
    assert(parsed.image.prg_rom.size() == 16 * 1024);
    assert(parsed.image.chr_rom.size() == 8 * 1024);
    assert(mapper0_layout_supported(parsed.image));

    const std::array<std::uint8_t, 4> invalid{0, 1, 2, 3};
    assert(!parse_cartridge(invalid));

    InputQueue queue;
    queue.push(Button::A, true, 100);
    queue.push(Button::A, false, 150);
    assert(queue.size() == 2);

    ControllerState state;
    while (const auto event = queue.pop()) {
        state.apply(*event);
    }
    assert(!state.pressed(Button::A));

    state.apply(InputEvent{Button::A, true, 200, 3});
    StandardController controller;
    controller.set_state(state);
    controller.write_strobe(1);
    controller.write_strobe(0);
    assert(controller.read() == 1);
    assert(controller.read() == 0);

    std::cout << "NES Revolution core foundation tests passed\n";
    return 0;
}
