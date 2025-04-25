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

    switch (cpu->m_cycle++) {
    case 0:
        prefetch(cpu);
        break;
    }
}

void sm83_m_cycle(struct sm83 *cpu) {
    switch (cpu->opcode) {
    case 0x00: // NOP
        nop(cpu);
        break;
    default:
        exit(1);
    }
}
