#include "nesrev/mapper0.hpp"

#include <algorithm>

namespace nesrev {

bool Mapper0::load(const CartridgeImage& image) {
    if (!mapper0_layout_supported(image)) {
        return false;
    }

    // Build temporary storage first so a failed allocation cannot leave a
    // half-loaded cartridge installed in the mapper.
    auto prg = image.prg_rom;
    auto chr = image.chr_rom;
    const bool use_chr_ram = chr.empty();
    if (use_chr_ram) {
        chr.assign(8U * 1024U, 0);
    }

    prg_rom_ = std::move(prg);
    chr_memory_ = std::move(chr);
    mirroring_ = image.mirroring;
    chr_ram_ = use_chr_ram;
    return true;
}

void Mapper0::unload() noexcept {
    prg_rom_.clear();
    chr_memory_.clear();
    mirroring_ = Mirroring::Horizontal;
    chr_ram_ = false;
}

std::uint8_t Mapper0::cpu_read(const std::uint16_t address) const noexcept {
    if (!loaded() || address < 0x8000U) {
        return 0;
    }
    const auto offset = static_cast<std::size_t>(address - 0x8000U) % prg_rom_.size();
    return prg_rom_[offset];
}

void Mapper0::cpu_write(const std::uint16_t address, const std::uint8_t value) noexcept {
    // NROM PRG is read-only. Writes to cartridge ROM space are ignored.
    (void)address;
    (void)value;
}

std::uint8_t Mapper0::ppu_read(const std::uint16_t address) const noexcept {
    if (address >= 0x2000U || chr_memory_.empty()) {
        return 0;
    }
    return chr_memory_[address];
}

void Mapper0::ppu_write(const std::uint16_t address, const std::uint8_t value) noexcept {
    if (chr_ram_ && address < 0x2000U && !chr_memory_.empty()) {
        chr_memory_[address] = value;
    }
}

} // namespace nesrev
