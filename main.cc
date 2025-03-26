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
    "01-implied.nes"
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
        for (int i = 0; i < 0x4000; i++)
        {
            bus.write8(&cpu, 0x8000 + i, prg_rom[i]);
        }

        // Create a PRG ROM device for the last 16 KiB of the PRG ROM
        bus_device_t prg_rom_1 = ram_device_create(0x4000);

        // Attach the PRG ROM to the bus at address 0xC000
        bus.attach(prg_rom_1, 0xC000, 0x4000);

        // Copy the last 16 KiB of the PRG ROM to the PRG ROM device
        for (int i = 0; i < 0x4000; i++)
        {
            bus.write8(&cpu, 0xC000 + i, prg_rom[0x4000 + i]);
        }

        free(prg_rom);
        
        fclose(file);

        cpu.power_on(&bus);

        // Run the CPU
        for (unsigned tickcount = 0;;tickcount++)
        {
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
        }

    }
    
    #if notnow
    // First instruction is at 0x600
    bus.write16(&cpu, 0x0FFC, 0x600);
    static uint8_t const sample_code[] = {
        /* 0600: */ 0x20, 0x06, 0x06, 0x20, 0x38, 0x06, 0x20, 0x0d, 0x06, 0x20, 0x2a, 0x06, 0x60, 0xa9, 0x02, 0x85, 
        /* 0610: */ 0x02, 0xa9, 0x04, 0x85, 0x03, 0xa9, 0x11, 0x85, 0x10, 0xa9, 0x10, 0x85, 0x12, 0xa9, 0x0f, 0x85, 
        /* 0620: */ 0x14, 0xa9, 0x04, 0x85, 0x11, 0x85, 0x13, 0x85, 0x15, 0x60, 0xa5, 0xfe, 0x85, 0x00, 0xa5, 0xfe, 
        /* 0630: */ 0x29, 0x03, 0x18, 0x69, 0x02, 0x85, 0x01, 0x60, 0x20, 0x4d, 0x06, 0x20, 0x8d, 0x06, 0x20, 0xc3, 
        /* 0640: */ 0x06, 0x20, 0x19, 0x07, 0x20, 0x20, 0x07, 0x20, 0x2d, 0x07, 0x4c, 0x38, 0x06, 0xa5, 0xff, 0xc9, 
        /* 0650: */ 0x77, 0xf0, 0x0d, 0xc9, 0x64, 0xf0, 0x14, 0xc9, 0x73, 0xf0, 0x1b, 0xc9, 0x61, 0xf0, 0x22, 0x60, 
        /* 0660: */ 0xa9, 0x04, 0x24, 0x02, 0xd0, 0x26, 0xa9, 0x01, 0x85, 0x02, 0x60, 0xa9, 0x08, 0x24, 0x02, 0xd0, 
        /* 0670: */ 0x1b, 0xa9, 0x02, 0x85, 0x02, 0x60, 0xa9, 0x01, 0x24, 0x02, 0xd0, 0x10, 0xa9, 0x04, 0x85, 0x02, 
        /* 0680: */ 0x60, 0xa9, 0x02, 0x24, 0x02, 0xd0, 0x05, 0xa9, 0x08, 0x85, 0x02, 0x60, 0x60, 0x20, 0x94, 0x06, 
        /* 0690: */ 0x20, 0xa8, 0x06, 0x60, 0xa5, 0x00, 0xc5, 0x10, 0xd0, 0x0d, 0xa5, 0x01, 0xc5, 0x11, 0xd0, 0x07, 
        /* 06a0: */ 0xe6, 0x03, 0xe6, 0x03, 0x20, 0x2a, 0x06, 0x60, 0xa2, 0x02, 0xb5, 0x10, 0xc5, 0x10, 0xd0, 0x06, 
        /* 06b0: */ 0xb5, 0x11, 0xc5, 0x11, 0xf0, 0x09, 0xe8, 0xe8, 0xe4, 0x03, 0xf0, 0x06, 0x4c, 0xaa, 0x06, 0x4c, 
        /* 06c0: */ 0x35, 0x07, 0x60, 0xa6, 0x03, 0xca, 0x8a, 0xb5, 0x10, 0x95, 0x12, 0xca, 0x10, 0xf9, 0xa5, 0x02, 
        /* 06d0: */ 0x4a, 0xb0, 0x09, 0x4a, 0xb0, 0x19, 0x4a, 0xb0, 0x1f, 0x4a, 0xb0, 0x2f, 0xa5, 0x10, 0x38, 0xe9, 
        /* 06e0: */ 0x20, 0x85, 0x10, 0x90, 0x01, 0x60, 0xc6, 0x11, 0xa9, 0x01, 0xc5, 0x11, 0xf0, 0x28, 0x60, 0xe6, 
        /* 06f0: */ 0x10, 0xa9, 0x1f, 0x24, 0x10, 0xf0, 0x1f, 0x60, 0xa5, 0x10, 0x18, 0x69, 0x20, 0x85, 0x10, 0xb0, 
        /* 0700: */ 0x01, 0x60, 0xe6, 0x11, 0xa9, 0x06, 0xc5, 0x11, 0xf0, 0x0c, 0x60, 0xc6, 0x10, 0xa5, 0x10, 0x29, 
        /* 0710: */ 0x1f, 0xc9, 0x1f, 0xf0, 0x01, 0x60, 0x4c, 0x35, 0x07, 0xa0, 0x00, 0xa5, 0xfe, 0x91, 0x00, 0x60, 
        /* 0720: */ 0xa6, 0x03, 0xa9, 0x00, 0x81, 0x10, 0xa2, 0x00, 0xa9, 0x01, 0x81, 0x10, 0x60, 0xa2, 0x00, 0xea, 
        /* 0730: */ 0xea, 0xca, 0xd0, 0xfb, 0x60, 0x00
    };

    for (int i = 0; i < sizeof(sample_code); i++)
    {
        bus.write8(&cpu, 0x600 + i, sample_code[i]);
    }

    cpu.power_on(&bus);

    initscr();

    if (has_colors())
    {
        start_color();
        init_pair(COLOR_RED, COLOR_BLACK, COLOR_RED);
    }
    else
    {
        printw("Your terminal does not support color\n");
        refresh();
        endwin();
        return 1;        
    }
    
    start_color();

    noecho();
    curs_set(0);
    timeout(0);

    for (int i = 0; i < 8; i++)
    {
        init_pair(i, COLOR_BLACK, i);
    }

    // Get current timestamp in milliseconds, we need this value to refresh the screen every 16ms. Use utimes() on Linux.
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t last_time = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;


    for(uint32_t cycle = 0;; cycle++)
    {
        // If a key is pressed write it's ASCII value to the memory address 0xFF. We don't wait for a key press, so this is non-blocking.
        bus.write8(&cpu, 0xFF, getch());

        // Write a random value to 0xFE
        bus.write8(&cpu, 0xFE, rand() % 256);
        
        // Clock the CPU
        cpu.tick(&bus);

        // Get the current timestamp in milliseconds
        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t current_time = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

        // If 16ms have passed since the last screen refresh, refresh the screen
        if ((current_time - last_time)  < 30)
        {
            continue;
        }

        last_time = current_time;

        // Draw a 32x32 grid of the memory contents at 0x200 in the RAM
        // Each cell in the grid is a whitespace character with the background color set to the value of the byte in memory
        for (int y = 0; y < 32; y++)
        {
            for (int x = 0; x < 32; x++)
            {
                uint8_t value = bus.read8(&cpu, 0x200 + y * 32 + x);
                attron(COLOR_PAIR(value));
                mvaddch(y, x, ' ');
                attroff(COLOR_PAIR(value));
            }
        }

        // On the right side of the grid, draw the CPU registers
        mvprintw(0, 32, "A: %02X", cpu.m_registers.a);
        mvprintw(1, 32, "X: %02X", cpu.m_registers.x);
        mvprintw(2, 32, "Y: %02X", cpu.m_registers.y);
        mvprintw(3, 32, "S: %02X", cpu.m_registers.s);
        mvprintw(4, 32, "PC: %04X", cpu.m_registers.pc);
        
        // Draw the status register flags in the format NV-BDIZC with the bits under the letters
        mvprintw(5, 32, "NV-BDIZC");
        for (int i = 0; i < 8; i++)
        {
            mvaddch(6, 32 + i, (cpu.m_registers.status.raw & (1 << (7 - i))) ? '1' : '0');
        }
        // Also the cycle count
        mvprintw(7, 32, "Cycle: %u", cycle);
        refresh();

        //usleep(100);
    }

    endwin();
 
#endif
    
    return 0;
}