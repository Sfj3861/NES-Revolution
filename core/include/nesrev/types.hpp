#pragma once

#include <cstdint>

namespace nesrev {

using CpuCycle = std::uint64_t;
using HostTimestampNs = std::uint64_t;

enum class Region : std::uint8_t {
    Ntsc,
    Pal,
    Dendy,
    Unknown
};

enum class Mirroring : std::uint8_t {
    Horizontal,
    Vertical,
    FourScreen,
    MapperControlled
};

} // namespace nesrev
