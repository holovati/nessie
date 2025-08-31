#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <utime.h>
#include <time.h>

#ifndef __emerixx__
#include <SDL2/SDL.h>
#endif

#include "apu.h"
#include "cpu.h"
#include "bus.h"
#include "ppu.h"
#include "ram_device.h"
#include "mapper.h"
#include <sched.h>

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
    "test_roms/power_up_palette.nes", //?
    "test_roms/palette_ram.nes", //OK
    "test_roms/af.nes", // ?
    "test_roms/Castlevania.nes", // unsupported mapper
    "test_roms/punchout.nes", // unsupported mapper
    "test_roms/t2.nes", // unsupported mapper
    "test_roms/paperboy.nes", // unsupported mapper
    "test_roms/solstice.nes", // unsupported mapper
    "test_roms/stropics.nes", // unsupported mapper
    "test_roms/dd.nes", // double dragon
    "test_roms/colorwin_ntsc.nes", // ?
    "test_roms/nes15-NTSC.nes", // Works, but color palette is wrong
    "test_roms/window2_ntsc.nes", // Works, but does not render correctly
    "test_roms/full_palette.nes", // Does not work
    "test_roms/smb.nes", // ?
    "test_roms/pacman.nes", // Works 
    "test_roms/cpu.nes", // Passes all tests
    "test_roms/all_instrs.nes", // Passes all tests
};

#define NES_FRAME_WIDTH 256
#define NES_FRAME_HEIGHT 240
#define NES_FRAME_BORDER 3
#define NES_SCALE_FACTOR 3

static void ppu_frame_render(ppu_rgb_color_t a_frame, void *a_context)
{
#ifndef __emerixx__

    // Cast the context to SDL_Renderer
    SDL_Renderer *renderer = (SDL_Renderer *)a_context;

    // Draw a red rectangle around the NES frame with a thickness of 3 pixels
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Set color to red
    // Draw a 3 pixel border around the nes frame
    SDL_Rect r = {0,
                  0,
                  (NES_FRAME_WIDTH * NES_SCALE_FACTOR) + (NES_FRAME_BORDER * NES_SCALE_FACTOR) * 2,
                  (NES_FRAME_HEIGHT * NES_SCALE_FACTOR) + (NES_FRAME_BORDER * NES_SCALE_FACTOR) * 2};
                    
    SDL_RenderDrawRect(renderer, &r); // Draw the border

    // Draw each pixel from the frame buffer to the SDL renderer
    for (int y = 0; y < NES_FRAME_HEIGHT; y++)
    {
        for (int x = 0; x < NES_FRAME_WIDTH; x++)
        {
            // Set the color for this pixel
            ppu_rgb_color_t pixel = &a_frame[(y * NES_FRAME_WIDTH) + x];
            SDL_SetRenderDrawColor(renderer, pixel->r, pixel->g, pixel->b, 255);
            // Draw a scaled rectangle for each pixel
            SDL_Rect rect = {(x * NES_SCALE_FACTOR) + (NES_FRAME_BORDER * NES_SCALE_FACTOR),
                             (y * NES_SCALE_FACTOR) + (NES_FRAME_BORDER * NES_SCALE_FACTOR),
                             NES_SCALE_FACTOR,
                             NES_SCALE_FACTOR};

            SDL_RenderFillRect(renderer, &rect);
        }
    }


    // Present the renderer (show the frame)
    SDL_RenderPresent(renderer);

    // Process any pending SDL events to keep the window responsive
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
        {
            // Handle quit event if needed
            // You could set a flag to signal the emulator to shut down
        }
    }
#endif
}

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0); // Disable buffering for stdout
#ifndef __emerixx__
    if (SDL_Init(SDL_INIT_VIDEO) != 0) // Initialize SDL
    {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window;     // SDL window

    SDL_Renderer *renderer; // SDL renderer

    // Create SDL window
    window = SDL_CreateWindow("Nessie", 
                               SDL_WINDOWPOS_CENTERED, 
                               SDL_WINDOWPOS_CENTERED, 
                               (NES_FRAME_WIDTH  * NES_SCALE_FACTOR) + (((NES_FRAME_BORDER * 2) * NES_SCALE_FACTOR)), 
                               (NES_FRAME_HEIGHT * NES_SCALE_FACTOR) + (((NES_FRAME_BORDER * 2) * NES_SCALE_FACTOR)), 
                               SDL_WINDOW_SHOWN);
    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create SDL renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

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

    // Attach the APU to the bus at address 0x4000.
    // The APU registers are mirrored every 8 bytes from 0x4000 to 0x4017. 
    // We mirror the APU registers to 0x4018-0x4100.
    bus_device_t apu = apu_device_create();
    bus.attach(apu, 0x4000, 0x100);

    struct apu_device_tick_state_data apu_tick_state = {};
    
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
                ppu_device_tick(ppu, ppu_frame_render, renderer, &nmi);
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
                
                apu_device_tick(apu, &bus, &apu_tick_state);
                
                if (apu_tick_state.out.poll_joypad)
                {
                    const Uint8 *state = SDL_GetKeyboardState(NULL);
                    apu_tick_state.in.joypad1.button.select = state[SDL_SCANCODE_S]; // Set joypad 1 to all buttons released
                    apu_tick_state.in.joypad1.button.start = state[SDL_SCANCODE_RETURN];
                    apu_tick_state.in.joypad1.button.up = state[SDL_SCANCODE_UP];
                    apu_tick_state.in.joypad1.button.down = state[SDL_SCANCODE_DOWN];
                    apu_tick_state.in.joypad1.button.left = state[SDL_SCANCODE_LEFT];
                    apu_tick_state.in.joypad1.button.right = state[SDL_SCANCODE_RIGHT];
                    apu_tick_state.in.joypad1.button.a = state[SDL_SCANCODE_Z];
                    apu_tick_state.in.joypad1.button.b = state[SDL_SCANCODE_X];
                }

                if (apu_tick_state.out.oam_dma)
                {
                    // Handle OAM DMA transfer
                    apu_tick_state.out.oam_dma = 0;
                    // Stall the CPU for 513/514 cycles, the actual "DMA" transfer will be performed in the APU
                    cpu.stall(cpu.m_tickcount & 1 ? 513 : 514); 
                }

            }

            clock_gettime(CLOCK_MONOTONIC, &ts_end);

            // Simplified elapsed time calculation
            uint64_t elapsed_ns =  (ts_end.tv_nsec - ts_start.tv_nsec);

            // Sleep for the remaining time to maintain the tick duration
            if (elapsed_ns < tick_duration_ns)
            {
                struct timespec ts_sleep = {
                    .tv_sec = 0,
                    .tv_nsec = (long)(tick_duration_ns - elapsed_ns)
                };
                // Sleep for the remaining time
                // nanosleep(&ts_sleep, NULL);
                sched_yield();
            }
        }
    }

    if (renderer)
    {
        // Destroy SDL renderer
        SDL_DestroyRenderer(renderer);
    }

    if (window)
    {
        // Destroy SDL window
        SDL_DestroyWindow(window);
    }

    // Quit SDL
    SDL_Quit();
#endif
    return 0;
}
