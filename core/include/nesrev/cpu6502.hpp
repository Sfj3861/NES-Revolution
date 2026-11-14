#pragma once

#include "nesrev/mapper0.hpp"

#include <array>
#include <cstdint>

namespace nesrev {

class Ppu2C02;

// Minimal 2A03 CPU bus. Internal RAM is mirrored through $0000-$1FFF.
class CpuBus {
public:
    std::uint8_t read(std::uint16_t address) const noexcept;
    void write(std::uint16_t address, std::uint8_t value) noexcept;
    void load(std::uint16_t address, const std::uint8_t* data, std::uint32_t size) noexcept;
    [[nodiscard]] bool insert_cartridge(const CartridgeImage& image);
    void eject_cartridge() noexcept;
    [[nodiscard]] bool has_cartridge() const noexcept { return mapper0_.loaded(); }
    [[nodiscard]] Mapper0& mapper0() noexcept { return mapper0_; }
    void attach_ppu(Ppu2C02* ppu) noexcept { ppu_ = ppu; }
    void tick_cpu_cycles(std::uint32_t cycles) noexcept;
    [[nodiscard]] bool consume_ppu_nmi() noexcept;
    [[nodiscard]] std::uint8_t ppu_read_cartridge(std::uint16_t address) const noexcept;
    void ppu_write_cartridge(std::uint16_t address, std::uint8_t value) noexcept;
    [[nodiscard]] Mirroring cartridge_mirroring() const noexcept { return mapper0_.mirroring(); }
    void set_reset_vector(std::uint16_t address) noexcept;
    void set_irq_vector(std::uint16_t address) noexcept;
    void set_nmi_vector(std::uint16_t address) noexcept;

private:
    std::array<std::uint8_t, 0x800> ram_{};
    std::array<std::uint8_t, 0x2000> prg_ram_{};
    std::array<std::uint8_t, 0x10000> memory_{};
    Mapper0 mapper0_{};
    Ppu2C02* ppu_{nullptr};
};

struct CpuState {
    std::uint8_t a{0};
    std::uint8_t x{0};
    std::uint8_t y{0};
    std::uint8_t sp{0xFD};
    std::uint8_t status{0x24};
    std::uint16_t pc{0};
    std::uint64_t cycles{0};
};

class Cpu6502 {
public:
    explicit Cpu6502(CpuBus& bus) noexcept : bus_(bus) {}
    void reset() noexcept;
    void request_nmi() noexcept { nmi_pending_ = true; }
    void set_irq_line(bool asserted) noexcept { irq_line_ = asserted; }
    // Executes one instruction (or services an interrupt). Returns false for
    // an opcode not implemented yet; PC is restored to the opcode on failure.
    bool step() noexcept;
    [[nodiscard]] const CpuState& state() const noexcept { return state_; }

private:
    static constexpr std::uint8_t C = 0x01, Z = 0x02, I = 0x04,
        D = 0x08, B = 0x10, U = 0x20, V = 0x40, N = 0x80;
    CpuBus& bus_;
    CpuState state_{};
    bool nmi_pending_{false};
    bool irq_line_{false};

    std::uint8_t fetch() noexcept;
    std::uint16_t fetch16() noexcept;
    void set_flag(std::uint8_t flag, bool value) noexcept;
    [[nodiscard]] bool flag(std::uint8_t flag) const noexcept;
    void set_nz(std::uint8_t value) noexcept;
    void push(std::uint8_t value) noexcept;
    std::uint8_t pull() noexcept;
    void interrupt(std::uint16_t vector, bool is_brk) noexcept;
    void adc(std::uint8_t value) noexcept;
    void compare(std::uint8_t lhs, std::uint8_t rhs) noexcept;
};

} // namespace nesrev
