#ifndef BUS_H
#define BUS_H

#include "internal/memory/cartridge.h"

struct bus {
    struct cartridge *cart;
};

// Constructs a new bus.
struct bus *bus_new(struct cartridge *cart);

// Deallocates a bus, doesn't touch the connected devices (cartridge, etc).
void bus_delete(struct bus *bus);

// Reads data.
uint8_t bus_rom_read(struct bus *bus, uint16_t address);

// Writes data.
void bus_rom_write(struct bus *bus, uint16_t address, uint8_t val);

#endif
