#include <stddef.h>

#define MAPPER_IMPL
#include "mapper.h"
#include "bus.h"
#include "ppu.h"
#include "ram_device.h"

/*
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| Address Range     | Description                   | Notes                                                                                                            |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| CPU $0000-$07FF   | Internal RAM                  | 2 KiB of internal RAM, mirrored every 2 KiB up to $1FFF                                                          |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| CPU $0800-$1FFF   | Mirrors of $0000-$07FF        | Mirrored internal RAM                                                                                            |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| CPU $2000-$2007   | PPU Registers                 | Registers for the Picture Processing Unit (PPU), mirrored every 8 bytes up to $3FFF                              |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| CPU $2008-$3FFF   | Mirrors of $2000-$2007        | Mirrored PPU registers                                                                                           |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| CPU $4000-$4017   | APU and I/O Registers         | Registers for the Audio Processing Unit (APU) and I/O ports                                                      |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| CPU $4018-$401F   | APU and I/O Test Registers    | Typically disabled in normal operation                                                                           |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| CPU $4020-$5FFF   | Expansion ROM                 | Used by some cartridges for additional ROM or hardware                                                           |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| CPU $6000-$7FFF   | PRG RAM                       | Family Basic only: mirrored as necessary to fill entire 8 KiB window, write protectable with an external switch  |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| CPU $8000-$BFFF   | First 16 KB of ROM            |                                                                                                                  |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| CPU $C000-$FFFF   | Last 16 KB of ROM (NROM-256)  | Mirror of $8000-$BFFF (NROM-128)                                                                                 |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
*/

mapper_return_t NROM_probe_ines(ines_header_t a_ines_hdr)
{
    if (a_ines_hdr->m_prg_rom_size == 0)
    {
        return MAPPER_INES_VALUE_INVALID;
    }
    
    if (a_ines_hdr->m_chr_rom_size == 0)
    {
        // return MAPPER_INES_VALUE_INVALID;
    }

    return MAPPER_OK;
}

mapper_return_t NROM_map_ines(ines_header_t a_ines_hdr, bus_t a_bus, bus_device_t a_ppu)
{
    uint8_t *ines_file = (uint8_t *)&a_ines_hdr[1];

    // If there is a trainer, skip 512 bytes
    if (a_ines_hdr->m_flags_6 & INES_FLAG_6_TRAINER)
    {
        ines_file += 512;
    }

    size_t prg_rom_size_in_bytes = a_ines_hdr->m_prg_rom_size * 0x4000;

    // Create a PRG RAM device, they are usually  2 or 4 KiB and are mirrored to fill the entire 8 KiB range
    // We just create a 8 KiB device
    bus_device_t prg_ram = ram_device_create(0x2000);

    // Map the PRG RAM to the bus at 0x6000
    a_bus->attach(prg_ram, 0x6000, 0x2000);

    // Create a PRG ROM device for the first 16 KiB of the PRG ROM
    bus_device_t prg_rom_0 = ram_device_create(0x4000);

    // Attach the PRG ROM to the bus at address 0x8000
    a_bus->attach(prg_rom_0, 0x8000, 0x4000);

    // Copy the first 16 KiB of the PRG ROM to the PRG ROM device
    ram_device_write_buffer(prg_rom_0, 0, ines_file, 0x4000);

    if (a_ines_hdr->m_prg_rom_size > 1)
    {
        // NROM-256

        // Create a PRG ROM device for the second 16 KiB of the PRG ROM
        bus_device_t prg_rom_1 = ram_device_create(0x4000);

        // Attach the PRG ROM to the bus at address 0xA000
        a_bus->attach(prg_rom_1, 0xC000, 0x4000);

        // Copy the second 16 KiB of the PRG ROM to the PRG ROM device
        ram_device_write_buffer(prg_rom_1, 0, ines_file + 0x4000, 0x4000);
    }
    else
    {
        // NROM-128

        // Just mirror the first 16 KiB of the PRG ROM to the second 16 KiB
        a_bus->attach(prg_rom_0, 0xC000, 0x4000);
    }

    size_t chr_rom_size_in_bytes = a_ines_hdr->m_chr_rom_size * 0x2000;

    uint8_t *chr_rom = ines_file + prg_rom_size_in_bytes;
    
    // Create a CHR ROM device for pattern table 0
    bus_device_t chr_rom_pattern_table_0 = ram_device_create(0x1000);

    // Attach the CHR ROM to the bus at address 0x0000
    ppu_device_attach(a_ppu, chr_rom_pattern_table_0, 0x0000, 0x1000);

    if (chr_rom_size_in_bytes > 0)
    {
        // Copy the first 4 KiB of the CHR ROM to the CHR ROM device
        ram_device_write_buffer(chr_rom_pattern_table_0, 0, chr_rom, 0x1000);
    }

    // Create a CHR ROM device for pattern table 1
    bus_device_t chr_rom_pattern_table_1 = ram_device_create(0x1000);

    // Attach the CHR ROM to the bus at address 0x1000
    ppu_device_attach(a_ppu, chr_rom_pattern_table_1, 0x1000, 0x1000);

    if (chr_rom_size_in_bytes > 0)
    {
        // Copy the last 4 KiB of the CHR ROM to the CHR ROM device
        ram_device_write_buffer(chr_rom_pattern_table_1, 0, chr_rom + 0x1000, 0x1000);
    }
    // Create the 2 nametable devices
    /*
          (0,0)     (256,0)     (511,0)
            +-----------+-----------+
            |           |           |
            |           |           |
            |   $2000   |   $2400   |
            |           |           |
            |           |           |
     (0,240)+-----------+-----------+(511,240)
            |           |           |
            |           |           |
            |   $2800   |   $2C00   |
            |           |           |
            |           |           |
            +-----------+-----------+
          (0,479)   (256,479)   (511,479)
    */
    for (int i = 0; i < 2; i++)
    {
        bus_device_t nametable = ram_device_create(0x400);

        // Attach the nametable to the bus at address 0x2000
        ppu_device_attach(a_ppu, nametable, (0x2000 + (i * 0x0400)) + 0x0000, 0x0400);
        ppu_device_attach(a_ppu, nametable, (0x2000 + (i * 0x0400)) + 0x0800, 0x0400);
    }

    return MAPPER_OK;
}