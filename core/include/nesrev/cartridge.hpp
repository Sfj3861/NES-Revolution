#pragma once

#include "nesrev/types.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace nesrev {

enum class RomFormat : std::uint8_t {
    INes,
    Nes20
};

struct CartridgeImage {
    RomFormat format{RomFormat::INes};
    std::uint16_t mapper_id{0};
    std::uint8_t submapper{0};
    Mirroring mirroring{Mirroring::Horizontal};
    bool battery_backed{false};
    bool has_trainer{false};
    bool uses_chr_ram{false};
    std::vector<std::uint8_t> trainer;
    std::vector<std::uint8_t> prg_rom;
    std::vector<std::uint8_t> chr_rom;
};

struct CartridgeParseResult {
    CartridgeImage image;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error.empty();
    }
};

// Parses the iNES 1.0 header and the linear-size subset of NES 2.0.
// NES 2.0 exponent/multiplier ROM sizes are deliberately rejected until
// implemented, rather than being misinterpreted.
[[nodiscard]] CartridgeParseResult parse_cartridge(
    std::span<const std::uint8_t> bytes);

[[nodiscard]] bool mapper0_layout_supported(
    const CartridgeImage& image) noexcept;

} // namespace nesrev
