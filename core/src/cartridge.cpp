#include "nesrev/cartridge.hpp"

#include <limits>

namespace nesrev {
namespace {

constexpr std::size_t kHeaderSize = 16;
constexpr std::size_t kTrainerSize = 512;
constexpr std::size_t kPrgUnit = 16 * 1024;
constexpr std::size_t kChrUnit = 8 * 1024;

std::uint32_t read_u16_le(const std::uint8_t lo, const std::uint8_t hi) {
    return static_cast<std::uint32_t>(lo) |
           (static_cast<std::uint32_t>(hi) << 8U);
}

bool has_magic(const std::span<const std::uint8_t> bytes) {
    return bytes.size() >= 4 && bytes[0] == 'N' && bytes[1] == 'E' &&
           bytes[2] == 'S' && bytes[3] == 0x1A;
}

} // namespace

CartridgeParseResult parse_cartridge(const std::span<const std::uint8_t> bytes) {
    CartridgeParseResult result{};

    if (bytes.size() < kHeaderSize || !has_magic(bytes)) {
        result.error = "Invalid or truncated iNES header";
        return result;
    }

    const auto flags6 = bytes[6];
    const auto flags7 = bytes[7];
    const bool nes20 = (flags7 & 0x0CU) == 0x08U;

    result.image.format = nes20 ? RomFormat::Nes20 : RomFormat::INes;
    result.image.mapper_id = static_cast<std::uint16_t>(
        (flags6 >> 4U) | (flags7 & 0xF0U));
    result.image.mirroring = (flags6 & 0x08U) != 0
        ? Mirroring::FourScreen
        : ((flags6 & 0x01U) != 0 ? Mirroring::Vertical : Mirroring::Horizontal);
    result.image.battery_backed = (flags6 & 0x02U) != 0;
    result.image.has_trainer = (flags6 & 0x04U) != 0;

    std::uint64_t prg_size = 0;
    std::uint64_t chr_size = 0;

    if (nes20) {
        result.image.mapper_id |= static_cast<std::uint16_t>((bytes[8] & 0x0FU) << 8U);
        result.image.submapper = static_cast<std::uint8_t>(bytes[8] >> 4U);

        const auto prg_msb = static_cast<std::uint8_t>(bytes[9] & 0x0FU);
        const auto chr_msb = static_cast<std::uint8_t>(bytes[9] >> 4U);
        if (prg_msb == 0x0FU || chr_msb == 0x0FU) {
            result.error = "NES 2.0 exponent/multiplier ROM sizes are not supported yet";
            return result;
        }
        prg_size = static_cast<std::uint64_t>(
            read_u16_le(bytes[4], prg_msb) * kPrgUnit);
        chr_size = static_cast<std::uint64_t>(
            read_u16_le(bytes[5], chr_msb) * kChrUnit);
    } else {
        prg_size = static_cast<std::uint64_t>(bytes[4]) * kPrgUnit;
        chr_size = static_cast<std::uint64_t>(bytes[5]) * kChrUnit;
    }

    if (prg_size == 0) {
        result.error = "Cartridge declares no PRG ROM";
        return result;
    }

    const std::uint64_t trainer_size = result.image.has_trainer ? kTrainerSize : 0U;
    const std::uint64_t required = kHeaderSize + trainer_size + prg_size + chr_size;
    if (required > bytes.size()) {
        result.error = "ROM data is shorter than the sizes declared in its header";
        return result;
    }
    if (prg_size > std::numeric_limits<std::size_t>::max() ||
        chr_size > std::numeric_limits<std::size_t>::max()) {
        result.error = "Declared ROM size exceeds this platform's addressable memory";
        return result;
    }

    std::size_t offset = kHeaderSize;
    if (result.image.has_trainer) {
        result.image.trainer.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                    bytes.begin() + static_cast<std::ptrdiff_t>(offset + kTrainerSize));
        offset += kTrainerSize;
    }

    const auto prg_count = static_cast<std::size_t>(prg_size);
    const auto chr_count = static_cast<std::size_t>(chr_size);
    result.image.prg_rom.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                bytes.begin() + static_cast<std::ptrdiff_t>(offset + prg_count));
    offset += prg_count;

    if (chr_count == 0) {
        result.image.uses_chr_ram = true;
    } else {
        result.image.chr_rom.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                    bytes.begin() + static_cast<std::ptrdiff_t>(offset + chr_count));
    }

    if (nes20 && (bytes[12] & 0x03U) == 0x03U) {
        result.image.mirroring = Mirroring::MapperControlled;
    }

    return result;
}

bool mapper0_layout_supported(const CartridgeImage& image) noexcept {
    const bool valid_prg = image.prg_rom.size() == 16 * 1024 ||
                           image.prg_rom.size() == 32 * 1024;
    const bool valid_chr = image.chr_rom.empty() || image.chr_rom.size() == 8 * 1024;
    return image.mapper_id == 0 && valid_prg && valid_chr;
}

} // namespace nesrev
