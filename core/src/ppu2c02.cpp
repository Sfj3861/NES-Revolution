#include "nesrev/ppu2c02.hpp"

#include <algorithm>
#include <array>

namespace nesrev {
namespace {

constexpr std::array<std::uint32_t, 64> kNesPalette{
    0xFF666666,0xFF002A88,0xFF1412A7,0xFF3B00A4,0xFF5C007E,0xFF6E0040,0xFF6C0600,0xFF561D00,
    0xFF333500,0xFF0B4800,0xFF005200,0xFF004F08,0xFF00404D,0xFF000000,0xFF000000,0xFF000000,
    0xFFADADAD,0xFF155FD9,0xFF4240FF,0xFF7527FE,0xFFA01ACC,0xFFB71E7B,0xFFB53120,0xFF994E00,
    0xFF6B6D00,0xFF388700,0xFF0C9300,0xFF008F32,0xFF007C8D,0xFF000000,0xFF000000,0xFF000000,
    0xFFFFFEFF,0xFF64B0FF,0xFF9290FF,0xFFC676FF,0xFFF36AFF,0xFFFE6ECC,0xFFFE8170,0xFFE89E22,
    0xFFBCBE00,0xFF88D800,0xFF5CE430,0xFF45E082,0xFF48CDDE,0xFF4F4F4F,0xFF000000,0xFF000000,
    0xFFFFFEFF,0xFFC0DFFF,0xFFD3D2FF,0xFFE8C8FF,0xFFFBC2FF,0xFFFEC4EA,0xFFFECCC5,0xFFF7D8A5,
    0xFFE4E594,0xFFCFEE96,0xFFBDF4AB,0xFFB3F3CC,0xFFB5EBF2,0xFFB8B8B8,0xFF000000,0xFF000000
};

} // namespace

Ppu2C02::Ppu2C02(Mapper0& mapper) noexcept : mapper_(mapper) {
    framebuffer_.fill(kNesPalette[0]);
}

std::uint16_t Ppu2C02::nametable_index(const std::uint16_t address) const noexcept {
    auto relative = static_cast<std::uint16_t>((address - 0x2000U) & 0x0FFFU);
    const auto table = static_cast<std::uint16_t>(relative / 0x0400U);
    const auto offset = static_cast<std::uint16_t>(relative & 0x03FFU);
    std::uint16_t physical = 0;
    switch (mapper_.mirroring()) {
    case Mirroring::Vertical:
        physical = static_cast<std::uint16_t>(table & 1U);
        break;
    case Mirroring::FourScreen:
        physical = table;
        break;
    case Mirroring::Horizontal:
    case Mirroring::MapperControlled:
    default:
        physical = static_cast<std::uint16_t>(table >> 1U);
        break;
    }
    return static_cast<std::uint16_t>(physical * 0x0400U + offset);
}

std::uint8_t Ppu2C02::palette_index(const std::uint16_t address) const noexcept {
    auto index = static_cast<std::uint8_t>((address - 0x3F00U) & 0x1FU);
    if ((index & 0x13U) == 0x10U) {
        index = static_cast<std::uint8_t>(index - 0x10U);
    }
    return index;
}

std::uint8_t Ppu2C02::ppu_read(const std::uint16_t raw_address) const noexcept {
    const auto address = static_cast<std::uint16_t>(raw_address & 0x3FFFU);
    if (address < 0x2000U) {
        return mapper_.ppu_read(address);
    }
    if (address < 0x3F00U) {
        const auto normalized = address >= 0x3000U
            ? static_cast<std::uint16_t>(address - 0x1000U)
            : address;
        return nametable_ram_[nametable_index(normalized)];
    }
    return static_cast<std::uint8_t>(palette_ram_[palette_index(address)] & 0x3FU);
}

void Ppu2C02::ppu_write(const std::uint16_t raw_address, const std::uint8_t value) noexcept {
    const auto address = static_cast<std::uint16_t>(raw_address & 0x3FFFU);
    if (address < 0x2000U) {
        mapper_.ppu_write(address, value);
        return;
    }
    if (address < 0x3F00U) {
        const auto normalized = address >= 0x3000U
            ? static_cast<std::uint16_t>(address - 0x1000U)
            : address;
        nametable_ram_[nametable_index(normalized)] = value;
        return;
    }
    palette_ram_[palette_index(address)] = static_cast<std::uint8_t>(value & 0x3FU);
}

std::uint8_t Ppu2C02::cpu_read(const std::uint16_t address) noexcept {
    const auto reg = static_cast<std::uint8_t>(address & 0x0007U);
    std::uint8_t result = open_bus_;
    switch (reg) {
    case 0x2: // PPUSTATUS
        result = static_cast<std::uint8_t>((status_ & 0xE0U) | (open_bus_ & 0x1FU));
        status_ &= static_cast<std::uint8_t>(~0x80U);
        write_latch_ = false;
        nmi_pending_ = false;
        break;
    case 0x4: // OAMDATA
        result = oam_[oam_address_];
        break;
    case 0x7: { // PPUDATA
        const auto current = static_cast<std::uint16_t>(vram_address_ & 0x3FFFU);
        if (current >= 0x3F00U) {
            result = ppu_read(current);
            read_buffer_ = ppu_read(static_cast<std::uint16_t>(current - 0x1000U));
        } else {
            result = read_buffer_;
            read_buffer_ = ppu_read(current);
        }
        increment_vram_address();
        break;
    }
    default:
        break;
    }
    open_bus_ = result;
    return result;
}

void Ppu2C02::cpu_write(const std::uint16_t address, const std::uint8_t value) noexcept {
    open_bus_ = value;
    const auto reg = static_cast<std::uint8_t>(address & 0x0007U);
    switch (reg) {
    case 0x0: // PPUCTRL
        ctrl_ = value;
        if ((ctrl_ & 0x80U) != 0 && (status_ & 0x80U) != 0) {
            nmi_pending_ = true;
        }
        temp_address_ = static_cast<std::uint16_t>((temp_address_ & 0xF3FFU) |
            ((static_cast<std::uint16_t>(value) & 0x03U) << 10U));
        break;
    case 0x1: // PPUMASK
        mask_ = value;
        break;
    case 0x3: // OAMADDR
        oam_address_ = value;
        break;
    case 0x4: // OAMDATA
        oam_[oam_address_++] = value;
        break;
    case 0x5: // PPUSCROLL
        if (!write_latch_) {
            scroll_x_ = value;
            fine_x_ = static_cast<std::uint8_t>(value & 0x07U);
            temp_address_ = static_cast<std::uint16_t>((temp_address_ & 0xFFE0U) | (value >> 3U));
        } else {
            scroll_y_ = value;
            temp_address_ = static_cast<std::uint16_t>((temp_address_ & 0x8C1FU) |
                ((static_cast<std::uint16_t>(value) & 0xF8U) << 2U) |
                ((static_cast<std::uint16_t>(value) & 0x07U) << 12U));
        }
        write_latch_ = !write_latch_;
        break;
    case 0x6: // PPUADDR
        if (!write_latch_) {
            temp_address_ = static_cast<std::uint16_t>((temp_address_ & 0x00FFU) |
                ((static_cast<std::uint16_t>(value) & 0x3FU) << 8U));
        } else {
            temp_address_ = static_cast<std::uint16_t>((temp_address_ & 0x7F00U) | value);
            vram_address_ = temp_address_;
        }
        write_latch_ = !write_latch_;
        break;
    case 0x7: // PPUDATA
        ppu_write(vram_address_, value);
        increment_vram_address();
        break;
    default:
        break;
    }
}

void Ppu2C02::increment_vram_address() noexcept {
    vram_address_ = static_cast<std::uint16_t>((vram_address_ +
        ((ctrl_ & 0x04U) != 0 ? 32U : 1U)) & 0x7FFFU);
}

bool Ppu2C02::consume_nmi() noexcept {
    const bool pending = nmi_pending_;
    nmi_pending_ = false;
    return pending;
}

bool Ppu2C02::consume_frame_ready() noexcept {
    const bool ready = frame_ready_;
    frame_ready_ = false;
    return ready;
}

std::uint32_t Ppu2C02::color(const std::uint8_t palette_entry) const noexcept {
    auto index = static_cast<std::uint8_t>(palette_entry & 0x3FU);
    if ((mask_ & 0x01U) != 0) {
        index &= 0x30U;
    }
    return kNesPalette[index];
}

void Ppu2C02::render_pixel(const std::uint16_t x, const std::uint16_t y) noexcept {
    const bool show_background = (mask_ & 0x08U) != 0 && (x >= 8U || (mask_ & 0x02U) != 0);
    const bool show_sprites = (mask_ & 0x10U) != 0 && (x >= 8U || (mask_ & 0x04U) != 0);
    std::uint8_t background_pixel = 0;
    std::uint8_t background_palette = 0;

    if (show_background) {
        const auto world_x = static_cast<std::uint16_t>((x + scroll_x_) % 512U);
        const auto world_y = static_cast<std::uint16_t>((y + scroll_y_) % 480U);
        const auto base_nt = static_cast<std::uint8_t>(ctrl_ & 0x03U);
        const auto nt_x = static_cast<std::uint8_t>((base_nt & 1U) + (world_x / 256U));
        const auto nt_y = static_cast<std::uint8_t>(((base_nt >> 1U) & 1U) + (world_y / 240U));
        const auto table = static_cast<std::uint16_t>(((nt_y & 1U) * 2U + (nt_x & 1U)) * 0x0400U);
        const auto local_x = static_cast<std::uint16_t>(world_x % 256U);
        const auto local_y = static_cast<std::uint16_t>(world_y % 240U);
        const auto tile_x = static_cast<std::uint16_t>(local_x / 8U);
        const auto tile_y = static_cast<std::uint16_t>(local_y / 8U);
        const auto tile = ppu_read(static_cast<std::uint16_t>(0x2000U + table + tile_y * 32U + tile_x));
        const auto fine_y = static_cast<std::uint16_t>(local_y & 7U);
        const auto pattern_base = (ctrl_ & 0x10U) != 0 ? 0x1000U : 0x0000U;
        const auto pattern_address = static_cast<std::uint16_t>(pattern_base + tile * 16U + fine_y);
        const auto bit = static_cast<std::uint8_t>(7U - (local_x & 7U));
        const auto low = static_cast<std::uint8_t>((ppu_read(pattern_address) >> bit) & 1U);
        const auto high = static_cast<std::uint8_t>((ppu_read(static_cast<std::uint16_t>(pattern_address + 8U)) >> bit) & 1U);
        background_pixel = static_cast<std::uint8_t>(low | (high << 1U));

        if (background_pixel != 0) {
            const auto attribute_address = static_cast<std::uint16_t>(
                0x23C0U + table + (tile_y / 4U) * 8U + (tile_x / 4U));
            const auto attribute = ppu_read(attribute_address);
            const auto shift = static_cast<std::uint8_t>(((tile_y & 2U) << 1U) | (tile_x & 2U));
            background_palette = static_cast<std::uint8_t>((attribute >> shift) & 0x03U);
        }
    }

    std::uint8_t output_color = ppu_read(0x3F00U);
    if (background_pixel != 0) {
        output_color = ppu_read(static_cast<std::uint16_t>(
            0x3F00U + background_palette * 4U + background_pixel));
    }

    if (show_sprites) {
        const auto sprite_height = (ctrl_ & 0x20U) != 0 ? 16U : 8U;
        for (std::uint8_t i = 0; i < 64U; ++i) {
            const auto base = static_cast<std::uint16_t>(i) * 4U;
            const auto top = static_cast<std::uint16_t>(oam_[base]) + 1U;
            const auto left = static_cast<std::uint16_t>(oam_[base + 3U]);
            if (y < top || y >= top + sprite_height || x < left || x >= left + 8U) {
                continue;
            }

            const auto attributes = oam_[base + 2U];
            auto row = static_cast<std::uint8_t>(y - top);
            auto column = static_cast<std::uint8_t>(x - left);
            if ((attributes & 0x80U) != 0) {
                row = static_cast<std::uint8_t>(sprite_height - 1U - row);
            }
            if ((attributes & 0x40U) != 0) {
                column = static_cast<std::uint8_t>(7U - column);
            }

            std::uint16_t pattern_address = 0;
            if (sprite_height == 16U) {
                const auto tile = static_cast<std::uint8_t>(oam_[base + 1U] & 0xFEU);
                const auto table_base = (oam_[base + 1U] & 1U) != 0 ? 0x1000U : 0x0000U;
                pattern_address = static_cast<std::uint16_t>(table_base +
                    (tile + (row / 8U)) * 16U + (row & 7U));
            } else {
                const auto table_base = (ctrl_ & 0x08U) != 0 ? 0x1000U : 0x0000U;
                pattern_address = static_cast<std::uint16_t>(table_base +
                    oam_[base + 1U] * 16U + row);
            }

            const auto bit = static_cast<std::uint8_t>(7U - column);
            const auto low = static_cast<std::uint8_t>((ppu_read(pattern_address) >> bit) & 1U);
            const auto high = static_cast<std::uint8_t>((ppu_read(static_cast<std::uint16_t>(pattern_address + 8U)) >> bit) & 1U);
            const auto sprite_pixel = static_cast<std::uint8_t>(low | (high << 1U));
            if (sprite_pixel == 0) {
                continue;
            }

            if (i == 0U && background_pixel != 0U && x < 255U) {
                status_ |= 0x40U;
            }

            const bool behind_background = (attributes & 0x20U) != 0;
            if (background_pixel == 0U || !behind_background) {
                const auto palette = static_cast<std::uint8_t>(attributes & 0x03U);
                output_color = ppu_read(static_cast<std::uint16_t>(
                    0x3F10U + palette * 4U + sprite_pixel));
            }
            break;
        }
    }

    framebuffer_[static_cast<std::size_t>(y) * Width + x] = color(output_color);
}

void Ppu2C02::clock() noexcept {
    if (scanline_ == 261U && dot_ == 1U) {
        status_ &= static_cast<std::uint8_t>(~0xE0U);
        nmi_pending_ = false;
    }

    if (scanline_ == 241U && dot_ == 1U) {
        status_ |= 0x80U;
        if ((ctrl_ & 0x80U) != 0) {
            nmi_pending_ = true;
        }
    }

    if (scanline_ < Height && dot_ >= 1U && dot_ <= Width) {
        render_pixel(static_cast<std::uint16_t>(dot_ - 1U), scanline_);
    }

    ++dot_;
    if (dot_ >= 341U) {
        dot_ = 0;
        ++scanline_;
        if (scanline_ >= 262U) {
            scanline_ = 0;
            frame_ready_ = true;
        }
    }
}

void Ppu2C02::clock_cpu_cycles(const std::uint32_t cycles) noexcept {
    for (std::uint32_t i = 0; i < cycles * 3U; ++i) {
        clock();
    }
}

} // namespace nesrev
