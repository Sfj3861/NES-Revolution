#include "nesrev/apu2a03.hpp"
#include "nesrev/cartridge.hpp"
#include "nesrev/controller.hpp"
#include "nesrev/cpu6502.hpp"
#include "nesrev/input_queue.hpp"
#include "nesrev/ppu2c02.hpp"
#include "nesrev/nes_machine.hpp"

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

    // PPU nametable mirroring, palette aliases, CHR RAM and CPU registers.
    Ppu2C02 ppu(chr_ram_bus.mapper0());
    ppu.ppu_write(0x2000, 0x12);
    assert(ppu.ppu_read(0x2400) == 0x12); // Vertical mirroring
    ppu.ppu_write(0x3F00, 0x2A);
    assert(ppu.ppu_read(0x3F10) == 0x2A); // Universal background palette alias
    ppu.cpu_write(0x2006, 0x20);
    ppu.cpu_write(0x2006, 0x00);
    ppu.cpu_write(0x2007, 0x66);
    assert(ppu.ppu_read(0x2000) == 0x66);
    ppu.cpu_write(0x2000, 0x80);
    while (ppu.scanline() != 241U || ppu.dot() != 1U) ppu.clock();
    ppu.clock();
    assert(ppu.consume_nmi());
    assert((ppu.cpu_read(0x2002) & 0x80U) != 0);

    // CPU bus forwards PPU register reads/writes and advances three PPU dots
    // for every CPU cycle.
    CpuBus ppu_bus;
    CartridgeImage chr_ram_cart;
    chr_ram_cart.mapper_id = 0;
    chr_ram_cart.prg_rom.assign(16U * 1024U, 0xEA);
    assert(ppu_bus.insert_cartridge(chr_ram_cart));
    Ppu2C02 connected_ppu(ppu_bus.mapper0());
    ppu_bus.attach_ppu(&connected_ppu);
    ppu_bus.write(0x2006, 0x20);
    ppu_bus.write(0x2006, 0x00);
    ppu_bus.write(0x2007, 0xA5);
    assert(connected_ppu.ppu_read(0x2000) == 0xA5);
    const auto initial_dot = connected_ppu.dot();
    ppu_bus.tick_cpu_cycles(2);
    assert(connected_ppu.dot() == initial_dot + 6U);

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

    // APU pulse channel produces samples after its registers and timer are set.
    Apu2A03 test_apu(48000U);
    test_apu.write(0x4015, 0x01);
    test_apu.write(0x4000, 0x9F);
    test_apu.write(0x4002, 0x40);
    test_apu.write(0x4003, 0x08);
    test_apu.clock_cpu_cycles(40000U);
    auto audio_samples = test_apu.take_samples();
    assert(!audio_samples.empty());
    bool has_nonzero_audio = false;
    for (const auto sample : audio_samples) {
        assert(sample >= -1.0F && sample <= 1.0F);
        if (sample > -0.99F) has_nonzero_audio = true;
    }
    assert(has_nonzero_audio);
    assert(test_apu.take_samples().empty());

    // NesMachine loads an NROM ROM, resets the CPU from its cartridge vector,
    // and advances a complete PPU frame.
    std::vector<std::uint8_t> machine_rom(16U + 16U * 1024U, 0);
    machine_rom[0] = 'N'; machine_rom[1] = 'E'; machine_rom[2] = 'S'; machine_rom[3] = 0x1A;
    machine_rom[4] = 1; machine_rom[5] = 0;
    machine_rom[16] = 0x4C; machine_rom[17] = 0x00; machine_rom[18] = 0x80; // JMP $8000
    machine_rom[16U + 0x3FFCU] = 0x00;
    machine_rom[16U + 0x3FFDU] = 0x80;
    NesMachine machine;
    assert(machine.load_rom(machine_rom));
    assert(machine.loaded());
    assert(machine.cpu_state().pc == 0x8000);
    assert(machine.run_frame());
    assert(machine.cpu_state().cycles > 7U);
    assert(!machine.error().size());

    std::cout << "NES Revolution core tests passed
";
    return 0;
}
