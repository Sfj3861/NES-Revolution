#pragma once

#include "nesrev/cartridge.hpp"

#include <cstdint>
#include <vector>

namespace nesrev {

// Nintendo NROM (mapper 0): fixed 16/32 KiB PRG and 8 KiB CHR.
// CHR-less cartridges expose 8 KiB of writable CHR RAM.
class Mapper0 {
public:
    [[nodiscard]] bool load(const CartridgeImage& image);
    void unload() noexcept;

    [[nodiscard]] bool loaded() const noexcept { return !prg_rom_.empty(); }
    [[nodiscard]] std::uint8_t cpu_read(std::uint16_t address) const noexcept;
    void cpu_write(std::uint16_t address, std::uint8_t value) noexcept;
    [[nodiscard]] std::uint8_t ppu_read(std::uint16_t address) const noexcept;
    void ppu_write(std::uint16_t address, std::uint8_t value) noexcept;

    [[nodiscard]] Mirroring mirroring() const noexcept { return mirroring_; }
    [[nodiscard]] bool uses_chr_ram() const noexcept { return chr_ram_; }

private:
    std::vector<std::uint8_t> prg_rom_;
    std::vector<std::uint8_t> chr_memory_;
    Mirroring mirroring_{Mirroring::Horizontal};
    bool chr_ram_{false};
};

} // namespace nesrev
