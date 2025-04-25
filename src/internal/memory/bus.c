#include "internal/memory/bus.h"

#include <assert.h>
#include <stdlib.h>

struct bus *bus_new(struct cartridge *cart) {
    struct bus *bus = malloc(sizeof(struct bus));
    assert(bus != NULL);

    bus->cart = cart;

    return bus;
}

void bus_delete(struct bus *bus) { free(bus); }

uint8_t bus_read(struct bus *bus, uint16_t address) {
    if (0x0000 <= address && address <= 0x7FFF) {
        return cartridge_rom_read(bus->cart, address);
    } else {
        exit(1);
    }
}

void bus_write(struct bus *bus, uint16_t address, uint8_t val) {
    if (0x0000 <= address && address <= 0x7FFF) {
        cartridge_rom_write(bus->cart, address, val);
    } else {
        exit(1);
    }
}
