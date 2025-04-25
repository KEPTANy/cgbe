#include "internal/memory/cartridge.h"

#include <assert.h>
#include <stdlib.h>

static enum cartridge_mapper_type cartridge_mapper_type(uint8_t cart_type) {
    switch (cart_type) {
    case 0x00: return CMT_ROM_ONLY;
    default: return CMT_UNSUPPORTED;
    }
}

struct cartridge *cartridge_new(const char *fname) {
    assert(fname != NULL);

    struct cartridge *cart = malloc(sizeof(struct cartridge));
    assert(cart != NULL);

    FILE *file = fopen(fname, "r");
    assert(file != NULL);

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    assert(file_size == 2 * CARTRIDGE_ROM_BANK_SIZE);

    fread(cart->bank_00, CARTRIDGE_ROM_BANK_SIZE, sizeof(uint8_t), file);
    fread(cart->bank_01_nn, CARTRIDGE_ROM_BANK_SIZE, sizeof(uint8_t), file);
    cart->header = (void *)(cart->bank_00 + 0x0100);

    fclose(file);

    cart->mapper = cartridge_mapper_type(cart->header->cartridge_type);
    assert(cart->mapper != CMT_UNSUPPORTED);

    return cart;
}

void cartridge_delete(struct cartridge *cart) {
    if (cart == NULL) {
        return;
    }

    switch (cart->mapper) {
    case CMT_ROM_ONLY: free(cart); return;
    case CMT_UNSUPPORTED: exit(1);
    }
}

void cartridge_header_print_info(const struct cartridge_header *cart, FILE *out) {
    assert(cart != NULL);
    assert(out != NULL);

    fprintf(out, "Old title: %.16s\n", cart->old_title);
    fprintf(out, "New title: %.16s\n", cart->new_title);
    fprintf(out, "Manufacturer: %.16s\n", cart->manufacturer);
    fprintf(out, "CGB: %u\n", (unsigned int)cart->cgb);
    fprintf(out, "New lic code: %.2s\n", cart->new_licensee_code);
    fprintf(out, "SGB: %u\n", (unsigned int)cart->sgb);
    fprintf(out, "Cart type: %u\n", (unsigned int)cart->cartridge_type);
    fprintf(out, "Rom size: %u\n", (unsigned int)cart->rom_size);
    fprintf(out, "Ram size: %u\n", (unsigned int)cart->ram_size);
    fprintf(out, "Dest code: %u\n", (unsigned int)cart->destination_code);
    fprintf(out, "Old lic code: %u\n", (unsigned int)cart->old_licensee_code);
    fprintf(out, "Version: %u\n", (unsigned int)cart->version);
    fprintf(out, "Header checksum: %u\n", (unsigned int)cart->header_checksum);
    fprintf(out, "Global checksum: %u\n", (unsigned int)cart->global_checksum);
}

uint8_t cartridge_rom_read(struct cartridge *cart, uint16_t address) {
    assert(cart != NULL);
    assert(0x0000 <= address && address <= 0x7FFF);

    switch (cart->mapper) {
    case CMT_ROM_ONLY:
        return address >= 0x4000 ? cart->bank_01_nn[address - 0x4000]
                                 : cart->bank_00[address - 0x0000];
    case CMT_UNSUPPORTED: exit(1);
    }
}

void cartridge_rom_write(struct cartridge *cart, uint16_t address, uint8_t val) {
    assert(cart != NULL);
    assert(0x0000 <= address && address <= 0x7FFF);

    switch (cart->mapper) {
    case CMT_ROM_ONLY: return;
    case CMT_UNSUPPORTED: exit(1);
    }
}
