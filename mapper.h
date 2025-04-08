#pragma once
#include "hw_types.h"

typedef struct ines_header_data *ines_header_t;

typedef enum mapper_return
{
        MAPPER_OK = 0,
        MAPPER_UNSUPPORTED = -1,
        MAPPER_INES_HEADER_INVALID = -2,
        MAPPER_INES_VALUE_INVALID = -3,
} mapper_return_t;

mapper_return_t mapper_map_ines(ines_header_t a_ines_hdr, bus_t a_bus, bus_device_t a_ppu);

#ifdef MAPPER_IMPL

// Defines for flags 6
#define INES_FLAG_6_MIRRORING_MASK 0x03
#define INES_FLAG_6_MIRRORING_HORIZONTAL 0x00
#define INES_FLAG_6_MIRRORING_VERTICAL 0x01
#define INES_FLAG_6_MIRRORING_FOUR_SCREEN 0x03
#define INES_FLAG_6_BATTERY_BACKED_RAM 0x02
#define INES_FLAG_6_TRAINER 0x04
#define INES_FLAG_6_ALT_NAMETABLE_LAYOUT 0x08

#pragma pack(push, 1)
struct ines_header_data
{
    uint8_t m_magic[4];
    uint8_t m_prg_rom_size;
    uint8_t m_chr_rom_size;
    uint8_t m_flags_6;
    uint8_t m_flags_7;
    uint8_t m_prg_ram_size;
    uint8_t m_flags_9;
    uint8_t m_flags_10;
    uint8_t m_zeros[5];
};
#pragma pack(pop)

#define MAPPERS    \
        X(NROM, 0) \
        X(MMC1, 1) \

#define X(name, id) mapper_return_t name##_probe_ines(ines_header_t a_ines_hdr);
MAPPERS
#undef X

#define X(name, id) mapper_return_t name##_map_ines(ines_header_t a_ines_hdr, bus_t a_bus, bus_device_t a_ppu);
MAPPERS
#undef X

#endif
