#include "nesrev/cartridge.hpp"
#include "nesrev/controller.hpp"
#include "nesrev/cpu6502.hpp"
#include "nesrev/input_queue.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    using namespace nesrev;
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
    while (const auto event = queue.pop()) state.apply(*event);
    assert(!state.pressed(Button::A));
    state.apply(InputEvent{Button::A, true, 200, 3});
    StandardController controller;
    controller.set_state(state);
    controller.write_strobe(1);
    controller.write_strobe(0);
    assert(controller.read() == 1);
    assert(controller.read() == 0);

    // CPU smoke test: LDA #$2A; ADC #$01; STA $0002; INX.
    CpuBus bus;
    const std::array<std::uint8_t, 7> program{0xA9,0x2A,0x69,0x01,0x85,0x02,0xE8};
    bus.load(0x8000, program.data(), static_cast<std::uint32_t>(program.size()));
    bus.set_reset_vector(0x8000);
    Cpu6502 cpu(bus);
    cpu.reset();
    assert(cpu.step()); assert(cpu.state().a == 0x2A);
    assert(cpu.step()); assert(cpu.state().a == 0x2B);
    assert(cpu.step()); assert(bus.read(0x0002) == 0x2B);
    assert(cpu.step()); assert(cpu.state().x == 1);
    assert(cpu.state().cycles == 16);

    std::cout << "NES Revolution core tests passed\n";
    return 0;
}
