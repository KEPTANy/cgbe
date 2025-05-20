#include "internal/memory/bus.h"
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

static void add_a(struct sm83 *cpu, uint16_t x) {
    cpu->regs.f = ((cpu->regs.a & 0xF) + (x & 0xF) > 0xF) ? SM83_H_MASK : 0;
    cpu->regs.f |= (cpu->regs.a + x > 0xFF) ? SM83_C_MASK : 0;

    cpu->regs.a += x;

    cpu->regs.f |= (cpu->regs.a == 0) ? SM83_Z_MASK : 0;
}

static void adc_a(struct sm83 *cpu, uint16_t x) {
    if (cpu->regs.f & SM83_C_MASK) {
        x++;
    }
    add_a(cpu, x);
}

static void sub_a(struct sm83 *cpu, uint16_t x) {
    cpu->regs.f = ((cpu->regs.a & 0xF) < (x & 0xF)) ? SM83_H_MASK : 0;
    cpu->regs.f |= (cpu->regs.a < x) ? SM83_C_MASK : 0;

    cpu->regs.a -= x;

    cpu->regs.f |= (cpu->regs.a == 0) ? SM83_Z_MASK : 0;
}

static void sbc_a(struct sm83 *cpu, uint16_t x) {
    if (cpu->regs.f & SM83_C_MASK) {
        x++;
    }
    sub_a(cpu, x);
}

static void and_a(struct sm83 *cpu, uint8_t x) {
    cpu->regs.a &= x;

    cpu->regs.f = SM83_H_MASK;
    cpu->regs.f |= (cpu->regs.a == 0) ? SM83_Z_MASK : 0;
}

static void xor_a(struct sm83 *cpu, uint8_t x) {
    cpu->regs.a ^= x;
    cpu->regs.f |= (cpu->regs.a == 0) ? SM83_Z_MASK : 0;
}

static void or_a(struct sm83 *cpu, uint8_t x) {
    cpu->regs.a |= x;
    cpu->regs.f |= (cpu->regs.a == 0) ? SM83_Z_MASK : 0;
}

static void cp_a(struct sm83 *cpu, uint8_t x) {
    cpu->regs.f = SM83_N_MASK;
    cpu->regs.f |= (cpu->regs.a == x) ? SM83_Z_MASK : 0;
    cpu->regs.f |= (cpu->regs.a < x) ? SM83_C_MASK : 0;
    cpu->regs.f |= (cpu->regs.a & 0xF) < (x & 0xF) ? SM83_H_MASK : 0;
}

// returns true if reg is r8_hl
static bool load_from_r8(struct sm83 *cpu, uint8_t *dest, enum r8 source) {
    switch (source) {
    case r8_a: *dest = cpu->regs.a; break;
    case r8_b: *dest = cpu->regs.b; break;
    case r8_c: *dest = cpu->regs.c; break;
    case r8_d: *dest = cpu->regs.d; break;
    case r8_e: *dest = cpu->regs.e; break;
    case r8_h: *dest = cpu->regs.h; break;
    case r8_l: *dest = cpu->regs.l; break;
    case r8_hl: *dest = bus_read(cpu->bus, cpu->regs.hl); return true;
    }
    return false;
}

// returns true if reg is r8_hl
static bool load_to_r8(struct sm83 *cpu, enum r8 dest, uint8_t source) {
    switch (dest) {
    case r8_a: cpu->regs.a = source; break;
    case r8_b: cpu->regs.b = source; break;
    case r8_c: cpu->regs.c = source; break;
    case r8_d: cpu->regs.d = source; break;
    case r8_e: cpu->regs.e = source; break;
    case r8_h: cpu->regs.h = source; break;
    case r8_l: cpu->regs.l = source; break;
    case r8_hl: bus_write(cpu->bus, cpu->regs.hl, source); return true;
    }
    return false;
}

// NOP | Opcode: 0b00000000 | M-cycles: 1 | Flags: ----
static void nop(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);
    prefetch(cpu);
}

// LD r16, imm16 | Opcode: 0b00xx0001 | M-cycles: 3 | Flags: ----
static void ld_r16_imm16(struct sm83 *cpu, enum r16 dest) {
    assert(cpu->m_cycle < 3);

    switch (cpu->m_cycle++) {
    case 0: cpu->tmp.lo = bus_read(cpu->bus, cpu->regs.pc++); break;
    case 1: cpu->tmp.hi = bus_read(cpu->bus, cpu->regs.pc++); break;
    case 2:
        switch (dest) {
        case r16_bc: cpu->regs.bc = cpu->tmp.hilo; break;
        case r16_de: cpu->regs.de = cpu->tmp.hilo; break;
        case r16_hl: cpu->regs.hl = cpu->tmp.hilo; break;
        case r16_sp: cpu->regs.sp = cpu->tmp.hilo; break;
        }
        prefetch(cpu);
        break;
    }
}

// LD [r16mem], a | Opcode: 0b00xx0010 | M-cycles: 2 | Flags: ----
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

// LD a, [r16mem] | Opcode: 0b00xx1010 | M-cycles: 2 | Flags: ----
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

// LD [imm16], sp | Opcode: 0b00001000 | M-cycles: 5 | Flags: ----
static void ld_imm16_sp(struct sm83 *cpu) {
    assert(cpu->m_cycle < 5);

    switch (cpu->m_cycle++) {
    case 0: cpu->tmp.lo = bus_read(cpu->bus, cpu->regs.pc++); break;
    case 1: cpu->tmp.hi = bus_read(cpu->bus, cpu->regs.pc++); break;
    case 2: bus_write(cpu->bus, cpu->tmp.hilo++, cpu->regs.pc % 256); break;
    case 3: bus_write(cpu->bus, cpu->tmp.hilo, cpu->regs.pc / 256); break;
    case 4: prefetch(cpu); break;
    }
}

// INC r16 | Opcode: 0b00xx0011 | M-cycles: 2 | Flags: ----
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

// DEC r16 | Opcode: 0b00xx1011 | M-cycles: 2 | Flags: ----
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

// ADD hl, r16 | Opcode: 0b00xx1001 | M-cycles: 2 | Flags: -0HC
static void add_hl_r16(struct sm83 *cpu, enum r16 reg) {
    assert(cpu->m_cycle < 2);

    uint16_t x;
    uint16_t y;
    switch (cpu->m_cycle++) {
    // case 0:
    case 1:
        x = cpu->regs.hl;
        switch (reg) {
        case r16_bc: y = cpu->regs.bc; break;
        case r16_de: y = cpu->regs.de; break;
        case r16_hl: y = cpu->regs.hl; break;
        case r16_sp: y = cpu->regs.sp; break;
        }

        cpu->regs.f &= SM83_Z_MASK;
        cpu->regs.f |= ((x & 0xFFF) + (y & 0xFFF) > 0xFFF) ? SM83_H_MASK : 0;
        cpu->regs.f |= ((uint32_t)x + y > 0xFFFF) ? SM83_C_MASK : 0;

        cpu->regs.hl = x + y;

        prefetch(cpu);
        break;
    }
}

// INC r8 | Opcode: 0b00xxx100 | M-cycles: 1/3 | Flags: Z0H-
static void inc_r8(struct sm83 *cpu, enum r8 reg) {
    assert(cpu->m_cycle < 1 || (reg == r8_hl && cpu->m_cycle < 3));

    switch (cpu->m_cycle++) {
    case 0:
        bool one_more = load_from_r8(cpu, &cpu->tmp.lo, reg);

        cpu->regs.f &= SM83_C_MASK;
        cpu->regs.f |= ((cpu->tmp.lo ^ (cpu->tmp.lo + 1)) & (1 << 4)) ? SM83_H_MASK : 0;
        cpu->regs.f |= ((uint8_t)(cpu->tmp.lo++) == 0) ? SM83_Z_MASK : 0;

        if (one_more) {
            break;
        }
    case 1:
        if (load_to_r8(cpu, reg, cpu->tmp.lo)) {
            break;
        }
    case 2: prefetch(cpu); break;
    }
}

// DEC r8 | Opcode: 0b00xxx101 | M-cycles: 1/3 | Flags: Z0H-
static void dec_r8(struct sm83 *cpu, enum r8 reg) {
    assert(cpu->m_cycle < 1 || (reg == r8_hl && cpu->m_cycle < 3));

    switch (cpu->m_cycle++) {
    case 0:
        bool one_more = load_from_r8(cpu, &cpu->tmp.lo, reg);

        cpu->regs.f &= SM83_C_MASK;
        cpu->regs.f |= ((cpu->tmp.lo ^ (cpu->tmp.lo - 1)) & (1 << 4)) ? SM83_H_MASK : 0;
        cpu->regs.f |= ((uint8_t)(cpu->tmp.lo--) == 0) ? SM83_Z_MASK : 0;

        if (one_more) {
            break;
        }
    case 1:
        if (load_to_r8(cpu, reg, cpu->tmp.lo)) {
            break;
        }
    case 2: prefetch(cpu); break;
    }
}

// LD r8, imm8 | Opcode: 0b00xxx110 | M-cycles: 2/3 | Flags: ----
static void ld_r8_imm8(struct sm83 *cpu, enum r8 reg) {
    assert(cpu->m_cycle < 2 || (reg == r8_hl && cpu->m_cycle < 3));

    switch (cpu->m_cycle) {
    case 0: cpu->tmp.lo = bus_read(cpu->bus, cpu->regs.pc++); break;
    case 1:
        if (load_to_r8(cpu, reg, cpu->tmp.lo)) {
            break;
        }
    case 2: prefetch(cpu); break;
    }
}

// RLCA | Opcode: 0b00000111 | M-cycles: 1 | Flags: 000C
static void rlca(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    cpu->regs.f = (cpu->regs.a & (1 << 7)) ? SM83_C_MASK : 0;
    cpu->regs.a = (cpu->regs.a << 1) | (cpu->regs.f ? 1 : 0);
    prefetch(cpu);
}

// RRCA | Opcode: 0b00001111 | M-cycles: 1 | Flags: 000C
static void rrca(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    cpu->regs.f = (cpu->regs.a & 1) ? SM83_C_MASK : 0;
    cpu->regs.a = (cpu->regs.a >> 1) | (cpu->regs.f ? (1 << 7) : 0);
    prefetch(cpu);
}

// RLA | Opcode: 0b00010111 | M-cycles: 1 | Flags: 000C
static void rla(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    bool c = cpu->regs.f & SM83_C_MASK;
    cpu->regs.f = (cpu->regs.a & (1 << 7)) ? SM83_C_MASK : 0;
    cpu->regs.a = (cpu->regs.a << 1) | (c ? 1 : 0);
    prefetch(cpu);
}

// RRA | Opcode: 0b00011111 | M-cycles: 1 | Flags: 000C
static void rra(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    bool c = cpu->regs.f & SM83_C_MASK;
    cpu->regs.f = (cpu->regs.a & (1 << 7)) ? SM83_C_MASK : 0;
    cpu->regs.a = (cpu->regs.a << 1) | (c ? (1 << 7) : 0);
    prefetch(cpu);
}

// DAA | Opcode: 0b00100111 | M-cycles: 1 | Flags: Z-0C
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
    cpu->regs.f &= SM83_N_MASK;
    cpu->regs.f |= flags;

    prefetch(cpu);
}

// CPL | Opcode: 0b00101111 | M-cycles: 1 | Flags: -11-
static void cpl(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    cpu->regs.a = ~cpu->regs.a;
    cpu->regs.f |= SM83_N_MASK | SM83_H_MASK;
    prefetch(cpu);
}

// SCF | Opcode: 0b00110111 | M-cycles: 1 | Flags: -001
static void scf(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    cpu->regs.f &= SM83_Z_MASK;
    cpu->regs.f |= SM83_C_MASK;
    prefetch(cpu);
}

// CCF | Opcode: 0b00111111 | M-cycles: 1 | Flags: -00C
static void ccf(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);

    cpu->regs.f &= SM83_Z_MASK | SM83_C_MASK;
    cpu->regs.f ^= SM83_C_MASK;
    prefetch(cpu);
}

// JR imm8 | Opcode: 0x00011000 | M-cycles: 3 | Flags: ----
static void jr_imm8(struct sm83 *cpu) {
    assert(cpu->m_cycle < 3);

    switch (cpu->m_cycle++) {
    case 0: cpu->regs.pc += (int8_t)bus_read(cpu->bus, cpu->regs.pc++); break;
    // case 1:
    case 2: prefetch(cpu); break;
    }
}

// JR cond, imm8 | Opcode: 0x001xx000 | M-cycles: 2/3 | Flags: ----
static void jr_cond_imm8(struct sm83 *cpu, enum cond cc) {
    bool z = cpu->regs.f & SM83_Z_MASK;
    bool c = cpu->regs.f & SM83_C_MASK;

    bool cond = (cc == cond_z && z) || (cc == cond_nz && !z) || (cc == cond_c && c) ||
                (cc == cond_nc && !c);

    assert(cpu->m_cycle < 2 || (cond && cpu->m_cycle < 3));

    switch (cpu->m_cycle++) {
    case 0: cpu->tmp.lo = bus_read(cpu->bus, cpu->regs.pc++); break;
    case 1:
        if (cond) {
            cpu->regs.pc += (int8_t)cpu->tmp.lo;
            break;
        }
    case 2: prefetch(cpu); break;
    }
}

// LD r8, r8 | Opcode: 0x01dddsss | M-cycles: 1/2 | Flags: ----
static void ld_r8_r8(struct sm83 *cpu, enum r8 dest, enum r8 source) {
    assert(cpu->m_cycle < 1 || ((dest == r8_hl || source == r8_hl) && cpu->m_cycle < 2));

    switch (cpu->m_cycle++) {
    case 0:
        uint8_t tmp;
        bool one_more = load_from_r8(cpu, &tmp, source);
        one_more |= load_to_r8(cpu, dest, tmp);

        if (one_more) {
            break;
        }
    case 1: prefetch(cpu); break;
    }
}

enum mathop { op_add, op_adc, op_sub, op_sbc, op_and, op_xor, op_or, op_cp };

// ADD/ADC/SUB/SBC/AND/XOR/OR/CP a, r8
// Opcode: 0x10oooxxx | M-cycles: 1/2
static void mathop_a_r8(struct sm83 *cpu, enum mathop op, enum r8 reg) {
    assert(cpu->m_cycle < 1 || (reg == r8_hl && cpu->m_cycle < 2));

    switch (cpu->m_cycle++) {
    case 0:
        uint8_t tmp;
        bool one_more = load_from_r8(cpu, &tmp, reg);

        switch (op) {
        case op_add: add_a(cpu, tmp); break;
        case op_adc: adc_a(cpu, tmp); break;
        case op_sub: sub_a(cpu, tmp); break;
        case op_sbc: sbc_a(cpu, tmp); break;
        case op_and: and_a(cpu, tmp); break;
        case op_xor: xor_a(cpu, tmp); break;
        case op_or: or_a(cpu, tmp); break;
        case op_cp: cp_a(cpu, tmp); break;
        }

        if (one_more) {
            break;
        }
    case 1: prefetch(cpu); break;
    }
}

// ADD/ADC/SUB/SBC/AND/XOR/OR/CP a, imm8
// Opcode: 0x11ooo110 | M-cycles: 2
static void mathop_a_imm8(struct sm83 *cpu, enum mathop op) {
    assert(cpu->m_cycle < 2);

    switch (cpu->m_cycle++) {
    case 0:
        uint8_t arg = bus_read(cpu->bus, cpu->regs.pc++);

        switch (op) {
        case op_add: add_a(cpu, arg); break;
        case op_adc: adc_a(cpu, arg); break;
        case op_sub: sub_a(cpu, arg); break;
        case op_sbc: sbc_a(cpu, arg); break;
        case op_and: and_a(cpu, arg); break;
        case op_xor: xor_a(cpu, arg); break;
        case op_or: or_a(cpu, arg); break;
        case op_cp: cp_a(cpu, arg); break;
        }

        break;
    case 1: prefetch(cpu); break;
    }
}

// RET cond | Opcode: 0x110xx000 | M-cycles: 2/5 | Flags: ----
static void ret_cond(struct sm83 *cpu, enum cond cc) {
    bool z = cpu->regs.f & SM83_Z_MASK;
    bool c = cpu->regs.f & SM83_C_MASK;

    bool cond = (cc == cond_z && z) || (cc == cond_nz && !z) || (cc == cond_c && c) ||
                (cc == cond_nc && !c);

    assert(cpu->m_cycle < 2 || (cond && cpu->m_cycle < 5));

    switch (cpu->m_cycle++) {
    // case 0:
    case 1:
        if (cond) {
            cpu->tmp.lo = bus_read(cpu->bus, cpu->regs.sp++);
        } else {
            prefetch(cpu);
        }
        break;
    case 2:
        cpu->tmp.hi = bus_read(cpu->bus, cpu->regs.sp++);
        cpu->regs.pc = cpu->tmp.hilo;
        break;
    // case 3:
    case 4: prefetch(cpu); break;
    }
}

// RET | Opcode: 0x11001001 | M-cycles: 4 | Flags: ----
static void ret(struct sm83 *cpu) {
    assert(cpu->m_cycle < 4);

    switch (cpu->m_cycle++) {
    case 0: cpu->tmp.lo = bus_read(cpu->bus, cpu->regs.sp++); break;
    case 1:
        cpu->tmp.hi = bus_read(cpu->bus, cpu->regs.sp++);
        cpu->regs.pc = cpu->tmp.hilo;
        break;
    // case 2:
    case 3: prefetch(cpu); break;
    }
}

// JP cond, imm16 | Opcode: 0x110cc010 | M-cycles: 3/4 | Flags: ----
static void jp_cond_imm16(struct sm83 *cpu, enum cond cc) {
    bool z = cpu->regs.f & SM83_Z_MASK;
    bool c = cpu->regs.f & SM83_C_MASK;

    bool cond = (cc == cond_z && z) || (cc == cond_nz && !z) || (cc == cond_c && c) ||
                (cc == cond_nc && !c);

    assert(cpu->m_cycle < 3 || (cond && cpu->m_cycle < 4));

    switch (cpu->m_cycle++) {
    case 0: cpu->tmp.lo = bus_read(cpu->bus, cpu->regs.sp++); break;
    case 1: cpu->tmp.hi = bus_read(cpu->bus, cpu->regs.sp++); break;
    case 2:
        if (cc) {
            cpu->regs.pc = cpu->tmp.hilo;
            break;
        }
    case 3: prefetch(cpu); break;
    }
}

// JP imm16 | Opcode: 0x11000011 | M-cycles: 4 | Flags: ----
static void jp_imm16(struct sm83 *cpu) {
    assert(cpu->m_cycle < 4);

    switch (cpu->m_cycle++) {
    case 0: cpu->tmp.lo = bus_read(cpu->bus, cpu->regs.sp++); break;
    case 1: cpu->tmp.hi = bus_read(cpu->bus, cpu->regs.sp++); break;
    case 2: cpu->regs.pc = cpu->tmp.hilo; break;
    case 3: prefetch(cpu); break;
    }
}

// JP hl | Opcode: 0x11001001 | M-cycles: 1 | Flags: ----
static void jp_hl(struct sm83 *cpu) {
    assert(cpu->m_cycle < 1);
    cpu->regs.pc = cpu->regs.hl;
    prefetch(cpu);
}

// CALL cond, imm16 | Opcode: 0x110cc100 | M-cycles: 3/6 | Flags: ----
static void call_cond_imm16(struct sm83 *cpu, enum cond cc) {
    bool z = cpu->regs.f & SM83_Z_MASK;
    bool c = cpu->regs.f & SM83_C_MASK;

    bool cond = (cc == cond_z && z) || (cc == cond_nz && !z) || (cc == cond_c && c) ||
                (cc == cond_nc && !c);

    assert(cpu->m_cycle < 3 || (cond && cpu->m_cycle < 6));

    switch (cpu->m_cycle++) {
    case 0: cpu->tmp.lo = bus_read(cpu->bus, cpu->regs.sp++); break;
    case 1: cpu->tmp.hi = bus_read(cpu->bus, cpu->regs.sp++); break;
    case 2:
        if (!cc) {
            prefetch(cpu);
        }
        break;
    case 3: bus_write(cpu->bus, --cpu->regs.sp, cpu->regs.pc / 256); break;
    case 4: bus_write(cpu->bus, --cpu->regs.sp, cpu->regs.pc % 256); break;
    case 5: prefetch(cpu); break;
    }
}

// CALL imm16 | Opcode: 0x11001101 | M-cycles: 6 | Flags: ----
static void call_imm16(struct sm83 *cpu) {
    assert(cpu->m_cycle < 6);

    switch (cpu->m_cycle++) {
    case 0: cpu->tmp.lo = bus_read(cpu->bus, cpu->regs.sp++); break;
    case 1: cpu->tmp.hi = bus_read(cpu->bus, cpu->regs.sp++); break;
    // case 2:
    case 3: bus_write(cpu->bus, --cpu->regs.sp, cpu->regs.pc / 256); break;
    case 4: bus_write(cpu->bus, --cpu->regs.sp, cpu->regs.pc % 256); break;
    case 5: prefetch(cpu); break;
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
    case 0x2A: ld_a_r16mem(cpu, r16mem_hli); break; // LD a, [hl+]
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

    case 0x80: mathop_a_r8(cpu, op_add, r8_b); break;  // ADD a, b
    case 0x81: mathop_a_r8(cpu, op_add, r8_c); break;  // ADD a, c
    case 0x82: mathop_a_r8(cpu, op_add, r8_d); break;  // ADD a, d
    case 0x83: mathop_a_r8(cpu, op_add, r8_e); break;  // ADD a, e
    case 0x84: mathop_a_r8(cpu, op_add, r8_h); break;  // ADD a, h
    case 0x85: mathop_a_r8(cpu, op_add, r8_l); break;  // ADD a, l
    case 0x86: mathop_a_r8(cpu, op_add, r8_hl); break; // ADD a, [hl]
    case 0x87: mathop_a_r8(cpu, op_add, r8_a); break;  // ADD a, a

    case 0x88: mathop_a_r8(cpu, op_adc, r8_b); break;  // ADC a, b
    case 0x89: mathop_a_r8(cpu, op_adc, r8_c); break;  // ADC a, c
    case 0x8A: mathop_a_r8(cpu, op_adc, r8_d); break;  // ADC a, d
    case 0x8B: mathop_a_r8(cpu, op_adc, r8_e); break;  // ADC a, e
    case 0x8C: mathop_a_r8(cpu, op_adc, r8_h); break;  // ADC a, h
    case 0x8D: mathop_a_r8(cpu, op_adc, r8_l); break;  // ADC a, l
    case 0x8E: mathop_a_r8(cpu, op_adc, r8_hl); break; // ADC a, [hl]
    case 0x8F: mathop_a_r8(cpu, op_adc, r8_a); break;  // ADC a, a

    case 0x90: mathop_a_r8(cpu, op_sub, r8_b); break;  // SUB a, b
    case 0x91: mathop_a_r8(cpu, op_sub, r8_c); break;  // SUB a, c
    case 0x92: mathop_a_r8(cpu, op_sub, r8_d); break;  // SUB a, d
    case 0x93: mathop_a_r8(cpu, op_sub, r8_e); break;  // SUB a, e
    case 0x94: mathop_a_r8(cpu, op_sub, r8_h); break;  // SUB a, h
    case 0x95: mathop_a_r8(cpu, op_sub, r8_l); break;  // SUB a, l
    case 0x96: mathop_a_r8(cpu, op_sub, r8_hl); break; // SUB a, [hl]
    case 0x97: mathop_a_r8(cpu, op_sub, r8_a); break;  // SUB a, a

    case 0x98: mathop_a_r8(cpu, op_sbc, r8_b); break;  // SBC a, b
    case 0x99: mathop_a_r8(cpu, op_sbc, r8_c); break;  // SBC a, c
    case 0x9A: mathop_a_r8(cpu, op_sbc, r8_d); break;  // SBC a, d
    case 0x9B: mathop_a_r8(cpu, op_sbc, r8_e); break;  // SBC a, e
    case 0x9C: mathop_a_r8(cpu, op_sbc, r8_h); break;  // SBC a, h
    case 0x9D: mathop_a_r8(cpu, op_sbc, r8_l); break;  // SBC a, l
    case 0x9E: mathop_a_r8(cpu, op_sbc, r8_hl); break; // SBC a, [hl]
    case 0x9F: mathop_a_r8(cpu, op_sbc, r8_a); break;  // SBC a, a

    case 0xA0: mathop_a_r8(cpu, op_and, r8_b); break;  // AND a, b
    case 0xA1: mathop_a_r8(cpu, op_and, r8_c); break;  // AND a, c
    case 0xA2: mathop_a_r8(cpu, op_and, r8_d); break;  // AND a, d
    case 0xA3: mathop_a_r8(cpu, op_and, r8_e); break;  // AND a, e
    case 0xA4: mathop_a_r8(cpu, op_and, r8_h); break;  // AND a, h
    case 0xA5: mathop_a_r8(cpu, op_and, r8_l); break;  // AND a, l
    case 0xA6: mathop_a_r8(cpu, op_and, r8_hl); break; // AND a, [hl]
    case 0xA7: mathop_a_r8(cpu, op_and, r8_a); break;  // AND a, a

    case 0xA8: mathop_a_r8(cpu, op_xor, r8_b); break;  // XOR a, b
    case 0xA9: mathop_a_r8(cpu, op_xor, r8_c); break;  // XOR a, c
    case 0xAA: mathop_a_r8(cpu, op_xor, r8_d); break;  // XOR a, d
    case 0xAB: mathop_a_r8(cpu, op_xor, r8_e); break;  // XOR a, e
    case 0xAC: mathop_a_r8(cpu, op_xor, r8_h); break;  // XOR a, h
    case 0xAD: mathop_a_r8(cpu, op_xor, r8_l); break;  // XOR a, l
    case 0xAE: mathop_a_r8(cpu, op_xor, r8_hl); break; // XOR a, [hl]
    case 0xAF: mathop_a_r8(cpu, op_xor, r8_a); break;  // XOR a, a

    case 0xB0: mathop_a_r8(cpu, op_or, r8_b); break;  // OR a, b
    case 0xB1: mathop_a_r8(cpu, op_or, r8_c); break;  // OR a, c
    case 0xB2: mathop_a_r8(cpu, op_or, r8_d); break;  // OR a, d
    case 0xB3: mathop_a_r8(cpu, op_or, r8_e); break;  // OR a, e
    case 0xB4: mathop_a_r8(cpu, op_or, r8_h); break;  // OR a, h
    case 0xB5: mathop_a_r8(cpu, op_or, r8_l); break;  // OR a, l
    case 0xB6: mathop_a_r8(cpu, op_or, r8_hl); break; // OR a, [hl]
    case 0xB7: mathop_a_r8(cpu, op_or, r8_a); break;  // OR a, a

    case 0xB8: mathop_a_r8(cpu, op_cp, r8_b); break;  // CP a, b
    case 0xB9: mathop_a_r8(cpu, op_cp, r8_c); break;  // CP a, c
    case 0xBA: mathop_a_r8(cpu, op_cp, r8_d); break;  // CP a, d
    case 0xBB: mathop_a_r8(cpu, op_cp, r8_e); break;  // CP a, e
    case 0xBC: mathop_a_r8(cpu, op_cp, r8_h); break;  // CP a, h
    case 0xBD: mathop_a_r8(cpu, op_cp, r8_l); break;  // CP a, l
    case 0xBE: mathop_a_r8(cpu, op_cp, r8_hl); break; // CP a, [hl]
    case 0xBF: mathop_a_r8(cpu, op_cp, r8_a); break;  // CP a, a

    case 0xC6: mathop_a_imm8(cpu, op_add); break; // ADD a, imm8
    case 0xCE: mathop_a_imm8(cpu, op_adc); break; // ADC a, imm8
    case 0xD6: mathop_a_imm8(cpu, op_sub); break; // SUB a, imm8
    case 0xDE: mathop_a_imm8(cpu, op_sbc); break; // SBC a, imm8
    case 0xE6: mathop_a_imm8(cpu, op_and); break; // AND a, imm8
    case 0xEE: mathop_a_imm8(cpu, op_xor); break; // XOR a, imm8
    case 0xF6: mathop_a_imm8(cpu, op_or); break;  // OR a, imm8
    case 0xFE: mathop_a_imm8(cpu, op_cp); break;  // CP a, imm8

    case 0xC0: ret_cond(cpu, cond_nz); break; // RET NZ
    case 0xC8: ret_cond(cpu, cond_z); break;  // RET Z
    case 0xC9: ret(cpu); break;               // RET
    case 0xD0: ret_cond(cpu, cond_nc); break; // RET NC
    case 0xD8: ret_cond(cpu, cond_c); break;  // RET C
    case 0xD9: exit(1);                       // RETI (implement later)

    case 0xC2: jp_cond_imm16(cpu, cond_nz); break; // JP nz, imm16
    case 0xCA: jp_cond_imm16(cpu, cond_z); break;  // JP z, imm16
    case 0xD2: jp_cond_imm16(cpu, cond_nc); break; // JP nc, imm16
    case 0xDA: jp_cond_imm16(cpu, cond_c); break;  // JP c, imm16

    case 0xC3: jp_imm16(cpu); break; // JP imm16
    case 0xE9: jp_hl(cpu); break;    // JP hl

    case 0xC4: call_cond_imm16(cpu, cond_nz); break; // CALL nz, imm16
    case 0xCC: call_cond_imm16(cpu, cond_z); break;  // CALL z, imm16
    case 0xD4: call_cond_imm16(cpu, cond_nc); break; // CALL nc, imm16
    case 0xDC: call_cond_imm16(cpu, cond_c); break;  // CALL c, imm16

    case 0xCD: call_imm16(cpu); break; // CALL imm16

    case 0x10: exit(1); // STOP (implement later)
    case 0x76: exit(1); // HALT (implement later)
    default: break;     // Invalid opcodes, pc doesn't change making an inf loop
    }
}
