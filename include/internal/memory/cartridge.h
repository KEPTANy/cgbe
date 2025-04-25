#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct cartridge_header {
    uint8_t entry[4];
    uint8_t logo[48];
    union {
        char old_title[15]; // not necessarily null-terminated
        struct {
            char new_title[11]; // not necessarily null-terminated
            char manufacturer[4];
        };
    };
    uint8_t cgb;
    char new_licensee_code[2];
    uint8_t sgb;
    uint8_t cartridge_type;
    uint8_t rom_size;
    uint8_t ram_size;
    uint8_t destination_code;
    uint8_t old_licensee_code;
    uint8_t version;
    uint8_t header_checksum;
    uint16_t global_checksum;
};

enum cartridge_mapper_type { CMT_ROM_ONLY, CMT_UNSUPPORTED = -1 };

#define CARTRIDGE_ROM_BANK_SIZE 16384

struct cartridge {
    enum cartridge_mapper_type mapper;
    struct cartridge_header *header;
    uint8_t bank_00[CARTRIDGE_ROM_BANK_SIZE];
    uint8_t bank_01_nn[CARTRIDGE_ROM_BANK_SIZE];
};

// Constructs cartridge rom data from a file.
struct cartridge *cartridge_new(const char *fname);

// Deallocates cartridge and all the memory associated with it.
void cartridge_delete(struct cartridge *cart);

// Prints header.
void cartridge_header_print_info(const struct cartridge_header *cart, FILE *out);

// Reads data from rom of a cartridge.
uint8_t cartridge_rom_read(struct cartridge *cart, uint16_t address);

// Writes data to a rom of a cartridge.
void cartridge_rom_write(struct cartridge *cart, uint16_t address, uint8_t val);

#endif
