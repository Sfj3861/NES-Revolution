#include "nesrev/cpu6502.hpp"
#include "nesrev/ppu2c02.hpp"

namespace nesrev {

std::uint8_t CpuBus::read(std::uint16_t address) const noexcept {
    if (address < 0x2000U) {
        return ram_[address & 0x07FFU];
    }
    if (address < 0x4000U) {
        return ppu_ != nullptr
            ? ppu_->cpu_read(static_cast<std::uint16_t>(0x2000U + (address & 0x0007U)))
            : memory_[0x2000U + (address & 0x0007U)];
    }
    if (address >= 0x6000U && address < 0x8000U) {
        return prg_ram_[address - 0x6000U];
    }
    if (address >= 0x8000U && mapper0_.loaded()) {
        return mapper0_.cpu_read(address);
    }
    return memory_[address];
}

void CpuBus::write(std::uint16_t address, std::uint8_t value) noexcept {
    if (address < 0x2000U) {
        ram_[address & 0x07FFU] = value;
        return;
    }
    if (address < 0x4000U) {
        if (ppu_ != nullptr) {
            ppu_->cpu_write(static_cast<std::uint16_t>(0x2000U + (address & 0x0007U)), value);
        } else {
            memory_[0x2000U + (address & 0x0007U)] = value;
        }
        return;
    }
    if (address >= 0x6000U && address < 0x8000U) {
        prg_ram_[address - 0x6000U] = value;
        return;
    }
    if (address >= 0x8000U && mapper0_.loaded()) {
        mapper0_.cpu_write(address, value);
        return;
    }
    memory_[address] = value;
}

void CpuBus::load(std::uint16_t address, const std::uint8_t* data, std::uint32_t size) noexcept {
    if (data == nullptr) {
        return;
    }
    for (std::uint32_t i = 0; i < size && static_cast<std::uint32_t>(address) + i < 0x10000U; ++i) {
        memory_[static_cast<std::uint16_t>(address + i)] = data[i];
    }
}

bool CpuBus::insert_cartridge(const CartridgeImage& image) {
    if (!mapper0_.load(image)) {
        return false;
    }
    prg_ram_.fill(0);
    return true;
}

void CpuBus::eject_cartridge() noexcept {
    mapper0_.unload();
    prg_ram_.fill(0);
}

std::uint8_t CpuBus::ppu_read_cartridge(const std::uint16_t address) const noexcept {
    return mapper0_.ppu_read(address);
}

void CpuBus::ppu_write_cartridge(const std::uint16_t address, const std::uint8_t value) noexcept {
    mapper0_.ppu_write(address, value);
}

void CpuBus::tick_cpu_cycles(const std::uint32_t cycles) noexcept {
    if (ppu_ != nullptr) {
        ppu_->clock_cpu_cycles(cycles);
    }
}

bool CpuBus::consume_ppu_nmi() noexcept {
    return ppu_ != nullptr && ppu_->consume_nmi();
}

void CpuBus::set_reset_vector(std::uint16_t address) noexcept {
    memory_[0xFFFC] = static_cast<std::uint8_t>(address);
    memory_[0xFFFD] = static_cast<std::uint8_t>(address >> 8);
}
void CpuBus::set_irq_vector(std::uint16_t address) noexcept {
    memory_[0xFFFE] = static_cast<std::uint8_t>(address);
    memory_[0xFFFF] = static_cast<std::uint8_t>(address >> 8);
}
void CpuBus::set_nmi_vector(std::uint16_t address) noexcept {
    memory_[0xFFFA] = static_cast<std::uint8_t>(address);
    memory_[0xFFFB] = static_cast<std::uint8_t>(address >> 8);
}

std::uint8_t Cpu6502::fetch() noexcept { return bus_.read(state_.pc++); }
std::uint16_t Cpu6502::fetch16() noexcept {
    const auto lo = fetch(); const auto hi = fetch();
    return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
}
void Cpu6502::set_flag(std::uint8_t f, bool v) noexcept {
    if (v) state_.status |= f; else state_.status &= static_cast<std::uint8_t>(~f);
    state_.status |= U;
}
bool Cpu6502::flag(std::uint8_t f) const noexcept { return (state_.status & f) != 0; }
void Cpu6502::set_nz(std::uint8_t v) noexcept { set_flag(Z, v == 0); set_flag(N, (v & 0x80) != 0); }
void Cpu6502::push(std::uint8_t v) noexcept { bus_.write(static_cast<std::uint16_t>(0x0100 | state_.sp--), v); }
std::uint8_t Cpu6502::pull() noexcept { return bus_.read(static_cast<std::uint16_t>(0x0100 | ++state_.sp)); }
void Cpu6502::reset() noexcept {
    state_ = CpuState{};
    state_.sp = 0xFD; state_.status = static_cast<std::uint8_t>(I | U);
    state_.pc = static_cast<std::uint16_t>(bus_.read(0xFFFC) |
        (static_cast<std::uint16_t>(bus_.read(0xFFFD)) << 8));
    state_.cycles = 7; nmi_pending_ = false; irq_line_ = false;
}
void Cpu6502::interrupt(std::uint16_t vector, bool is_brk) noexcept {
    push(static_cast<std::uint8_t>(state_.pc >> 8));
    push(static_cast<std::uint8_t>(state_.pc));
    auto p = static_cast<std::uint8_t>((state_.status | U) & static_cast<std::uint8_t>(~B));
    if (is_brk) p |= B;
    push(p); set_flag(I, true);
    state_.pc = static_cast<std::uint16_t>(bus_.read(vector) |
        (static_cast<std::uint16_t>(bus_.read(static_cast<std::uint16_t>(vector + 1))) << 8));
    state_.cycles += 7;
    bus_.tick_cpu_cycles(7);
}
void Cpu6502::adc(std::uint8_t v) noexcept {
    const auto sum = static_cast<std::uint16_t>(state_.a) + v + (flag(C) ? 1U : 0U);
    const auto result = static_cast<std::uint8_t>(sum);
    set_flag(C, sum > 0xFF);
    set_flag(V, ((~(state_.a ^ v) & (state_.a ^ result)) & 0x80) != 0);
    state_.a = result; set_nz(state_.a);
}
void Cpu6502::compare(std::uint8_t lhs, std::uint8_t rhs) noexcept {
    set_flag(C, lhs >= rhs); set_nz(static_cast<std::uint8_t>(lhs - rhs));
}

bool Cpu6502::step() noexcept {
    if (bus_.consume_ppu_nmi()) {
        nmi_pending_ = true;
    }
    if (nmi_pending_) { nmi_pending_ = false; interrupt(0xFFFA, false); return true; }
    if (irq_line_ && !flag(I)) { interrupt(0xFFFE, false); return true; }
    const auto start = state_.pc;
    const auto op = fetch();
    std::uint8_t value = 0;
    std::uint16_t address = 0;
    std::uint8_t cost = 0;
    const auto zp = [this]() noexcept { return static_cast<std::uint16_t>(fetch()); };
    const auto zpx = [this]() noexcept { return static_cast<std::uint16_t>(static_cast<std::uint8_t>(fetch() + state_.x)); };
    const auto zpy = [this]() noexcept { return static_cast<std::uint16_t>(static_cast<std::uint8_t>(fetch() + state_.y)); };
    const auto absx = [this]() noexcept { return static_cast<std::uint16_t>(fetch16() + state_.x); };
    const auto absy = [this]() noexcept { return static_cast<std::uint16_t>(fetch16() + state_.y); };
    const auto indx = [this]() noexcept {
        const auto p = static_cast<std::uint8_t>(fetch() + state_.x);
        const auto lo = bus_.read(p);
        const auto hi = bus_.read(static_cast<std::uint8_t>(p + 1));
        return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
    };
    bool indirect_y_crossed = false;
    const auto indy = [this, &indirect_y_crossed]() noexcept {
        const auto p = fetch();
        const auto lo = bus_.read(p);
        const auto hi = bus_.read(static_cast<std::uint8_t>(p + 1));
        const auto base = static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
        const auto result = static_cast<std::uint16_t>(base + state_.y);
        indirect_y_crossed = (base & 0xFF00) != (result & 0xFF00);
        return result;
    };
    const auto page_crossed = [](std::uint16_t a, std::uint16_t b) noexcept { return (a & 0xFF00) != (b & 0xFF00); };
    switch (op) {
    case 0xEA: cost=2; break; // NOP
    case 0xA9: state_.a=fetch(); set_nz(state_.a); cost=2; break;
    case 0xA5: state_.a=bus_.read(fetch()); set_nz(state_.a); cost=3; break;
    case 0xAD: state_.a=bus_.read(fetch16()); set_nz(state_.a); cost=4; break;
    case 0xBD: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.x); state_.a=bus_.read(address); set_nz(state_.a); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0xB9: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.y); state_.a=bus_.read(address); set_nz(state_.a); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0xA2: state_.x=fetch(); set_nz(state_.x); cost=2; break;
    case 0xA0: state_.y=fetch(); set_nz(state_.y); cost=2; break;
    case 0x85: bus_.write(fetch(),state_.a); cost=3; break;
    case 0x8D: address=fetch16(); bus_.write(address,state_.a); cost=4; break;
    case 0x9D: address=static_cast<std::uint16_t>(fetch16()+state_.x); bus_.write(address,state_.a); cost=5; break;
    case 0x99: address=static_cast<std::uint16_t>(fetch16()+state_.y); bus_.write(address,state_.a); cost=5; break;
    case 0x86: bus_.write(fetch(),state_.x); cost=3; break;
    case 0x84: bus_.write(fetch(),state_.y); cost=3; break;
    case 0xAA: state_.x=state_.a; set_nz(state_.x); cost=2; break;
    case 0xA8: state_.y=state_.a; set_nz(state_.y); cost=2; break;
    case 0x8A: state_.a=state_.x; set_nz(state_.a); cost=2; break;
    case 0x98: state_.a=state_.y; set_nz(state_.a); cost=2; break;
    case 0xBA: state_.x=state_.sp; set_nz(state_.x); cost=2; break;
    case 0x9A: state_.sp=state_.x; cost=2; break;
    case 0xE8: ++state_.x; set_nz(state_.x); cost=2; break;
    case 0xC8: ++state_.y; set_nz(state_.y); cost=2; break;
    case 0xCA: --state_.x; set_nz(state_.x); cost=2; break;
    case 0x88: --state_.y; set_nz(state_.y); cost=2; break;
    case 0x69: adc(fetch()); cost=2; break;
    case 0xE9: case 0xEB: adc(static_cast<std::uint8_t>(~fetch())); cost=2; break;
    case 0xC9: compare(state_.a,fetch()); cost=2; break;
    case 0xE0: compare(state_.x,fetch()); cost=2; break;
    case 0xC0: compare(state_.y,fetch()); cost=2; break;
    case 0x29: state_.a &= fetch(); set_nz(state_.a); cost=2; break;
    case 0x09: state_.a |= fetch(); set_nz(state_.a); cost=2; break;
    case 0x49: state_.a ^= fetch(); set_nz(state_.a); cost=2; break;
    case 0x18: set_flag(C,false); cost=2; break;
    case 0x38: set_flag(C,true); cost=2; break;
    case 0x58: set_flag(I,false); cost=2; break;
    case 0x78: set_flag(I,true); cost=2; break;
    case 0xD8: set_flag(D,false); cost=2; break; // 2A03 ignores decimal arithmetic
    case 0xF8: set_flag(D,true); cost=2; break;
    case 0xB8: set_flag(V,false); cost=2; break;
    case 0x48: push(state_.a); cost=3; break;
    case 0x68: state_.a=pull(); set_nz(state_.a); cost=4; break;
    case 0x08: push(static_cast<std::uint8_t>(state_.status|B|U)); cost=3; break;
    case 0x28: state_.status=static_cast<std::uint8_t>((pull()|U)&static_cast<std::uint8_t>(~B)); cost=4; break;
    case 0x4C: state_.pc=fetch16(); cost=3; break;
    case 0x6C: {
        const auto ptr=fetch16();
        const auto lo=bus_.read(ptr);
        const auto hi=bus_.read(static_cast<std::uint16_t>((ptr&0xFF00)|((ptr+1)&0x00FF)));
        state_.pc=static_cast<std::uint16_t>(lo|(static_cast<std::uint16_t>(hi)<<8)); cost=5; break;
    }
    case 0x20: address=fetch16(); push(static_cast<std::uint8_t>((state_.pc-1)>>8)); push(static_cast<std::uint8_t>(state_.pc-1)); state_.pc=address; cost=6; break;
    case 0x60: { const auto lo=pull(); const auto hi=pull(); state_.pc=static_cast<std::uint16_t>((lo|(static_cast<std::uint16_t>(hi)<<8))+1); cost=6; break; }
    case 0x00: ++state_.pc; interrupt(0xFFFE,true); return true;
    case 0x40: { state_.status=static_cast<std::uint8_t>((pull()|U)&static_cast<std::uint8_t>(~B)); const auto lo=pull(); const auto hi=pull(); state_.pc=static_cast<std::uint16_t>(lo|(static_cast<std::uint16_t>(hi)<<8)); cost=6; break; }
    case 0x10: case 0x30: case 0x50: case 0x70: case 0x90: case 0xB0: case 0xD0: case 0xF0: {
        const bool take = op==0x10 ? !flag(N) : op==0x30 ? flag(N) : op==0x50 ? !flag(V) :
            op==0x70 ? flag(V) : op==0x90 ? !flag(C) : op==0xB0 ? flag(C) : op==0xD0 ? !flag(Z) : flag(Z);
        const auto rel=static_cast<std::int8_t>(fetch()); cost=2;
        if(take){const auto old=state_.pc; state_.pc=static_cast<std::uint16_t>(state_.pc+rel); ++cost; if((old&0xFF00)!=(state_.pc&0xFF00)) ++cost;} break;
    }
    case 0x24: address=fetch(); value=bus_.read(address); set_flag(Z,(state_.a&value)==0); set_flag(V,(value&0x40)!=0); set_flag(N,(value&0x80)!=0); cost=3; break;
    case 0x2C: address=fetch16(); value=bus_.read(address); set_flag(Z,(state_.a&value)==0); set_flag(V,(value&0x40)!=0); set_flag(N,(value&0x80)!=0); cost=4; break;
    case 0xE6: address=fetch(); value=static_cast<std::uint8_t>(bus_.read(address)+1); bus_.write(address,value); set_nz(value); cost=5; break;
    case 0xC6: address=fetch(); value=static_cast<std::uint8_t>(bus_.read(address)-1); bus_.write(address,value); set_nz(value); cost=5; break;
    // Additional official 2A03 opcodes and addressing modes.
    case 0xA6: state_.x=bus_.read(zp()); set_nz(state_.x); cost=3; break;
    case 0xB6: state_.x=bus_.read(zpy()); set_nz(state_.x); cost=4; break;
    case 0xAE: state_.x=bus_.read(fetch16()); set_nz(state_.x); cost=4; break;
    case 0xBE: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.y); state_.x=bus_.read(address); set_nz(state_.x); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0xB4: state_.y=bus_.read(zpx()); set_nz(state_.y); cost=4; break;
    case 0xAC: state_.y=bus_.read(fetch16()); set_nz(state_.y); cost=4; break;
    case 0xBC: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.x); state_.y=bus_.read(address); set_nz(state_.y); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0xB5: state_.a=bus_.read(zpx()); set_nz(state_.a); cost=4; break;
    case 0xA1: state_.a=bus_.read(indx()); set_nz(state_.a); cost=6; break;
    case 0xB1: address=indy(); state_.a=bus_.read(address); set_nz(state_.a); cost=static_cast<std::uint8_t>(5+(indirect_y_crossed?1:0)); break;
    case 0xA3: state_.a=bus_.read(indx()); state_.x=state_.a; set_nz(state_.a); cost=6; break; // unofficial LAX
    case 0xA7: state_.a=bus_.read(zp()); state_.x=state_.a; set_nz(state_.a); cost=3; break;
    case 0xAF: state_.a=bus_.read(fetch16()); state_.x=state_.a; set_nz(state_.a); cost=4; break;
    case 0xB7: state_.a=bus_.read(zpy()); state_.x=state_.a; set_nz(state_.a); cost=4; break;
    case 0xBF: address=absy(); state_.a=bus_.read(address); state_.x=state_.a; set_nz(state_.a); cost=5; break;
    case 0x95: bus_.write(zpx(),state_.a); cost=4; break;
    case 0x81: bus_.write(indx(),state_.a); cost=6; break;
    case 0x91: bus_.write(indy(),state_.a); cost=6; break;
    case 0x96: bus_.write(zpy(),state_.x); cost=4; break;
    case 0x8E: bus_.write(fetch16(),state_.x); cost=4; break;
    case 0x94: bus_.write(zpx(),state_.y); cost=4; break;
    case 0x8C: bus_.write(fetch16(),state_.y); cost=4; break;
    case 0xA4: state_.y=bus_.read(zp()); set_nz(state_.y); cost=3; break;
    case 0x7D: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.x); adc(bus_.read(address)); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0x79: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.y); adc(bus_.read(address)); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0x65: adc(bus_.read(zp())); cost=3; break;
    case 0x75: adc(bus_.read(zpx())); cost=4; break;
    case 0x61: adc(bus_.read(indx())); cost=6; break;
    case 0x71: address=indy(); adc(bus_.read(address)); cost=static_cast<std::uint8_t>(5+(indirect_y_crossed?1:0)); break;
    case 0xE5: value=bus_.read(zp()); adc(static_cast<std::uint8_t>(~value)); cost=3; break;
    case 0xF5: value=bus_.read(zpx()); adc(static_cast<std::uint8_t>(~value)); cost=4; break;
    case 0xED: value=bus_.read(fetch16()); adc(static_cast<std::uint8_t>(~value)); cost=4; break;
    case 0xFD: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.x); adc(static_cast<std::uint8_t>(~bus_.read(address))); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0xF9: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.y); adc(static_cast<std::uint8_t>(~bus_.read(address))); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0xE1: adc(static_cast<std::uint8_t>(~bus_.read(indx()))); cost=6; break;
    case 0xF1: address=indy(); adc(static_cast<std::uint8_t>(~bus_.read(address))); cost=static_cast<std::uint8_t>(5+(indirect_y_crossed?1:0)); break;
    case 0x25: state_.a &= bus_.read(zp()); set_nz(state_.a); cost=3; break;
    case 0x35: state_.a &= bus_.read(zpx()); set_nz(state_.a); cost=4; break;
    case 0x2D: state_.a &= bus_.read(fetch16()); set_nz(state_.a); cost=4; break;
    case 0x3D: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.x); state_.a &= bus_.read(address); set_nz(state_.a); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0x39: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.y); state_.a &= bus_.read(address); set_nz(state_.a); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0x21: state_.a &= bus_.read(indx()); set_nz(state_.a); cost=6; break;
    case 0x31: address=indy(); state_.a &= bus_.read(address); set_nz(state_.a); cost=static_cast<std::uint8_t>(5+(indirect_y_crossed?1:0)); break;
    case 0x05: state_.a |= bus_.read(zp()); set_nz(state_.a); cost=3; break;
    case 0x15: state_.a |= bus_.read(zpx()); set_nz(state_.a); cost=4; break;
    case 0x0D: state_.a |= bus_.read(fetch16()); set_nz(state_.a); cost=4; break;
    case 0x1D: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.x); state_.a |= bus_.read(address); set_nz(state_.a); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0x19: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.y); state_.a |= bus_.read(address); set_nz(state_.a); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0x01: state_.a |= bus_.read(indx()); set_nz(state_.a); cost=6; break;
    case 0x11: address=indy(); state_.a |= bus_.read(address); set_nz(state_.a); cost=static_cast<std::uint8_t>(5+(indirect_y_crossed?1:0)); break;
    case 0x45: state_.a ^= bus_.read(zp()); set_nz(state_.a); cost=3; break;
    case 0x55: state_.a ^= bus_.read(zpx()); set_nz(state_.a); cost=4; break;
    case 0x4D: state_.a ^= bus_.read(fetch16()); set_nz(state_.a); cost=4; break;
    case 0x5D: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.x); state_.a ^= bus_.read(address); set_nz(state_.a); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0x59: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.y); state_.a ^= bus_.read(address); set_nz(state_.a); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0x41: state_.a ^= bus_.read(indx()); set_nz(state_.a); cost=6; break;
    case 0x51: address=indy(); state_.a ^= bus_.read(address); set_nz(state_.a); cost=static_cast<std::uint8_t>(5+(indirect_y_crossed?1:0)); break;
    case 0xC5: compare(state_.a,bus_.read(zp())); cost=3; break;
    case 0xD5: compare(state_.a,bus_.read(zpx())); cost=4; break;
    case 0xCD: compare(state_.a,bus_.read(fetch16())); cost=4; break;
    case 0xDD: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.x); compare(state_.a,bus_.read(address)); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0xD9: { const auto base=fetch16(); address=static_cast<std::uint16_t>(base+state_.y); compare(state_.a,bus_.read(address)); cost=static_cast<std::uint8_t>(4+(page_crossed(base,address)?1:0)); break; }
    case 0xC1: compare(state_.a,bus_.read(indx())); cost=6; break;
    case 0xD1: address=indy(); compare(state_.a,bus_.read(address)); cost=static_cast<std::uint8_t>(5+(indirect_y_crossed?1:0)); break;
    case 0xE4: compare(state_.x,bus_.read(zp())); cost=3; break;
    case 0xEC: compare(state_.x,bus_.read(fetch16())); cost=4; break;
    case 0xC4: compare(state_.y,bus_.read(zp())); cost=3; break;
    case 0xCC: compare(state_.y,bus_.read(fetch16())); cost=4; break;
    case 0xEE: address=fetch16(); value=static_cast<std::uint8_t>(bus_.read(address)+1); bus_.write(address,value); set_nz(value); cost=6; break;
    case 0xF6: address=zpx(); value=static_cast<std::uint8_t>(bus_.read(address)+1); bus_.write(address,value); set_nz(value); cost=6; break;
    case 0xFE: address=absx(); value=static_cast<std::uint8_t>(bus_.read(address)+1); bus_.write(address,value); set_nz(value); cost=7; break;
    case 0xCE: address=fetch16(); value=static_cast<std::uint8_t>(bus_.read(address)-1); bus_.write(address,value); set_nz(value); cost=6; break;
    case 0xD6: address=zpx(); value=static_cast<std::uint8_t>(bus_.read(address)-1); bus_.write(address,value); set_nz(value); cost=6; break;
    case 0xDE: address=absx(); value=static_cast<std::uint8_t>(bus_.read(address)-1); bus_.write(address,value); set_nz(value); cost=7; break;
    case 0x0A: set_flag(C,(state_.a&0x80)!=0); state_.a=static_cast<std::uint8_t>(state_.a<<1); set_nz(state_.a); cost=2; break;
    case 0x06: address=zp(); value=bus_.read(address); set_flag(C,(value&0x80)!=0); value=static_cast<std::uint8_t>(value<<1); bus_.write(address,value); set_nz(value); cost=5; break;
    case 0x16: address=zpx(); value=bus_.read(address); set_flag(C,(value&0x80)!=0); value=static_cast<std::uint8_t>(value<<1); bus_.write(address,value); set_nz(value); cost=6; break;
    case 0x0E: address=fetch16(); value=bus_.read(address); set_flag(C,(value&0x80)!=0); value=static_cast<std::uint8_t>(value<<1); bus_.write(address,value); set_nz(value); cost=6; break;
    case 0x1E: address=absx(); value=bus_.read(address); set_flag(C,(value&0x80)!=0); value=static_cast<std::uint8_t>(value<<1); bus_.write(address,value); set_nz(value); cost=7; break;
    case 0x4A: set_flag(C,(state_.a&1)!=0); state_.a=static_cast<std::uint8_t>(state_.a>>1); set_nz(state_.a); cost=2; break;
    case 0x46: address=zp(); value=bus_.read(address); set_flag(C,(value&1)!=0); value=static_cast<std::uint8_t>(value>>1); bus_.write(address,value); set_nz(value); cost=5; break;
    case 0x56: address=zpx(); value=bus_.read(address); set_flag(C,(value&1)!=0); value=static_cast<std::uint8_t>(value>>1); bus_.write(address,value); set_nz(value); cost=6; break;
    case 0x4E: address=fetch16(); value=bus_.read(address); set_flag(C,(value&1)!=0); value=static_cast<std::uint8_t>(value>>1); bus_.write(address,value); set_nz(value); cost=6; break;
    case 0x5E: address=absx(); value=bus_.read(address); set_flag(C,(value&1)!=0); value=static_cast<std::uint8_t>(value>>1); bus_.write(address,value); set_nz(value); cost=7; break;
    case 0x2A: { const bool c=flag(C); set_flag(C,(state_.a&0x80)!=0); state_.a=static_cast<std::uint8_t>((state_.a<<1)|(c?1:0)); set_nz(state_.a); cost=2; break; }
    case 0x26: address=zp(); value=bus_.read(address); { const bool c=flag(C); set_flag(C,(value&0x80)!=0); value=static_cast<std::uint8_t>((value<<1)|(c?1:0)); } bus_.write(address,value); set_nz(value); cost=5; break;
    case 0x6A: { const bool c=flag(C); set_flag(C,(state_.a&1)!=0); state_.a=static_cast<std::uint8_t>((state_.a>>1)|(c?0x80:0)); set_nz(state_.a); cost=2; break; }
    case 0x66: address=zp(); value=bus_.read(address); { const bool c=flag(C); set_flag(C,(value&1)!=0); value=static_cast<std::uint8_t>((value>>1)|(c?0x80:0)); } bus_.write(address,value); set_nz(value); cost=5; break;
    // Remaining official 2A03 read/modify/write opcodes.
    case 0x36: address=zpx(); value=bus_.read(address); { const bool c=flag(C); set_flag(C,(value&0x80)!=0); value=static_cast<std::uint8_t>((value<<1)|(c?1:0)); } bus_.write(address,value); set_nz(value); cost=6; break;
    case 0x2E: address=fetch16(); value=bus_.read(address); { const bool c=flag(C); set_flag(C,(value&0x80)!=0); value=static_cast<std::uint8_t>((value<<1)|(c?1:0)); } bus_.write(address,value); set_nz(value); cost=6; break;
    case 0x3E: address=absx(); value=bus_.read(address); { const bool c=flag(C); set_flag(C,(value&0x80)!=0); value=static_cast<std::uint8_t>((value<<1)|(c?1:0)); } bus_.write(address,value); set_nz(value); cost=7; break;
    case 0x76: address=zpx(); value=bus_.read(address); { const bool c=flag(C); set_flag(C,(value&1)!=0); value=static_cast<std::uint8_t>((value>>1)|(c?0x80:0)); } bus_.write(address,value); set_nz(value); cost=6; break;
    case 0x6E: address=fetch16(); value=bus_.read(address); { const bool c=flag(C); set_flag(C,(value&1)!=0); value=static_cast<std::uint8_t>((value>>1)|(c?0x80:0)); } bus_.write(address,value); set_nz(value); cost=6; break;
    case 0x7E: address=absx(); value=bus_.read(address); { const bool c=flag(C); set_flag(C,(value&1)!=0); value=static_cast<std::uint8_t>((value>>1)|(c?0x80:0)); } bus_.write(address,value); set_nz(value); cost=7; break;
    default: state_.pc=start; return false;
    }
    state_.status |= U;
    state_.cycles += cost;
    bus_.tick_cpu_cycles(cost);
    return true;
}

} // namespace nesrev
