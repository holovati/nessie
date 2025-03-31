#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <utime.h>
#include <time.h>

#include "cpu.h"
#include "bus.h"
#include "ppu.h"
#include "ram_device.h"

// NES Memory Map
/*
$0000–$07FF	$0800	2 KB internal RAM
$0800–$0FFF	$0800	Mirrors of $0000–$07FF
$1000–$17FF	$0800   Mirrors of $0000–$07FF
$1800–$1FFF	$0800   Mirrors of $0000–$07FF
$2000–$2007	$0008	NES PPU registers
$2008–$3FFF	$1FF8	Mirrors of $2000–$2007 (repeats every 8 bytes)
$4000–$4017	$0018	NES APU and I/O registers
$4018–$401F	$0008	APU and I/O functionality that is normally disabled. See CPU Test Mode.
$4020–$FFFF $BFE0   Unmapped. Available for cartridge use.
$6000–$7FFF $2000   Usually cartridge RAM, when present. 
$8000–$FFFF $8000   Usually cartridge ROM and mapper registers.
*/

static char const  * const s_test_rom_files[] = {
    "01-implied.nes",
    "pacman.nes",
};

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

int main()
{
    struct cpu_data cpu;
    
    struct bus_data bus;
        
    bus.initialize();
    
    // Create a 2KB RAM device
    bus_device_t internal_ram = ram_device_create(0x800);
    
    // Attach the internal RAM to the bus at address 0. The first 0x800 will be mirrored at 0x0800, 0x1000, 0x1800
    bus.attach(internal_ram, 0, 0x2000); 

    // Create a PPU device
    bus_device_t ppu = ppu_device_create();
    
    // Attach the PPU to the bus at address 0x2000.
    // The PPU registers are mirrored every 8 bytes from 0x2000 to 0x3FFF 
    bus.attach(ppu, 0x2000, 0x2000);

    // Load the test ROM file
    for (int test_idx = 0; test_idx < sizeof(s_test_rom_files) / sizeof(s_test_rom_files[0]); test_idx++)
    {
        FILE *file = fopen(s_test_rom_files[test_idx], "rb");
        
        if (!file)
        {
            fprintf(stderr, "Failed to open file %s\n", s_test_rom_files[test_idx]);
            return 1;
        }
        
        ines_header_data header;
        
        fread(&header, sizeof(header), 1, file);
        
        if (header.m_magic[0] != 'N' || header.m_magic[1] != 'E' || header.m_magic[2] != 'S' || header.m_magic[3] != 0x1A)
        {
            fprintf(stderr, "Invalid iNES header\n");
            return 1;
        }
        
        if (header.m_prg_rom_size == 0)
        {
            fprintf(stderr, "No PRG ROM data\n");
            return 1;
        }
        
        if (header.m_chr_rom_size == 0)
        {
            fprintf(stderr, "No CHR ROM data\n");
            return 1;
        }

        // Extract mapper number from flags 6 and 7
        uint8_t mapper = (header.m_flags_6 >> 4) | (header.m_flags_7 & 0xF0);

        // Only mapper zero is supported
        if (mapper != 0)
        {
            fprintf(stderr, "Unsupported mapper %d\n", mapper);
            return 1;
        }

        // If there is a trainer, skip 512 bytes
        if (header.m_flags_6 & INES_FLAG_6_TRAINER)
        {
            fseek(file, 512, SEEK_CUR);
        }

        size_t prg_rom_size_in_bytes = header.m_prg_rom_size * 0x4000;
        uint8_t *prg_rom = (uint8_t *)malloc(prg_rom_size_in_bytes);   
        fread(prg_rom, prg_rom_size_in_bytes, 1, file);

        // Create a PRG RAM device, they are usually  2 or 4 KiB and are mirrored to fill the entire 8 KiB range
        // We just create a 8 KiB device
        bus_device_t prg_ram = ram_device_create(0x2000);

        // Map the PRG RAM to the bus at 0x6000
        bus.attach(prg_ram, 0x6000, 0x2000);

        // Create a PRG ROM device for the first 16 KiB of the PRG ROM
        bus_device_t prg_rom_0 = ram_device_create(0x4000);

        // Attach the PRG ROM to the bus at address 0x8000
        bus.attach(prg_rom_0, 0x8000, 0x4000);

        // Copy the first 16 KiB of the PRG ROM to the PRG ROM device
        ram_device_write_buffer(prg_rom_0, 0, prg_rom, 0x4000);

        if (header.m_prg_rom_size > 1)
        {
            // NROM-256

            // Create a PRG ROM device for the second 16 KiB of the PRG ROM
            bus_device_t prg_rom_1 = ram_device_create(0x4000);

            // Attach the PRG ROM to the bus at address 0xA000
            bus.attach(prg_rom_1, 0xC000, 0x4000);

            // Copy the second 16 KiB of the PRG ROM to the PRG ROM device
            ram_device_write_buffer(prg_rom_1, 0, prg_rom + 0x4000, 0x4000);
        }
        else
        {
            // NROM-128

            // Just mirror the first 16 KiB of the PRG ROM to the second 16 KiB
            bus.attach(prg_rom_0, 0xC000, 0x4000);
        }

        free(prg_rom);

        uint8_t *chr_rom = (uint8_t *)malloc(header.m_chr_rom_size * 0x2000);
        fread(chr_rom, header.m_chr_rom_size * 0x2000, 1, file);
        
        // Create a CHR ROM device for pattern table 0
        bus_device_t chr_rom_pattern_table_0 = ram_device_create(0x1000);

        // Attach the CHR ROM to the bus at address 0x0000
        ppu_device_attach(ppu, chr_rom_pattern_table_0, 0x0000, 0x1000);

        // Copy the first 4 KiB of the CHR ROM to the CHR ROM device
        ram_device_write_buffer(chr_rom_pattern_table_0, 0, chr_rom, 0x1000);

        // Create a CHR ROM device for pattern table 1
        bus_device_t chr_rom_pattern_table_1 = ram_device_create(0x1000);

        // Attach the CHR ROM to the bus at address 0x1000
        ppu_device_attach(ppu, chr_rom_pattern_table_1, 0x1000, 0x1000);

        // Copy the last 4 KiB of the CHR ROM to the CHR ROM device
        ram_device_write_buffer(chr_rom_pattern_table_1, 0, chr_rom + 0x1000, 0x1000);

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
            ppu_device_attach(ppu, nametable, (0x2000 + (i * 0x0400)) + 0x0000, 0x0400);
            ppu_device_attach(ppu, nametable, (0x2000 + (i * 0x0400)) + 0x0800, 0x0400);
        }
        
        fclose(file);

        const uint64_t tick_duration_ns = 1000000000 / 21441960; // Adjusted frequency

        struct timespec ts_start, ts_end;

        cpu.power_on(&bus);

        // Run the CPU
        for (unsigned tickcount = 0;; tickcount++)
        {
            clock_gettime(CLOCK_MONOTONIC, &ts_start);

            // PPU divides the master clock by 4
            if ((tickcount % 4) == 0)
            {
                ppu_device_tick(ppu, &cpu);
            }

            // CPU divides the master clock by 12
            if ((tickcount % 12) == 0)
            {
                cpu.tick(&bus);
            }

            clock_gettime(CLOCK_MONOTONIC, &ts_end);

            // Simplified elapsed time calculation
            uint64_t elapsed_ns = (ts_end.tv_sec - ts_start.tv_sec) * 1000000000 + (ts_end.tv_nsec - ts_start.tv_nsec);

            // Sleep for the remaining time to maintain the tick duration
            if (elapsed_ns < tick_duration_ns)
            {
                struct timespec ts_sleep = {
                    .tv_sec = 0,
                    .tv_nsec = (long)(tick_duration_ns - elapsed_ns)
                };
                //nanosleep(&ts_sleep, NULL);
            }
        }

    }
    
    return 0;
}