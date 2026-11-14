#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace nesrev {

// Initial Ricoh 2A03 APU. Implements pulse 1/2, triangle and noise channels.
// DMC DMA and cycle-exact analog behavior are intentionally not included yet.
class Apu2A03 {
public:
    explicit Apu2A03(std::uint32_t sample_rate = 48000U) noexcept;

    void reset() noexcept;
    void write(std::uint16_t address, std::uint8_t value) noexcept;
    [[nodiscard]] std::uint8_t read_status() noexcept;
    void clock_cpu_cycles(std::uint32_t cycles) noexcept;

    [[nodiscard]] std::vector<float> take_samples();
    [[nodiscard]] std::uint32_t sample_rate() const noexcept { return sample_rate_; }

private:
    struct Pulse {
        std::array<std::uint8_t, 4> regs{};
        std::uint16_t timer{0};
        std::uint16_t timer_counter{0};
        std::uint8_t duty_step{0};
        std::uint8_t length{0};
        std::uint8_t envelope_divider{0};
        std::uint8_t envelope_decay{0};
        bool envelope_start{false};
    };

    struct Triangle {
        std::array<std::uint8_t, 4> regs{};
        std::uint16_t timer{0};
        std::uint16_t timer_counter{0};
        std::uint8_t sequence_step{0};
        std::uint8_t length{0};
        std::uint8_t linear_counter{0};
        bool linear_reload{false};
    };

    struct Noise {
        std::array<std::uint8_t, 4> regs{};
        std::uint16_t timer_counter{0};
        std::uint16_t shift_register{1};
        std::uint8_t length{0};
        std::uint8_t envelope_divider{0};
        std::uint8_t envelope_decay{0};
        bool envelope_start{false};
    };

    static constexpr std::uint32_t CpuClock = 1789773U;
    static constexpr std::array<std::uint8_t, 32> LengthTable{
        10,254,20,2,40,4,80,6,160,8,60,10,14,12,26,14,
        12,16,24,18,48,20,96,22,192,24,72,26,16,28,32,30
    };
    static constexpr std::array<std::uint16_t, 16> NoisePeriods{
        4,8,16,32,64,96,128,160,202,254,380,508,762,1016,2034,4068
    };
    static constexpr std::array<std::uint8_t, 32> TriangleWave{
        15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
    };

    std::uint32_t sample_rate_{48000U};
    std::uint32_t sample_accumulator_{0};
    std::uint32_t frame_cycle_{0};
    std::uint8_t frame_step_{0};
    bool five_step_mode_{false};
    bool irq_inhibit_{false};
    bool frame_irq_{false};
    bool even_cpu_cycle_{false};
    std::uint8_t enabled_{0};
    Pulse pulse1_{};
    Pulse pulse2_{};
    Triangle triangle_{};
    Noise noise_{};
    std::vector<float> samples_;
    float previous_mix_input_{0.0F};
    float previous_filtered_output_{0.0F};

    void quarter_frame() noexcept;
    void half_frame() noexcept;
    void clock_timers() noexcept;
    void emit_sample() noexcept;
    [[nodiscard]] std::uint8_t envelope(Pulse& channel) noexcept;
    [[nodiscard]] std::uint8_t envelope(Noise& channel) noexcept;
    [[nodiscard]] float mix() const noexcept;
    static void clock_envelope(std::uint8_t control, std::uint8_t& divider,
                               std::uint8_t& decay, bool& start) noexcept;
};

} // namespace nesrev
