#ifndef SM83_H
#define SM83_H

#include <stdint.h>

#include "internal/memory/bus.h"
#include "util.h"

#if defined(LITTLE_ENDIAN)
#define SM83_REGISTER_PAIR(hi, lo)                                             \
    union {                                                                    \
        uint16_t hi##lo;                                                       \
        struct {                                                               \
            uint8_t lo;                                                        \
            uint8_t hi;                                                        \
        };                                                                     \
    }
#elif defined(BIG_ENDIAN)
#define SM83_REGISTER_PAIR(hi, lo)                                             \
    union {                                                                    \
        uint16_t hi##lo;                                                       \
        struct {                                                               \
            uint8_t hi;                                                        \
            uint8_t lo;                                                        \
        };                                                                     \
    }
#endif

#define SM83_Z_MASK (1 << 7)
#define SM83_N_MASK (1 << 6)
#define SM83_H_MASK (1 << 5)
#define SM83_C_MASK (1 << 4)

struct sm83_register_file {
    SM83_REGISTER_PAIR(a, f);
    SM83_REGISTER_PAIR(b, c);
    SM83_REGISTER_PAIR(d, e);
    SM83_REGISTER_PAIR(h, l);

    uint16_t pc;
    uint16_t sp;
};

struct sm83 {
    struct sm83_register_file regs;

    struct bus *bus;
};

// Allocates and initializes a new SM83 core.
struct sm83 *sm83_new(struct bus *bus);

// Deallocates the core, bus isn't deleted.
void sm83_delete(struct sm83 *cpu);

#endif
