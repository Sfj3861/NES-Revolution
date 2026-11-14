#pragma once

#include "nesrev/mapper0.hpp"

#include <array>
#include <cstdint>

namespace nesrev {

class Ppu2C02 {
public:
    static constexpr std::uint16_t Width = 256;
    static constexpr std::uint16_t Height = 240;

    explicit Ppu2C02(Mapper0& mapper) noexcept;

    [[nodiscard]] std::uint8_t cpu_read(std::uint16_t address) noexcept;
    void cpu_write(std::uint16_t address, std::uint8_t value) noexcept;
    void dma_write_oam(std::uint8_t value) noexcept;

    [[nodiscard]] std::uint8_t ppu_read(std::uint16_t address) const noexcept;
    void ppu_write(std::uint16_t address, std::uint8_t value) noexcept;

    void reset() noexcept;
    void clock() noexcept;
    void clock_cpu_cycles(std::uint32_t cycles) noexcept;

    [[nodiscard]] bool consume_nmi() noexcept;
    [[nodiscard]] bool consume_frame_ready() noexcept;
    [[nodiscard]] const std::array<std::uint32_t, Width * Height>& framebuffer() const noexcept {
        return framebuffer_;
    }
    [[nodiscard]] std::uint16_t scanline() const noexcept { return scanline_; }
    [[nodiscard]] std::uint16_t dot() const noexcept { return dot_; }

private:
    Mapper0& mapper_;
    std::array<std::uint8_t, 0x1000> nametable_ram_{};
    std::array<std::uint8_t, 32> palette_ram_{};
    std::array<std::uint8_t, 256> oam_{};
    std::array<std::uint32_t, Width * Height> framebuffer_{};
    // Predecoded sprite pixels for the current scanline (0xFF means transparent).
    std::array<std::uint8_t, Width> sprite_line_palette_{};
    std::array<std::uint8_t, Width> sprite_line_behind_{};
    std::array<std::uint8_t, Width> sprite_line_zero_{};

    std::uint8_t ctrl_{0};
    std::uint8_t mask_{0};
    std::uint8_t status_{0};
    std::uint8_t oam_address_{0};
    std::uint8_t open_bus_{0};
    std::uint8_t read_buffer_{0};
    std::uint16_t vram_address_{0};
    std::uint16_t temp_address_{0};
    std::uint8_t fine_x_{0};
    bool write_latch_{false};
    std::uint8_t scroll_x_{0};
    std::uint8_t scroll_y_{0};
    bool nmi_pending_{false};
    bool frame_ready_{false};
    std::uint16_t scanline_{0};
    std::uint16_t dot_{0};

    [[nodiscard]] std::uint16_t nametable_index(std::uint16_t address) const noexcept;
    [[nodiscard]] std::uint8_t palette_index(std::uint16_t address) const noexcept;
    void increment_vram_address() noexcept;
    void prepare_sprite_line(std::uint16_t y) noexcept;
    void render_pixel(std::uint16_t x, std::uint16_t y) noexcept;
    [[nodiscard]] std::uint32_t color(std::uint8_t palette_entry) const noexcept;
};

} // namespace nesrev
