#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <utime.h>
#include <time.h>

#include "cpu.h"
#include "bus.h"
#include "ppu.h"
#include "ram_device.h"
#include "mapper.h"

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
    "test_roms/cpu.nes", // Passes all tests
    "test_roms/all_instrs.nes", // Passes all tests
    "test_roms/pacman.nes", // Shows the title screen, but does not work correctly
    "test_roms/full_palette.nes", // Does not work
    "test_roms/window2_ntsc.nes", // Works, but does not render correctly
    "test_roms/nes15-NTSC.nes", // Works, but color palette is wrong
};

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0); // Disable buffering for stdout

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
    for (size_t test_idx = 0; test_idx < sizeof(s_test_rom_files) / sizeof(s_test_rom_files[0]); test_idx++)
    {
        FILE *file = fopen(s_test_rom_files[test_idx], "rb");
        
        if (!file)
        {
            fprintf(stderr, "Failed to open file %s\n", s_test_rom_files[test_idx]);
            return 1;
        }

        size_t file_size = 0;

        fseek(file, 0, SEEK_END);

        file_size = ftell(file);

        fseek(file, 0, SEEK_SET);
        
        ines_header_t ines_file = (ines_header_t)malloc(file_size);

        if (!ines_file)
        {
            fprintf(stderr, "Failed to allocate memory for file buffer\n");
            fclose(file);
            return 1;
        }

        fread(ines_file, file_size, 1, file);
        
        fclose(file);

        mapper_return_t mapper_ret = mapper_map_ines(ines_file, &bus, ppu);

        free(ines_file);

        if (mapper_ret != MAPPER_OK)
        {
            fprintf(stderr, "Unsupported mapper\n");
            return 1;
        }

        const uint64_t tick_duration_ns = 1000000000 / 21441960; // Adjusted frequency

        struct timespec ts_start, ts_end;

        cpu.power_on(&bus);

        uint32_t nmi = 0;

        // Run the CPU
        for (unsigned tickcount = 0;; tickcount++)
        {
            clock_gettime(CLOCK_MONOTONIC, &ts_start);

            // PPU divides the master clock by 4
            if ((tickcount % 4) == 0)
            {
                ppu_device_tick(ppu, &nmi);
            }

            // CPU divides the master clock by 12
            if ((tickcount % 12) == 0)
            {
                if (nmi)
                {
                    cpu.nmi();
                    nmi = 0;
                }

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