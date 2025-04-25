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
        case r16_bc:
            cpu->regs.c = bus_read(cpu->bus, cpu->regs.pc++);
            break;
        case r16_de:
            cpu->regs.e = bus_read(cpu->bus, cpu->regs.pc++);
            break;
        case r16_hl:
            cpu->regs.l = bus_read(cpu->bus, cpu->regs.pc++);
            break;
        case r16_sp:
            cpu->regs.sp = bus_read(cpu->bus, cpu->regs.pc++);
            break;
        }
        break;
    case 1:
        switch (dest) {
        case r16_bc:
            cpu->regs.b = bus_read(cpu->bus, cpu->regs.pc++);
            break;
        case r16_de:
            cpu->regs.d = bus_read(cpu->bus, cpu->regs.pc++);
            break;
        case r16_hl:
            cpu->regs.h = bus_read(cpu->bus, cpu->regs.pc++);
            break;
        case r16_sp:
            cpu->regs.sp += (uint16_t)bus_read(cpu->bus, cpu->regs.pc++) * 256;
            break;
        }
        break;
    case 2:
        prefetch(cpu);
        break;
    }
}

void sm83_m_cycle(struct sm83 *cpu) {
    switch (cpu->opcode) {
    case 0x00: // NOP
        nop(cpu);
        break;
    case 0x01: // LD bc, imm16
        ld_r16_imm16(cpu, r16_bc);
        break;
    case 0x11: // LD de, imm16
        ld_r16_imm16(cpu, r16_de);
        break;
    case 0x21: // LD hl, imm16
        ld_r16_imm16(cpu, r16_hl);
        break;
    case 0x31: // LD sp, imm16
        ld_r16_imm16(cpu, r16_sp);
        break;
    default:
        exit(1);
    }
}
