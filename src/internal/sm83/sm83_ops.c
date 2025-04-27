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
    case 0:
        switch (dest) {
        case r16_bc: cpu->regs.c = bus_read(cpu->bus, cpu->regs.pc++); break;
        case r16_de: cpu->regs.e = bus_read(cpu->bus, cpu->regs.pc++); break;
        case r16_hl: cpu->regs.l = bus_read(cpu->bus, cpu->regs.pc++); break;
        case r16_sp: cpu->regs.sp = bus_read(cpu->bus, cpu->regs.pc++); break;
        }
        break;
    case 1:
        switch (dest) {
        case r16_bc: cpu->regs.b = bus_read(cpu->bus, cpu->regs.pc++); break;
        case r16_de: cpu->regs.d = bus_read(cpu->bus, cpu->regs.pc++); break;
        case r16_hl: cpu->regs.h = bus_read(cpu->bus, cpu->regs.pc++); break;
        case r16_sp: cpu->regs.sp += (uint16_t)bus_read(cpu->bus, cpu->regs.pc++) * 256; break;
        }
        break;
    case 2: prefetch(cpu); break;
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
        switch (dest) {
        case r16mem_bc: bus_write(cpu->bus, cpu->regs.bc, cpu->regs.a); break;
        case r16mem_de: bus_write(cpu->bus, cpu->regs.de, cpu->regs.a); break;
        case r16mem_hli: bus_write(cpu->bus, cpu->regs.hl++, cpu->regs.a); break;
        case r16mem_hld: bus_write(cpu->bus, cpu->regs.hl--, cpu->regs.a); break;
        }
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
        switch (source) {
        case r16mem_bc: cpu->regs.a = bus_read(cpu->bus, cpu->regs.bc); break;
        case r16mem_de: cpu->regs.a = bus_read(cpu->bus, cpu->regs.de); break;
        case r16mem_hli: cpu->regs.a = bus_read(cpu->bus, cpu->regs.hl++); break;
        case r16mem_hld: cpu->regs.a = bus_read(cpu->bus, cpu->regs.hl--); break;
        }
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
    case 0:
        switch (reg) {
        case r16_bc: cpu->regs.bc++; break;
        case r16_de: cpu->regs.de++; break;
        case r16_hl: cpu->regs.hl++; break;
        case r16_sp: cpu->regs.sp++; break;
        }
        break;
    case 1: prefetch(cpu); break;
    }
}

// DEC r16
// Opcode: 0b00xx1011
// M-cycles: 2
// Flags: ----
static void dec_r16(struct sm83 *cpu, enum r16 reg) {
    assert(cpu->m_cycle < 2);

    switch (cpu->m_cycle++) {
    case 0:
        switch (reg) {
        case r16_bc: cpu->regs.bc--; break;
        case r16_de: cpu->regs.de--; break;
        case r16_hl: cpu->regs.hl--; break;
        case r16_sp: cpu->regs.sp--; break;
        }
        break;
    case 1: prefetch(cpu); break;
    }
}

// ADD hl, r16
// Opcode: 0b00xx1001
// M-cycles: 2
// Flags: -0HC
static void add_hl_r16(struct sm83 *cpu, enum r16 reg) {
    assert(cpu->m_cycle < 2);

    uint32_t sum = 0;
    switch (cpu->m_cycle++) {
    // case 0:
    case 1:
        cpu->regs.f &= ~(SM83_N_MASK & SM83_H_MASK & SM83_C_MASK); // N = 0, H = 0, C = 0

        sum = cpu->regs.hl;
        switch (reg) {
        case r16_bc: sum += cpu->regs.bc; break;
        case r16_de: sum += cpu->regs.de; break;
        case r16_hl: sum += cpu->regs.hl; break;
        case r16_sp: sum += cpu->regs.sp; break;
        }
        cpu->regs.hl = sum;

        if (sum & (1 << 12)) {
            cpu->regs.f |= SM83_H_MASK;
        }

        if (sum & (1 << 16)) {
            cpu->regs.f |= SM83_C_MASK;
        }

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

            if (cpu->tmp & (1 << 4)) {
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

        if (res & (1 << 4)) {
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

            if (cpu->tmp & (1 << 4)) {
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

        if (res & (1 << 4)) {
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

    default: exit(1);
    }
}
