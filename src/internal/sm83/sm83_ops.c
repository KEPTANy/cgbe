#include "internal/sm83/sm83.h"

#include <assert.h>
#include <stdlib.h>

enum r8 { r8_b, r8_c, r8_d, r8_e, r8_h, r8_l, r8_hl, r8_a };
enum r16 { r16_bc, r16_de, r16_hl, r16_sp };
enum r16stk { r16stk_bc, r16stk_de, r16stk_hl, r16stk_af };
enum r16mem { r16mem_bc, r16mem_de, r16mem_hli, r16mem_hld };
enum cond { cond_nz, cond_z, cond_nc, cond_c };

static void prefetch(struct sm83 *cpu) {
    cpu->opcode = bus_read(cpu->bus, cpu->regs.pc++);
    cpu->m_cycle = 0;
}

// NOP
// Opcode: 0b00000000
// M-cycles: 1
// Flags: ----
static void nop(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);
    prefetch(cpu);
}

// LD r16, imm16
// Opcode: 0b00xx0001
// M-cycles: 3
// Flags: ----
static void ld_r16_imm16(struct sm83 *cpu, enum r16 dest) {
    assert(cpu->m_cycle < 3);

    switch (cpu->m_cycle++) {
    case 0: cpu->tmp = bus_read(cpu->bus, cpu->regs.pc++); break;
    case 1: cpu->tmp += bus_read(cpu->bus, cpu->regs.pc++) * 256; break;
    case 2:
        switch (dest) {
        case r16_bc: cpu->regs.b = cpu->tmp; break;
        case r16_de: cpu->regs.d = cpu->tmp; break;
        case r16_hl: cpu->regs.h = cpu->tmp; break;
        case r16_sp: cpu->regs.sp = cpu->tmp; break;
        }
        prefetch(cpu);
        break;
    }
}

// LD [r16mem], a
// Opcode: 0b00xx0010
// M-cycles: 2
// Flags: ----
static void ld_r16mem_a(struct sm83 *cpu, enum r16mem dest) {
    assert(cpu->m_cycle < 2);

    switch (cpu->m_cycle++) {
    case 0:
        uint16_t address;
        switch (dest) {
        case r16mem_bc: address = cpu->regs.bc; break;
        case r16mem_de: address = cpu->regs.de; break;
        case r16mem_hli: address = cpu->regs.hl++; break;
        case r16mem_hld: address = cpu->regs.hl--; break;
        }
        bus_write(cpu->bus, address, cpu->regs.a);
        break;
    case 1: prefetch(cpu); break;
    }
}

// LD a, [r16mem]
// Opcode: 0b00xx1010
// M-cycles: 2
// Flags: ----
static void ld_a_r16mem(struct sm83 *cpu, enum r16mem source) {
    assert(cpu->m_cycle < 2);

    switch (cpu->m_cycle++) {
    case 0:
        uint16_t address;
        switch (source) {
        case r16mem_bc: address = cpu->regs.bc; break;
        case r16mem_de: address = cpu->regs.de; break;
        case r16mem_hli: address = cpu->regs.hl++; break;
        case r16mem_hld: address = cpu->regs.hl--; break;
        }
        cpu->regs.a = bus_read(cpu->bus, address);
        break;
    case 1: prefetch(cpu); break;
    }
}

// LD [imm16], sp
// Opcode: 0b00001000
// M-cycles: 5
// Flags: ----
static void ld_imm16_sp(struct sm83 *cpu) {
    assert(cpu->m_cycle < 5);

    switch (cpu->m_cycle++) {
    case 0: cpu->tmp = bus_read(cpu->bus, cpu->regs.pc++); break;
    case 1: cpu->tmp += bus_read(cpu->bus, cpu->regs.pc++) * 256; break;
    case 2: bus_write(cpu->bus, cpu->tmp++, cpu->regs.pc & 0xFF); break;
    case 3: bus_write(cpu->bus, cpu->tmp, cpu->regs.pc / 256); break;
    case 4: prefetch(cpu); break;
    }
}

// INC r16
// Opcode: 0b00xx0011
// M-cycles: 2
// Flags: ----
static void inc_r16(struct sm83 *cpu, enum r16 reg) {
    assert(cpu->m_cycle < 2);

    switch (cpu->m_cycle++) {
    // case 0:
    case 1:
        switch (reg) {
        case r16_bc: cpu->regs.bc++; break;
        case r16_de: cpu->regs.de++; break;
        case r16_hl: cpu->regs.hl++; break;
        case r16_sp: cpu->regs.sp++; break;
        }
        prefetch(cpu);
        break;
    }
}

// DEC r16
// Opcode: 0b00xx1011
// M-cycles: 2
// Flags: ----
static void dec_r16(struct sm83 *cpu, enum r16 reg) {
    assert(cpu->m_cycle < 2);

    switch (cpu->m_cycle++) {
    // case 0:
    case 1:
        switch (reg) {
        case r16_bc: cpu->regs.bc--; break;
        case r16_de: cpu->regs.de--; break;
        case r16_hl: cpu->regs.hl--; break;
        case r16_sp: cpu->regs.sp--; break;
        }
        prefetch(cpu);
        break;
    }
}

// ADD hl, r16
// Opcode: 0b00xx1001
// M-cycles: 2
// Flags: -0HC
static void add_hl_r16(struct sm83 *cpu, enum r16 reg) {
    assert(cpu->m_cycle < 2);

    uint16_t x;
    uint16_t y;
    switch (cpu->m_cycle++) {
    // case 0:
    case 1:
        cpu->regs.f &= ~(SM83_N_MASK & SM83_H_MASK & SM83_C_MASK); // N = 0, H = 0, C = 0

        x = cpu->regs.hl;
        switch (reg) {
        case r16_bc: y = cpu->regs.bc; break;
        case r16_de: y = cpu->regs.de; break;
        case r16_hl: y = cpu->regs.hl; break;
        case r16_sp: y = cpu->regs.sp; break;
        }

        // check for overflow from bit 11
        if ((x & 0xFFF) + (y & 0xFFF) > 0xFFF) {
            cpu->regs.f |= SM83_H_MASK;
        }

        // check for overflow from bit 15
        if ((uint32_t)x + y > 0xFFFF) {
            cpu->regs.f |= SM83_C_MASK;
        }

        cpu->regs.hl = x + y;

        prefetch(cpu);
        break;
    }
}

// INC r8
// Opcode: 0b00xxx100
// M-cycles: 1/3 (3 for [hl])
// Flags: Z0H-
static void inc_r8(struct sm83 *cpu, enum r8 reg) {
    assert(cpu->m_cycle < 1 || (reg == r8_hl && cpu->m_cycle < 3));

    if (reg == r8_hl) {
        switch (cpu->m_cycle++) {
        case 0: cpu->tmp = bus_read(cpu->bus, cpu->regs.hl) + 1; break;
        case 1:
            cpu->regs.f &= ~(SM83_N_MASK & SM83_H_MASK & SM83_Z_MASK); // N = 0, H = 0, Z = 0

            bus_write(cpu->bus, cpu->regs.hl, (uint8_t)cpu->tmp);

            if ((uint8_t)cpu->tmp == 0) {
                cpu->regs.f |= SM83_Z_MASK;
            }

            // check for overflow from bit 3
            if ((cpu->tmp ^ (cpu->tmp - 1)) & (1 << 4)) {
                cpu->regs.f |= SM83_H_MASK;
            }

            break;
        case 2: prefetch(cpu); break;
        }
    } else {
        cpu->regs.f &= ~(SM83_N_MASK & SM83_H_MASK & SM83_Z_MASK); // N = 0, H = 0, Z = 0

        uint8_t res;
        switch (reg) {
        case r8_a: res = cpu->regs.a++; break;
        case r8_b: res = cpu->regs.b++; break;
        case r8_c: res = cpu->regs.c++; break;
        case r8_d: res = cpu->regs.d++; break;
        case r8_e: res = cpu->regs.e++; break;
        case r8_h: res = cpu->regs.h++; break;
        case r8_l: res = cpu->regs.l++; break;
        default: unreachable();
        }

        if (res == 0) {
            cpu->regs.f |= SM83_Z_MASK;
        }

        // check for overflow from bit 3
        if ((res ^ (res - 1)) & (1 << 4)) {
            cpu->regs.f |= SM83_H_MASK;
        }

        prefetch(cpu);
    }
}

// DEC r8
// Opcode: 0b00xxx101
// M-cycles: 1/3 (3 for [hl])
// Flags: Z0H-
static void dec_r8(struct sm83 *cpu, enum r8 reg) {
    assert(cpu->m_cycle < 1 || (reg == r8_hl && cpu->m_cycle < 3));

    if (reg == r8_hl) {
        switch (cpu->m_cycle++) {
        case 0: cpu->tmp = bus_read(cpu->bus, cpu->regs.hl) - 1; break;
        case 1:
            cpu->regs.f &= ~(SM83_H_MASK & SM83_Z_MASK); // H = 0, Z = 0
            cpu->regs.f |= SM83_N_MASK;

            bus_write(cpu->bus, cpu->regs.hl, (uint8_t)cpu->tmp);

            if ((uint8_t)cpu->tmp == 0) {
                cpu->regs.f |= SM83_Z_MASK;
            }

            // check for borrow from bit 4
            if ((cpu->tmp ^ (cpu->tmp + 1)) & (1 << 4)) {
                cpu->regs.f |= SM83_H_MASK;
            }

            break;
        case 2: prefetch(cpu); break;
        }
    } else {
        cpu->regs.f &= ~(SM83_H_MASK & SM83_Z_MASK); // H = 0, Z = 0
        cpu->regs.f |= SM83_N_MASK;

        uint8_t res;
        switch (reg) {
        case r8_a: res = cpu->regs.a--; break;
        case r8_b: res = cpu->regs.b--; break;
        case r8_c: res = cpu->regs.c--; break;
        case r8_d: res = cpu->regs.d--; break;
        case r8_e: res = cpu->regs.e--; break;
        case r8_h: res = cpu->regs.h--; break;
        case r8_l: res = cpu->regs.l--; break;
        default: unreachable();
        }

        if (res == 0) {
            cpu->regs.f |= SM83_Z_MASK;
        }

        // check for borrow from bit 4
        if ((res ^ (res + 1)) & (1 << 4)) {
            cpu->regs.f |= SM83_H_MASK;
        }

        prefetch(cpu);
    }
}

// LD r8, imm8
// Opcode: 0b00xxx110
// M-cycles: 2/3 (3 for [hl])
// Flags: ----
static void ld_r8_imm8(struct sm83 *cpu, enum r8 reg) {
    assert(cpu->m_cycle < 2 || (reg == r8_hl && cpu->m_cycle < 3));

    switch (cpu->m_cycle) {
    case 0: cpu->tmp = bus_read(cpu->bus, cpu->regs.pc++); break;
    case 1:
        switch (reg) {
        case r8_a: cpu->regs.a = (uint8_t)cpu->tmp; break;
        case r8_b: cpu->regs.b = (uint8_t)cpu->tmp; break;
        case r8_c: cpu->regs.c = (uint8_t)cpu->tmp; break;
        case r8_d: cpu->regs.d = (uint8_t)cpu->tmp; break;
        case r8_e: cpu->regs.e = (uint8_t)cpu->tmp; break;
        case r8_h: cpu->regs.h = (uint8_t)cpu->tmp; break;
        case r8_l: cpu->regs.l = (uint8_t)cpu->tmp; break;
        case r8_hl:
            bus_write(cpu->bus, cpu->regs.hl, (uint8_t)cpu->tmp);
            return; // this return delays prefetch for 1 m-cycle
        }
        [[fallthrough]];
    case 2: prefetch(cpu); break; // executed on the 2nd m-cycle for all regs other than [hl]
    }
}

// RLCA
// Opcode: 0b00000111
// M-cycles: 1
// Flags: 000C
static void rlca(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    cpu->regs.f = (cpu->regs.a & (1 << 7)) ? SM83_C_MASK : 0;
    cpu->regs.a = (cpu->regs.a << 1) | (cpu->regs.f ? 1 : 0);
    prefetch(cpu);
}

// RRCA
// Opcode: 0b00001111
// M-cycles: 1
// Flags: 000C
static void rrca(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    cpu->regs.f = (cpu->regs.a & 1) ? SM83_C_MASK : 0;
    cpu->regs.a = (cpu->regs.a >> 1) | (cpu->regs.f ? (1 << 7) : 0);
    prefetch(cpu);
}

// RLA
// Opcode: 0b00010111
// M-cycles: 1
// Flags: 000C
static void rla(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    bool c = cpu->regs.f & SM83_C_MASK;
    cpu->regs.f = (cpu->regs.a & (1 << 7)) ? SM83_C_MASK : 0;
    cpu->regs.a = (cpu->regs.a << 1) | (c ? 1 : 0);
    prefetch(cpu);
}

// RRA
// Opcode: 0b00011111
// M-cycles: 1
// Flags: 000C
static void rra(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    bool c = cpu->regs.f & SM83_C_MASK;
    cpu->regs.f = (cpu->regs.a & (1 << 7)) ? SM83_C_MASK : 0;
    cpu->regs.a = (cpu->regs.a << 1) | (c ? (1 << 7) : 0);
    prefetch(cpu);
}

// DAA
// Opcode: 0b00100111
// M-cycles: 1
// Flags: Z-0C
static void daa(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    // Based on code from here: https://ehaskins.com/2018-01-30%20Z80%20DAA/
    // And notes from here: https://rgbds.gbdev.io/docs/v0.9.1/gbz80.7#DAA

    bool H_flag = cpu->regs.f & SM83_H_MASK;
    bool N_flag = cpu->regs.f & SM83_N_MASK;
    bool C_flag = cpu->regs.f & SM83_C_MASK;
    uint8_t val = cpu->regs.a;

    uint8_t flags = 0;
    uint8_t adjustment = 0;
    if (H_flag || (!N_flag && (val & 0xF) > 0x9)) {
        adjustment |= 0x06;
    }

    if (C_flag || (!N_flag && val > 0x99)) {
        adjustment |= 0x60;
        flags |= SM83_C_MASK;
    }

    val += N_flag ? -adjustment : adjustment;

    if (val == 0) {
        flags |= SM83_Z_MASK;
    }

    cpu->regs.a = val;
    cpu->regs.f &= ~(SM83_Z_MASK | SM83_H_MASK | SM83_C_MASK);
    cpu->regs.f |= flags;

    prefetch(cpu);
}

// CPL
// Opcode: 0b00101111
// M-cycles: 1
// Flags: -11-
static void cpl(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    cpu->regs.a = ~cpu->regs.a;
    cpu->regs.f |= SM83_N_MASK | SM83_H_MASK;
    prefetch(cpu);
}

// SCF
// Opcode: 0b00110111
// M-cycles: 1
// Flags: -001
static void scf(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    cpu->regs.f &= ~(SM83_N_MASK | SM83_H_MASK);
    cpu->regs.f |= SM83_C_MASK;
    prefetch(cpu);
}

// CCF
// Opcode: 0b00111111
// M-cycles: 1
// Flags: -00C
static void ccf(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    cpu->regs.f &= ~(SM83_N_MASK | SM83_H_MASK);
    cpu->regs.f ^= SM83_C_MASK;
    prefetch(cpu);
}

// JR imm8
// Opcode: 0x00011000
// M-cycles: 3
// Flags: ----
static void jr_imm8(struct sm83 *cpu) {
    assert(cpu->m_cycle < 3);

    switch (cpu->m_cycle++) {
    case 0: cpu->regs.pc += (int8_t)bus_read(cpu->bus, cpu->regs.pc++); break;
    // case 1:
    case 2: prefetch(cpu); break;
    }
}

// JR cond, imm8
// Opcode: 0x001xx000
// M-cycles: 2/3 (2 if cond is true, 3 if cond is false)
// Flags: ----
static void jr_cond_imm8(struct sm83 *cpu, enum cond cc) {
    bool z = cpu->regs.f & SM83_Z_MASK;
    bool c = cpu->regs.f & SM83_C_MASK;

    bool cond = (cc == cond_z && z) || (cc == cond_nz && !z) || (cc == cond_c && c) ||
                (cc == cond_nc && !c);

    assert(cpu->m_cycle < 2 || (cond && cpu->m_cycle < 3));

    if (cond) {
        switch (cpu->m_cycle++) {
        case 0: cpu->regs.pc += (int8_t)bus_read(cpu->bus, cpu->regs.pc++); break;
        // case 1:
        case 2: prefetch(cpu); break;
        }
    } else {
        switch (cpu->m_cycle++) {
        case 0: bus_read(cpu->bus, cpu->regs.pc++); break;
        case 1: prefetch(cpu); break;
        }
    }
}

// LD r8, r8
// Opcode: 0x01dddsss (d for dest, s for source)
// M-cycles: 1/2 (2 if either dest or source is [hl])
// Flags: ----
static void ld_r8_r8(struct sm83 *cpu, enum r8 dest, enum r8 source) {
    assert(cpu->m_cycle < 1 || ((dest == r8_hl || source == r8_hl) && cpu->m_cycle < 2));

    bool one_more = false;
    switch (cpu->m_cycle++) {
    case 0:
        switch (source) {
        case r8_a: cpu->tmp = cpu->regs.a; break;
        case r8_b: cpu->tmp = cpu->regs.b; break;
        case r8_c: cpu->tmp = cpu->regs.c; break;
        case r8_d: cpu->tmp = cpu->regs.d; break;
        case r8_e: cpu->tmp = cpu->regs.e; break;
        case r8_h: cpu->tmp = cpu->regs.h; break;
        case r8_l: cpu->tmp = cpu->regs.l; break;
        case r8_hl:
            cpu->tmp = bus_read(cpu->bus, cpu->regs.hl);
            one_more = true;
        }

        switch (dest) {
        case r8_a: cpu->regs.a = cpu->tmp; break;
        case r8_b: cpu->regs.b = cpu->tmp; break;
        case r8_c: cpu->regs.c = cpu->tmp; break;
        case r8_d: cpu->regs.d = cpu->tmp; break;
        case r8_e: cpu->regs.e = cpu->tmp; break;
        case r8_h: cpu->regs.h = cpu->tmp; break;
        case r8_l: cpu->regs.l = cpu->tmp; break;
        case r8_hl:
            bus_write(cpu->bus, cpu->regs.hl, cpu->tmp);
            one_more = true;
        }

        if (one_more) {
            return;
        }

        [[fallthrough]];
    case 1: prefetch(cpu); break;
    }
}

// ADD a, r8
// Opcode: 0x10000xxx
// M-cycles: 1/2 (2 for [hl])
// Flags: Z0HC
static void add_a_r8(struct sm83 *cpu, enum r8 reg) {
    assert(cpu->m_cycle < 1 || (reg == r8_hl && cpu->m_cycle < 2));

    bool one_more = false;
    switch (cpu->m_cycle++) {
    case 0:
        uint16_t val;
        switch (reg) {
        case r8_a: val = cpu->regs.a; break;
        case r8_b: val = cpu->regs.b; break;
        case r8_c: val = cpu->regs.c; break;
        case r8_d: val = cpu->regs.d; break;
        case r8_e: val = cpu->regs.e; break;
        case r8_h: val = cpu->regs.h; break;
        case r8_l: val = cpu->regs.l; break;
        case r8_hl:
            val = bus_read(cpu->bus, cpu->regs.hl);
            one_more = true;
        }

        cpu->regs.f = 0;

        // check for overflow from bit 3
        if ((cpu->regs.a & 0xF) + (val & 0xF) > 0xF) {
            cpu->regs.f |= SM83_H_MASK;
        }

        // check for overflow from bit 7
        if (cpu->regs.a + val > 0xFF) {
            cpu->regs.f |= SM83_C_MASK;
        }

        cpu->regs.a += val;

        if (cpu->regs.a == 0) {
            cpu->regs.f |= SM83_Z_MASK;
        }

        if (one_more) {
            return;
        }

        [[fallthrough]];
    case 1: prefetch(cpu); break;
    }
}

// ADC a, r8
// Opcode: 0x10001xxx
// M-cycles: 1/2 (2 for [hl])
// Flags: Z0HC
static void adc_a_r8(struct sm83 *cpu, enum r8 reg) {
    assert(cpu->m_cycle < 1 || (reg == r8_hl && cpu->m_cycle < 2));

    bool one_more = false;
    switch (cpu->m_cycle++) {
    case 0:
        uint16_t val;
        switch (reg) {
        case r8_a: val = cpu->regs.a; break;
        case r8_b: val = cpu->regs.b; break;
        case r8_c: val = cpu->regs.c; break;
        case r8_d: val = cpu->regs.d; break;
        case r8_e: val = cpu->regs.e; break;
        case r8_h: val = cpu->regs.h; break;
        case r8_l: val = cpu->regs.l; break;
        case r8_hl:
            val = bus_read(cpu->bus, cpu->regs.hl);
            one_more = true;
        }

        if (cpu->regs.f & SM83_C_MASK) {
            val++;
        }

        cpu->regs.f = 0;

        // check for overflow from bit 3
        if ((cpu->regs.a & 0xF) + (val & 0xF) > 0xF) {
            cpu->regs.f |= SM83_H_MASK;
        }

        // check for overflow from bit 7
        if (cpu->regs.a + val > 0xFF) {
            cpu->regs.f |= SM83_C_MASK;
        }

        cpu->regs.a += val;

        if (cpu->regs.a == 0) {
            cpu->regs.f |= SM83_Z_MASK;
        }

        if (one_more) {
            return;
        }

        [[fallthrough]];
    case 1: prefetch(cpu); break;
    }
}

void sm83_m_cycle(struct sm83 *cpu) {
    switch (cpu->opcode) {
    case 0x00: nop(cpu); break; // NOP

    case 0x01: ld_r16_imm16(cpu, r16_bc); break; // LD bc, imm16
    case 0x11: ld_r16_imm16(cpu, r16_de); break; // LD de, imm16
    case 0x21: ld_r16_imm16(cpu, r16_hl); break; // LD hl, imm16
    case 0x31: ld_r16_imm16(cpu, r16_sp); break; // LD sp, imm16

    case 0x02: ld_r16mem_a(cpu, r16mem_bc); break;  // LD [bc], a
    case 0x12: ld_r16mem_a(cpu, r16mem_de); break;  // LD [de], a
    case 0x22: ld_r16mem_a(cpu, r16mem_hli); break; // LD [hl+], a
    case 0x32: ld_r16mem_a(cpu, r16mem_hld); break; // LD [hl-], a

    case 0x0A: ld_a_r16mem(cpu, r16mem_bc); break;  // LD a, [bc]
    case 0x1A: ld_a_r16mem(cpu, r16mem_de); break;  // LD a, [de]
    case 0xA2: ld_a_r16mem(cpu, r16mem_hli); break; // LD a, [hl+]
    case 0x3A: ld_a_r16mem(cpu, r16mem_hld); break; // LD a, [hl-]

    case 0x08: ld_imm16_sp(cpu); break; // LD [imm16], sp

    case 0x03: inc_r16(cpu, r16_bc); break; // INC bc
    case 0x13: inc_r16(cpu, r16_de); break; // INC de
    case 0x23: inc_r16(cpu, r16_hl); break; // INC hl
    case 0x33: inc_r16(cpu, r16_sp); break; // INC sp

    case 0x0B: dec_r16(cpu, r16_bc); break; // DEC bc
    case 0x1B: dec_r16(cpu, r16_de); break; // DEC de
    case 0x2B: dec_r16(cpu, r16_hl); break; // DEC hl
    case 0x3B: dec_r16(cpu, r16_sp); break; // DEC sp

    case 0x09: add_hl_r16(cpu, r16_bc); break; // ADD hl, bc
    case 0x19: add_hl_r16(cpu, r16_de); break; // ADD hl, de
    case 0x29: add_hl_r16(cpu, r16_hl); break; // ADD hl, hl
    case 0x39: add_hl_r16(cpu, r16_sp); break; // ADD hl, sp

    case 0x04: inc_r8(cpu, r8_b); break;  // INC b
    case 0x0C: inc_r8(cpu, r8_c); break;  // INC c
    case 0x14: inc_r8(cpu, r8_d); break;  // INC d
    case 0x1C: inc_r8(cpu, r8_e); break;  // INC e
    case 0x24: inc_r8(cpu, r8_h); break;  // INC h
    case 0x2C: inc_r8(cpu, r8_l); break;  // INC l
    case 0x34: inc_r8(cpu, r8_hl); break; // INC [hl]
    case 0x3C: inc_r8(cpu, r8_a); break;  // INC a

    case 0x05: dec_r8(cpu, r8_b); break;  // DEC b
    case 0x0D: dec_r8(cpu, r8_c); break;  // DEC c
    case 0x15: dec_r8(cpu, r8_d); break;  // DEC d
    case 0x1D: dec_r8(cpu, r8_e); break;  // DEC e
    case 0x25: dec_r8(cpu, r8_h); break;  // DEC h
    case 0x2D: dec_r8(cpu, r8_l); break;  // DEC l
    case 0x35: dec_r8(cpu, r8_hl); break; // DEC [hl]
    case 0x3D: dec_r8(cpu, r8_a); break;  // DEC a

    case 0x06: ld_r8_imm8(cpu, r8_b); break;  // LD b, imm8
    case 0x0E: ld_r8_imm8(cpu, r8_c); break;  // LD c, imm8
    case 0x16: ld_r8_imm8(cpu, r8_d); break;  // LD d, imm8
    case 0x1E: ld_r8_imm8(cpu, r8_e); break;  // LD e, imm8
    case 0x26: ld_r8_imm8(cpu, r8_h); break;  // LD h, imm8
    case 0x2E: ld_r8_imm8(cpu, r8_l); break;  // LD l, imm8
    case 0x36: ld_r8_imm8(cpu, r8_hl); break; // LD [hl], imm8
    case 0x3E: ld_r8_imm8(cpu, r8_a); break;  // LD a, imm8

    case 0x07: rlca(cpu); break; // RLCA
    case 0x0F: rrca(cpu); break; // RRCA
    case 0x17: rla(cpu); break;  // RLA
    case 0x1F: rra(cpu); break;  // RRA
    case 0x27: daa(cpu); break;  // DAA
    case 0x2F: cpl(cpu); break;  // CPL
    case 0x37: scf(cpu); break;  // SCF
    case 0x3F: ccf(cpu); break;  // CCF

    case 0x18: jr_imm8(cpu); break;               // JR imm8
    case 0x20: jr_cond_imm8(cpu, cond_nz); break; // JR nz, imm8
    case 0x28: jr_cond_imm8(cpu, cond_z); break;  // JR z, imm8
    case 0x30: jr_cond_imm8(cpu, cond_nc); break; // JR nc, imm8
    case 0x38: jr_cond_imm8(cpu, cond_c); break;  // JR c, imm8

    case 0x40: ld_r8_r8(cpu, r8_b, r8_b); break;  // LD b, b
    case 0x41: ld_r8_r8(cpu, r8_b, r8_c); break;  // LD b, c
    case 0x42: ld_r8_r8(cpu, r8_b, r8_d); break;  // LD b, d
    case 0x43: ld_r8_r8(cpu, r8_b, r8_e); break;  // LD b, e
    case 0x44: ld_r8_r8(cpu, r8_b, r8_h); break;  // LD b, h
    case 0x45: ld_r8_r8(cpu, r8_b, r8_l); break;  // LD b, l
    case 0x46: ld_r8_r8(cpu, r8_b, r8_hl); break; // LD b, [hl]
    case 0x47: ld_r8_r8(cpu, r8_b, r8_a); break;  // LD b, a

    case 0x48: ld_r8_r8(cpu, r8_c, r8_b); break;  // LD c, b
    case 0x49: ld_r8_r8(cpu, r8_c, r8_c); break;  // LD c, c
    case 0x4A: ld_r8_r8(cpu, r8_c, r8_d); break;  // LD c, d
    case 0x4B: ld_r8_r8(cpu, r8_c, r8_e); break;  // LD c, e
    case 0x4C: ld_r8_r8(cpu, r8_c, r8_h); break;  // LD c, h
    case 0x4D: ld_r8_r8(cpu, r8_c, r8_l); break;  // LD c, l
    case 0x4E: ld_r8_r8(cpu, r8_c, r8_hl); break; // LD c, [hl]
    case 0x4F: ld_r8_r8(cpu, r8_c, r8_a); break;  // LD c, a

    case 0x50: ld_r8_r8(cpu, r8_d, r8_b); break;  // LD d, b
    case 0x51: ld_r8_r8(cpu, r8_d, r8_c); break;  // LD d, c
    case 0x52: ld_r8_r8(cpu, r8_d, r8_d); break;  // LD d, d
    case 0x53: ld_r8_r8(cpu, r8_d, r8_e); break;  // LD d, e
    case 0x54: ld_r8_r8(cpu, r8_d, r8_h); break;  // LD d, h
    case 0x55: ld_r8_r8(cpu, r8_d, r8_l); break;  // LD d, l
    case 0x56: ld_r8_r8(cpu, r8_d, r8_hl); break; // LD d, [hl]
    case 0x57: ld_r8_r8(cpu, r8_d, r8_a); break;  // LD d, a

    case 0x58: ld_r8_r8(cpu, r8_e, r8_b); break;  // LD e, b
    case 0x59: ld_r8_r8(cpu, r8_e, r8_c); break;  // LD e, c
    case 0x5A: ld_r8_r8(cpu, r8_e, r8_d); break;  // LD e, d
    case 0x5B: ld_r8_r8(cpu, r8_e, r8_e); break;  // LD e, e
    case 0x5C: ld_r8_r8(cpu, r8_e, r8_h); break;  // LD e, h
    case 0x5D: ld_r8_r8(cpu, r8_e, r8_l); break;  // LD e, l
    case 0x5E: ld_r8_r8(cpu, r8_e, r8_hl); break; // LD e, [hl]
    case 0x5F: ld_r8_r8(cpu, r8_e, r8_a); break;  // LD e, a

    case 0x60: ld_r8_r8(cpu, r8_h, r8_b); break;  // LD h, b
    case 0x61: ld_r8_r8(cpu, r8_h, r8_c); break;  // LD h, c
    case 0x62: ld_r8_r8(cpu, r8_h, r8_d); break;  // LD h, d
    case 0x63: ld_r8_r8(cpu, r8_h, r8_e); break;  // LD h, e
    case 0x64: ld_r8_r8(cpu, r8_h, r8_h); break;  // LD h, h
    case 0x65: ld_r8_r8(cpu, r8_h, r8_l); break;  // LD h, l
    case 0x66: ld_r8_r8(cpu, r8_h, r8_hl); break; // LD h, [hl]
    case 0x67: ld_r8_r8(cpu, r8_h, r8_a); break;  // LD h, a

    case 0x68: ld_r8_r8(cpu, r8_l, r8_b); break;  // LD l, b
    case 0x69: ld_r8_r8(cpu, r8_l, r8_c); break;  // LD l, c
    case 0x6A: ld_r8_r8(cpu, r8_l, r8_d); break;  // LD l, d
    case 0x6B: ld_r8_r8(cpu, r8_l, r8_e); break;  // LD l, e
    case 0x6C: ld_r8_r8(cpu, r8_l, r8_h); break;  // LD l, h
    case 0x6D: ld_r8_r8(cpu, r8_l, r8_l); break;  // LD l, l
    case 0x6E: ld_r8_r8(cpu, r8_l, r8_hl); break; // LD l, [hl]
    case 0x6F: ld_r8_r8(cpu, r8_l, r8_a); break;  // LD l, a

    case 0x70: ld_r8_r8(cpu, r8_hl, r8_b); break; // LD [hl], b
    case 0x71: ld_r8_r8(cpu, r8_hl, r8_c); break; // LD [hl], c
    case 0x72: ld_r8_r8(cpu, r8_hl, r8_d); break; // LD [hl], d
    case 0x73: ld_r8_r8(cpu, r8_hl, r8_e); break; // LD [hl], e
    case 0x74: ld_r8_r8(cpu, r8_hl, r8_h); break; // LD [hl], h
    case 0x75: ld_r8_r8(cpu, r8_hl, r8_l); break; // LD [hl], l

    case 0x77: ld_r8_r8(cpu, r8_hl, r8_a); break; // LD [hl], a

    case 0x78: ld_r8_r8(cpu, r8_a, r8_b); break;  // LD a, b
    case 0x79: ld_r8_r8(cpu, r8_a, r8_c); break;  // LD a, c
    case 0x7A: ld_r8_r8(cpu, r8_a, r8_d); break;  // LD a, d
    case 0x7B: ld_r8_r8(cpu, r8_a, r8_e); break;  // LD a, e
    case 0x7C: ld_r8_r8(cpu, r8_a, r8_h); break;  // LD a, h
    case 0x7D: ld_r8_r8(cpu, r8_a, r8_l); break;  // LD a, l
    case 0x7E: ld_r8_r8(cpu, r8_a, r8_hl); break; // LD a, [hl]
    case 0x7F: ld_r8_r8(cpu, r8_a, r8_a); break;  // LD a, a

    case 0x80: add_a_r8(cpu, r8_b); break;  // ADD a, b
    case 0x81: add_a_r8(cpu, r8_c); break;  // ADD a, c
    case 0x82: add_a_r8(cpu, r8_d); break;  // ADD a, d
    case 0x83: add_a_r8(cpu, r8_e); break;  // ADD a, e
    case 0x84: add_a_r8(cpu, r8_h); break;  // ADD a, h
    case 0x85: add_a_r8(cpu, r8_l); break;  // ADD a, l
    case 0x86: add_a_r8(cpu, r8_hl); break; // ADD a, [hl]
    case 0x87: add_a_r8(cpu, r8_a); break;  // ADD a, a

    case 0x88: adc_a_r8(cpu, r8_b); break;  // ADC a, b
    case 0x89: adc_a_r8(cpu, r8_c); break;  // ADC a, c
    case 0x8A: adc_a_r8(cpu, r8_d); break;  // ADC a, d
    case 0x8B: adc_a_r8(cpu, r8_e); break;  // ADC a, e
    case 0x8C: adc_a_r8(cpu, r8_h); break;  // ADC a, h
    case 0x8D: adc_a_r8(cpu, r8_l); break;  // ADC a, l
    case 0x8E: adc_a_r8(cpu, r8_hl); break; // ADC a, [hl]
    case 0x8F: adc_a_r8(cpu, r8_a); break;  // ADC a, a

    case 0x10: // STOP (implement later)
    case 0x76: // HALT (implement later)
    default: exit(1);
    }
}
