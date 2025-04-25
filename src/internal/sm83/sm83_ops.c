#include "internal/sm83/sm83.h"

#include <assert.h>
#include <stdlib.h>

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
static void ld_r16_imm16(struct sm83 *cpu, uint16_t *dest) {
    assert(cpu->m_cycle < 3);

    switch (cpu->m_cycle++) {
    case 0:
        // read low byte
        *dest = bus_read(cpu->bus, cpu->regs.pc++);
        break;
    case 1:
        // read high byte
        *dest += (uint16_t)bus_read(cpu->bus, cpu->regs.pc++) * 256;
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
        ld_r16_imm16(cpu, &cpu->regs.bc);
        break;
    case 0x11: // LD de, imm16
        ld_r16_imm16(cpu, &cpu->regs.de);
        break;
    case 0x21: // LD hl, imm16
        ld_r16_imm16(cpu, &cpu->regs.hl);
        break;
    case 0x31: // LD hl, imm16
        ld_r16_imm16(cpu, &cpu->regs.sp);
        break;
    default:
        exit(1);
    }
}
