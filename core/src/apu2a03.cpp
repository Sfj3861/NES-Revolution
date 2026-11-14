#include "nesrev/apu2a03.hpp"

#include <algorithm>
#include <utility>

namespace nesrev {
namespace {

constexpr std::array<std::array<std::uint8_t, 8>, 4> kDuty{{
    {{0,0,0,0,0,0,0,1}},
    {{0,0,0,0,0,0,1,1}},
    {{0,0,0,0,1,1,1,1}},
    {{1,1,1,1,1,1,0,0}}
}};

} // namespace

Apu2A03::Apu2A03(const std::uint32_t sample_rate) noexcept
    : sample_rate_(sample_rate == 0 ? 48000U : sample_rate) {
    reset();
}

void Apu2A03::reset() noexcept {
    sample_accumulator_ = 0;
    frame_cycle_ = 0;
    frame_step_ = 0;
    five_step_mode_ = false;
    irq_inhibit_ = false;
    frame_irq_ = false;
    even_cpu_cycle_ = false;
    enabled_ = 0;
    pulse1_ = Pulse{};
    pulse2_ = Pulse{};
    triangle_ = Triangle{};
    noise_ = Noise{};
    noise_.shift_register = 1;
    samples_.clear();
    previous_mix_input_ = 0.0F;
    previous_filtered_output_ = 0.0F;
}

void Apu2A03::write(const std::uint16_t address, const std::uint8_t value) noexcept {
    if (address >= 0x4000U && address <= 0x4003U) {
        const auto reg = static_cast<std::uint8_t>(address - 0x4000U);
        pulse1_.regs[reg] = value;
        if (reg == 3U) {
            if ((enabled_ & 0x01U) != 0) {
                pulse1_.length = LengthTable[(value >> 3U) & 0x1FU];
            }
            pulse1_.duty_step = 0;
            pulse1_.envelope_start = true;
        }
        if (reg == 2U || reg == 3U) {
            pulse1_.timer = static_cast<std::uint16_t>(
                pulse1_.regs[2] | ((pulse1_.regs[3] & 0x07U) << 8U));
        }
        return;
    }
    if (address >= 0x4004U && address <= 0x4007U) {
        const auto reg = static_cast<std::uint8_t>(address - 0x4004U);
        pulse2_.regs[reg] = value;
        if (reg == 3U) {
            if ((enabled_ & 0x02U) != 0) {
                pulse2_.length = LengthTable[(value >> 3U) & 0x1FU];
            }
            pulse2_.duty_step = 0;
            pulse2_.envelope_start = true;
        }
        if (reg == 2U || reg == 3U) {
            pulse2_.timer = static_cast<std::uint16_t>(
                pulse2_.regs[2] | ((pulse2_.regs[3] & 0x07U) << 8U));
        }
        return;
    }
    if (address >= 0x4008U && address <= 0x400BU) {
        const auto reg = static_cast<std::uint8_t>(address - 0x4008U);
        triangle_.regs[reg] = value;
        if (reg == 3U) {
            if ((enabled_ & 0x04U) != 0) {
                triangle_.length = LengthTable[(value >> 3U) & 0x1FU];
            }
            triangle_.linear_reload = true;
        }
        if (reg == 2U || reg == 3U) {
            triangle_.timer = static_cast<std::uint16_t>(
                triangle_.regs[2] | ((triangle_.regs[3] & 0x07U) << 8U));
        }
        return;
    }
    if (address >= 0x400CU && address <= 0x400FU) {
        const auto reg = static_cast<std::uint8_t>(address - 0x400CU);
        noise_.regs[reg] = value;
        if (reg == 3U) {
            if ((enabled_ & 0x08U) != 0) {
                noise_.length = LengthTable[(value >> 3U) & 0x1FU];
            }
            noise_.envelope_start = true;
        }
        return;
    }

    if (address == 0x4015U) {
        enabled_ = static_cast<std::uint8_t>(value & 0x0FU);
        if ((enabled_ & 0x01U) == 0) pulse1_.length = 0;
        if ((enabled_ & 0x02U) == 0) pulse2_.length = 0;
        if ((enabled_ & 0x04U) == 0) triangle_.length = 0;
        if ((enabled_ & 0x08U) == 0) noise_.length = 0;
        // The DMC enable bit is not implemented in this initial APU.
        return;
    }

    if (address == 0x4017U) {
        five_step_mode_ = (value & 0x80U) != 0;
        irq_inhibit_ = (value & 0x40U) != 0;
        if (irq_inhibit_) frame_irq_ = false;
        frame_cycle_ = 0;
        frame_step_ = 0;
        if (five_step_mode_) {
            quarter_frame();
            half_frame();
        }
    }
}

std::uint8_t Apu2A03::read_status() noexcept {
    std::uint8_t status = 0;
    if (pulse1_.length != 0) status |= 0x01U;
    if (pulse2_.length != 0) status |= 0x02U;
    if (triangle_.length != 0) status |= 0x04U;
    if (noise_.length != 0) status |= 0x08U;
    if (frame_irq_) status |= 0x40U;
    frame_irq_ = false;
    return status;
}

void Apu2A03::clock_envelope(const std::uint8_t control, std::uint8_t& divider,
                             std::uint8_t& decay, bool& start) noexcept {
    const auto period = static_cast<std::uint8_t>(control & 0x0FU);
    if (start) {
        start = false;
        decay = 15;
        divider = period;
    } else if (divider == 0) {
        divider = period;
        if (decay != 0) {
            --decay;
        } else if ((control & 0x20U) != 0) {
            decay = 15;
        }
    } else {
        --divider;
    }
}

std::uint8_t Apu2A03::envelope(Pulse& channel) noexcept {
    return (channel.regs[0] & 0x10U) != 0
        ? static_cast<std::uint8_t>(channel.regs[0] & 0x0FU)
        : channel.envelope_decay;
}

std::uint8_t Apu2A03::envelope(Noise& channel) noexcept {
    return (channel.regs[0] & 0x10U) != 0
        ? static_cast<std::uint8_t>(channel.regs[0] & 0x0FU)
        : channel.envelope_decay;
}

void Apu2A03::quarter_frame() noexcept {
    clock_envelope(pulse1_.regs[0], pulse1_.envelope_divider, pulse1_.envelope_decay, pulse1_.envelope_start);
    clock_envelope(pulse2_.regs[0], pulse2_.envelope_divider, pulse2_.envelope_decay, pulse2_.envelope_start);
    clock_envelope(noise_.regs[0], noise_.envelope_divider, noise_.envelope_decay, noise_.envelope_start);

    if (triangle_.linear_reload) {
        triangle_.linear_counter = static_cast<std::uint8_t>(triangle_.regs[0] & 0x7FU);
    } else if (triangle_.linear_counter != 0) {
        --triangle_.linear_counter;
    }
    if ((triangle_.regs[0] & 0x80U) == 0) {
        triangle_.linear_reload = false;
    }
}

void Apu2A03::half_frame() noexcept {
    if ((pulse1_.regs[0] & 0x20U) == 0 && pulse1_.length != 0) --pulse1_.length;
    if ((pulse2_.regs[0] & 0x20U) == 0 && pulse2_.length != 0) --pulse2_.length;
    if ((triangle_.regs[0] & 0x80U) == 0 && triangle_.length != 0) --triangle_.length;
    if ((noise_.regs[0] & 0x20U) == 0 && noise_.length != 0) --noise_.length;
}

void Apu2A03::clock_timers() noexcept {
    if (even_cpu_cycle_) {
        const auto clock_pulse = [](Pulse& pulse) noexcept {
            if (pulse.timer_counter == 0) {
                pulse.timer_counter = pulse.timer;
                pulse.duty_step = static_cast<std::uint8_t>((pulse.duty_step + 1U) & 7U);
            } else {
                --pulse.timer_counter;
            }
        };
        clock_pulse(pulse1_);
        clock_pulse(pulse2_);

        const auto period_index = static_cast<std::uint8_t>(noise_.regs[2] & 0x0FU);
        const auto noise_period = NoisePeriods[period_index];
        if (noise_.timer_counter == 0) {
            noise_.timer_counter = static_cast<std::uint16_t>(noise_period - 1U);
            const auto tap = (noise_.regs[2] & 0x80U) != 0 ? 6U : 1U;
            const auto feedback = static_cast<std::uint16_t>(
                (noise_.shift_register ^ (noise_.shift_register >> tap)) & 1U);
            noise_.shift_register = static_cast<std::uint16_t>(
                (noise_.shift_register >> 1U) | (feedback << 14U));
        } else {
            --noise_.timer_counter;
        }
    }

    if (triangle_.timer_counter == 0) {
        triangle_.timer_counter = triangle_.timer;
        if (triangle_.length != 0 && triangle_.linear_counter != 0 && triangle_.timer > 1U) {
            triangle_.sequence_step = static_cast<std::uint8_t>((triangle_.sequence_step + 1U) & 31U);
        }
    } else {
        --triangle_.timer_counter;
    }
}

float Apu2A03::mix() const noexcept {
    const auto pulse_output = [](const Pulse& pulse, const std::uint8_t bit) noexcept -> float {
        if (pulse.length == 0 || pulse.timer < 8U || pulse.timer > 0x7FFU ||
            kDuty[pulse.regs[0] >> 6U][pulse.duty_step] == 0) {
            return 0.0F;
        }
        const auto volume = (pulse.regs[0] & 0x10U) != 0
            ? (pulse.regs[0] & 0x0FU)
            : pulse.envelope_decay;
        return (bit != 0U) ? static_cast<float>(volume) : 0.0F;
    };

    const auto p1 = pulse_output(pulse1_, 1U);
    const auto p2 = pulse_output(pulse2_, 1U);
    const auto triangle = (triangle_.length != 0 && triangle_.linear_counter != 0)
        ? static_cast<float>(TriangleWave[triangle_.sequence_step]) : 0.0F;
    const bool noise_muted = noise_.length == 0 || (noise_.shift_register & 1U) != 0;
    const auto noise_volume = (noise_.regs[0] & 0x10U) != 0
        ? (noise_.regs[0] & 0x0FU) : noise_.envelope_decay;
    const auto n = noise_muted ? 0.0F : static_cast<float>(noise_volume);

    const auto pulse_sum = p1 + p2;
    const auto pulse_mix = pulse_sum == 0.0F ? 0.0F : 95.88F / ((8128.0F / pulse_sum) + 100.0F);
    const auto tnd_input = triangle / 8227.0F + n / 12241.0F;
    const auto tnd_mix = tnd_input == 0.0F ? 0.0F : 159.79F / ((1.0F / tnd_input) + 100.0F);
    return std::clamp(pulse_mix + tnd_mix, 0.0F, 1.0F);
}

void Apu2A03::emit_sample() noexcept {
    // The nonlinear NES mixer is unipolar. Sending (mix * 2 - 1) directly
    // to PCM made silence become -1.0 (full-scale DC), causing severe pops
    // and distortion. Remove the DC component with a gentle one-pole
    // high-pass filter, then scale the useful signal into PCM float range.
    const float input = mix();
    const float filtered = input - previous_mix_input_ +
                           0.995F * previous_filtered_output_;
    previous_mix_input_ = input;
    previous_filtered_output_ = filtered;
    samples_.push_back(std::clamp(filtered * 4.0F, -1.0F, 1.0F));
}

void Apu2A03::clock_cpu_cycles(const std::uint32_t cycles) noexcept {
    for (std::uint32_t i = 0; i < cycles; ++i) {
        even_cpu_cycle_ = !even_cpu_cycle_;
        clock_timers();

        ++frame_cycle_;
        if (!five_step_mode_) {
            if (frame_cycle_ == 3729U || frame_cycle_ == 11186U ||
                frame_cycle_ == 18641U || frame_cycle_ == 29829U) {
                quarter_frame();
            }
            if (frame_cycle_ == 7457U || frame_cycle_ == 22371U) {
                half_frame();
            }
            if (frame_cycle_ >= 29830U) {
                if (!irq_inhibit_) frame_irq_ = true;
                frame_cycle_ = 0;
            }
        } else {
            if (frame_cycle_ == 3729U || frame_cycle_ == 11186U ||
                frame_cycle_ == 18641U || frame_cycle_ == 37281U) {
                quarter_frame();
            }
            if (frame_cycle_ == 7457U || frame_cycle_ == 22371U ||
                frame_cycle_ == 37281U) {
                half_frame();
            }
            if (frame_cycle_ >= 37282U) frame_cycle_ = 0;
        }

        sample_accumulator_ += sample_rate_;
        if (sample_accumulator_ >= CpuClock) {
            sample_accumulator_ -= CpuClock;
            emit_sample();
        }
    }
}

std::vector<float> Apu2A03::take_samples() {
    std::vector<float> result;
    result.swap(samples_);
    return result;
}

} // namespace nesrev
