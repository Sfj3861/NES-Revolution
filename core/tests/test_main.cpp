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

    // NROM 16 KiB PRG mirrors at $C000 and CHR ROM is read-only.
    CartridgeImage nrom;
    nrom.mapper_id = 0;
    nrom.mirroring = Mirroring::Vertical;
    nrom.prg_rom.assign(16U * 1024U, 0xEA);
    nrom.prg_rom[0] = 0xA9;
    nrom.prg_rom.back() = 0x5C;
    nrom.chr_rom.assign(8U * 1024U, 0x31);
    CpuBus cartridge_bus;
    assert(cartridge_bus.insert_cartridge(nrom));
    assert(cartridge_bus.has_cartridge());
    assert(cartridge_bus.read(0x8000) == 0xA9);
    assert(cartridge_bus.read(0xC000) == 0xA9);
    assert(cartridge_bus.read(0xFFFF) == 0x5C);
    assert(cartridge_bus.cartridge_mirroring() == Mirroring::Vertical);
    cartridge_bus.write(0x8000, 0x00);
    assert(cartridge_bus.read(0x8000) == 0xA9);
    assert(cartridge_bus.ppu_read_cartridge(0x0000) == 0x31);
    cartridge_bus.ppu_write_cartridge(0x0000, 0x99);
    assert(cartridge_bus.ppu_read_cartridge(0x0000) == 0x31);

    // CHR-less NROM gets writable 8 KiB CHR RAM.
    nrom.chr_rom.clear();
    CpuBus chr_ram_bus;
    assert(chr_ram_bus.insert_cartridge(nrom));
    chr_ram_bus.ppu_write_cartridge(0x1FFF, 0xA7);
    assert(chr_ram_bus.ppu_read_cartridge(0x1FFF) == 0xA7);

    // PRG RAM and PPU register mirroring on the CPU address bus.
    cartridge_bus.write(0x6000, 0x42);
    assert(cartridge_bus.read(0x7FFF) == 0x00);
    assert(cartridge_bus.read(0x6000) == 0x42);
    cartridge_bus.write(0x2008, 0x77);
    assert(cartridge_bus.read(0x2000) == 0x77);

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

    // Indexed reads add one cycle when the effective address crosses a page.
    CpuBus indexed_bus;
    const std::array<std::uint8_t, 5> indexed_program{0xA2,0x01,0xBD,0xFF,0x80};
    indexed_bus.load(0x8000, indexed_program.data(), static_cast<std::uint32_t>(indexed_program.size()));
    indexed_bus.write(0x8100, 0x5A);
    indexed_bus.set_reset_vector(0x8000);
    Cpu6502 indexed_cpu(indexed_bus);
    indexed_cpu.reset();
    assert(indexed_cpu.step());
    assert(indexed_cpu.step());
    assert(indexed_cpu.state().a == 0x5A);
    assert(indexed_cpu.state().cycles == 14);

    std::cout << "NES Revolution core tests passed\n";
    return 0;
}
