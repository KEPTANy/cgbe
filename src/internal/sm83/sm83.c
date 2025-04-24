#include "internal/sm83/sm83.h"

#include <stdlib.h>

struct sm83 *sm83_new(struct bus *bus) {
    struct sm83 *cpu = malloc(sizeof(struct sm83));

    cpu->bus = bus;

    cpu->regs.af = 0;
    cpu->regs.bc = 0;
    cpu->regs.de = 0;
    cpu->regs.hl = 0;
    cpu->regs.pc = 0;
    cpu->regs.sp = 0;

    return cpu;
}

void sm83_delete(struct sm83 *cpu) { free(cpu); }
