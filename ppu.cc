#include "ppu.h"
#include "bus.h"

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <assert.h>

// The PPU addresses a 14-bit (16kB) address space, $0000-$3FFF, completely separate from the CPU's address bus.
// It is either directly accessed by the PPU itself, or via the CPU with memory mapped registers at $2006 and $2007.
//
// Address range   Size     Description              Mapped by
// -----------------------------------------------------------------
// $0000-$0FFF     $1000    Pattern table 0          Cartridge
// $1000-$1FFF     $1000    Pattern table 1          Cartridge
// $2000-$23FF     $0400    Nametable 0              Cartridge
// $2400-$27FF     $0400    Nametable 1              Cartridge
// $2800-$2BFF     $0400    Nametable 2              Cartridge
// $2C00-$2FFF     $0400    Nametable 3              Cartridge
// $3000-$3EFF     $0F00    Unused                   Cartridge
// $3F00-$3F1F     $0020    Palette RAM indexes      Internal to PPU
// $3F20-$3FFF     $00E0    Mirrors of $3F00-$3F1F   Internal to PPU

#define DEVICE_TO_PPU(p) ((ppu_device_t)(p))

#define PPU_FETCH_CYCLE(ppu) (((ppu->m_cycle - 1) & 7))

typedef struct ppu_device_data *ppu_device_t;

typedef struct rgb_color_data
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} *rgb_color_t;

union vram_adress
{
    struct
    {
        uint16_t coarse_x : 5;    // Coarse X (tile column)
        uint16_t coarse_y : 5;    // Coarse Y (tile row)
        uint16_t nametable_x : 1; // Nametable X
        uint16_t nametable_y : 1; // Nametable Y
        uint16_t fine_y : 3;      // Fine Y (pixel row within tile)
        uint16_t unused : 1;
    };
    uint16_t raw;
};

struct ppu_device_data
{
    struct bus_device_data m_device;
    struct
    {
        union
        {
            struct
            {
                uint8_t nametable_x : 1;
                uint8_t nametable_y : 1;
                uint8_t increment : 1;
                uint8_t sprite_table : 1;
                uint8_t background_table : 1;
                uint8_t sprite_size : 1;
                uint8_t master_slave : 1;
                uint8_t nmi : 1;
            };
            uint8_t raw;
        } ctrl;

        union
        {
            struct
            {
                uint8_t grayscale : 1;
                uint8_t background_leftmost : 1;
                uint8_t sprites_leftmost : 1;
                uint8_t background : 1;
                uint8_t sprites : 1;
                uint8_t red_emphasis : 1;
                uint8_t green_emphasis : 1;
                uint8_t blue_emphasis : 1;
            };
            uint8_t raw;
        } mask;

        union
        {
            struct
            {
                uint8_t unused : 5;
                uint8_t sprite_overflow : 1;
                uint8_t sprite_zero_hit : 1;
                uint8_t vblank : 1;
            };
            uint8_t raw;
        } status;

        uint8_t oamaddr;
        uint8_t scroll;
    } m_registers;

    // Background rendering registers
    uint8_t bg_next_tile_id; // Current nametable data (8 bits)

    uint8_t bg_next_tile_attrib; // Current attribute table data (8 bits)

    uint8_t bg_next_tile_lsb; // Current pattern table data (low byte) (8 bits)

    uint8_t bg_next_tile_msb; // Current pattern table data (high byte) (8 bits)

    // Shift registers for background
    uint16_t bg_shift_pat_lo; // Pattern table low bits shift register
    uint16_t bg_shift_pat_hi; // Pattern table high bits shift register

    uint16_t bg_shift_at_lo; // Attribute low bits shift register
    uint16_t bg_shift_at_hi; // Attribute high bits shift register

    uint64_t vram_data : 8, // Data read from VRAM
        frame_odd : 1,      // Used for skip cycle on odd frames
        w : 1,              // Used to latch the high byte of the PPU address
        fine_x : 3,         // Fine X scroll (3 bits)
        unused : 51;

    union vram_adress v; // Current VRAM address
    union vram_adress t; // Temporary VRAM address

    uint32_t m_cycle;    // 0-340
    uint32_t m_scanline; // 0-261, 0-239=visible, 240=post, 241-260=vblank, 261=pre

    uint8_t m_oam[0x100]; // OAM data

    uint8_t m_palette[32]; // Palette memory

    struct bus_data m_bus;

    struct rgb_color_data frame[256][240]; // Frame buffer

    SDL_Window *m_window;     // SDL window
    SDL_Renderer *m_renderer; // SDL renderer
};

// Standard NES palette (RGB values)
static struct rgb_color_data const s_nes_palette[64] =
{
    { 84,  84,  84}, {  0,  30, 116}, {  8,  16, 144}, { 48,   0, 136}, { 68,   0, 100}, { 92,   0,  48}, { 84,   4,   0}, { 60,  24,   0},
    { 32,  42,   0}, {  8,  58,   0}, {  0,  64,   0}, {  0,  60,   0}, {  0,  50,  60}, {  0,   0,   0}, {  0,   0,   0}, {  0,   0,   0},
    {152, 150, 152}, {  8,  76, 196}, { 48,  50, 236}, { 92,  30, 228}, {136,  20, 176}, {160,  20, 100}, {152,  34,  32}, {120,  60,   0},
    { 84,  90,   0}, { 40, 114,   0}, {  8, 124,   0}, {  0, 118,  40}, {  0, 102, 120}, {  0,   0,   0}, {  0,   0,   0}, {  0,   0,   0},
    {236, 238, 236}, { 76, 154, 236}, {120, 124, 236}, {176,  98, 236}, {228,  84, 236}, {236,  88, 180}, {236, 106, 100}, {212, 136,  32},
    {160, 170,   0}, {116, 196,   0}, { 76, 208,  32}, { 56, 204, 108}, { 56, 180, 204}, { 60,  60,  60}, {  0,   0,   0}, {  0,   0,   0},
    {236, 238, 236}, {168, 204, 236}, {188, 188, 236}, {212, 178, 236}, {236, 174, 236}, {236, 174, 212}, {236, 180, 176}, {228, 196, 144},
    {204, 210, 120}, {180, 222, 120}, {168, 226, 144}, {152, 226, 180}, {160, 214, 228}, {160, 162, 160}, {  0,   0,   0}, {  0,   0,   0}
};

static void ppu_ctrl_write(ppu_device_t a_ppu, uint8_t a_value)
{
    a_ppu->m_registers.ctrl.raw = a_value;
    a_ppu->t.nametable_x = a_ppu->m_registers.ctrl.nametable_x;
    a_ppu->t.nametable_y = a_ppu->m_registers.ctrl.nametable_y;
}

static void ppu_mask_write(ppu_device_t a_ppu, uint8_t a_value)
{
    a_ppu->m_registers.mask.raw = a_value;
}

static uint8_t ppu_status_read(ppu_device_t a_ppu)
{
    uint8_t status = a_ppu->m_registers.status.raw;

    // Clear VBlank flag
    a_ppu->m_registers.status.vblank = 0;

    a_ppu->w = 0;

    return status;
}

static void ppu_oamaddr_write(ppu_device_t a_ppu, uint8_t a_value)
{
    a_ppu->m_registers.oamaddr = a_value;
}

static uint8_t ppu_oamdata_read(ppu_device_t a_ppu)
{
    return a_ppu->m_oam[a_ppu->m_registers.oamaddr];
}

static void ppu_oamdata_write(ppu_device_t a_ppu, uint8_t a_value)
{
    a_ppu->m_oam[a_ppu->m_registers.oamaddr] = a_value;
}

static void ppu_scroll_write(ppu_device_t a_ppu, uint8_t a_value)
{
    if (a_ppu->w == 0)
    {
        // First write (X scroll)
        a_ppu->fine_x = a_value & 0x7;      // Fine X (pixel column within tile)
        a_ppu->t.coarse_x = (a_value >> 3); // Coarse X (tile column)
        a_ppu->w = 1;
    }
    else
    {
        // Second write (Y scroll)
        a_ppu->t.fine_y = (a_value & 0x7);  // Fine Y (pixel row within tile)
        a_ppu->t.coarse_y = (a_value >> 3); // Coarse Y (tile row)
        a_ppu->w = 0;
    }
}

static void ppu_addr_write(ppu_device_t a_ppu, uint8_t a_value)
{
    if (a_ppu->w == 0)
    {
        // First write (high byte)
        a_ppu->t.raw = ((a_value & 0x3F) << 8) | (a_ppu->t.raw & 0x00FF);
        a_ppu->w = 1;
    }
    else
    {
        // Second write (low byte)
        a_ppu->t.raw = (a_ppu->t.raw & 0xFF00) | a_value;
        a_ppu->v = a_ppu->t;
        a_ppu->w = 0;
    }
}

static uint8_t ppu_data_read(ppu_device_t a_ppu)
{
    uint8_t result = 0;
    // Read new data into buffer
    uint16_t addr = a_ppu->v.raw & 0x3FFF;

    // Special case for palette memory
    if (addr >= 0x3F00 && addr < 0x4000)
    {
        // Palette reads are not buffered
        if ((addr & 0x3F10) == 0x3F10)
        {
            addr &= 0x3F0F;
        }
        result = a_ppu->m_palette[addr & 0x1F];
    }
    else
    {
        // Normal memory read
        // Return buffered data
        result = a_ppu->vram_data;
        a_ppu->vram_data = a_ppu->m_bus.read8(addr);
    }

    a_ppu->v.raw += a_ppu->m_registers.ctrl.increment ? 32 : 1;

    return result;
}

static void ppu_data_write(ppu_device_t a_ppu, uint8_t a_value)
{
    uint16_t addr = a_ppu->v.raw & 0x3FFF;

    // Special case for palette memory
    if (addr >= 0x3F00 && addr < 0x4000)
    {

        // Palette reads are not buffered
        if ((addr & 0x3F10) == 0x3F10)
        {
            addr &= 0x3F0F;
        }

        a_ppu->m_palette[addr & 0x1f] = a_value;
    }
    else
    {
        a_ppu->m_bus.write8(a_ppu->v.raw, a_value);
    }

    a_ppu->v.raw += a_ppu->m_registers.ctrl.increment ? 32 : 1;
}

static void ppu_inc_horizontal(ppu_device_t a_ppu)
{
    if (a_ppu->m_registers.mask.background || a_ppu->m_registers.mask.sprites)
    {
        // Increment coarse X
        if ((a_ppu->v.coarse_x) == 31) // if coarse X == 31
        {
            a_ppu->v.coarse_x = 0;                        // coarse X = 0
            a_ppu->v.nametable_x = ~a_ppu->v.nametable_x; // switch horizontal nametable
        }
        else
        {
            a_ppu->v.coarse_x++; // increment coarse X
        }
    }
}

static void ppu_inc_vertical(ppu_device_t a_ppu)
{
    if (a_ppu->m_registers.mask.background || a_ppu->m_registers.mask.sprites)
    {
        if (a_ppu->v.fine_y < 7)
        {
            a_ppu->v.fine_y++; // increment fine Y
        }
        else
        {
            a_ppu->v.fine_y = 0; // fine Y = 0

                // Increment coarse Y
            if (a_ppu->v.coarse_y == 29)
            {
                a_ppu->v.coarse_y = 0;                        // coarse Y = 0
                a_ppu->v.nametable_y = ~a_ppu->v.nametable_y; // switch vertical nametable
            }
            else if (a_ppu->v.coarse_y == 31)
            {
                a_ppu->v.coarse_y = 0; // coarse Y = 0, nametable unchanged
            }
            else
            {
                a_ppu->v.coarse_y++; // increment coarse Y
            }
        }
    }
}

static void ppu_t_to_v_horizontal(ppu_device_t a_ppu)
{
    if (a_ppu->m_registers.mask.background || a_ppu->m_registers.mask.sprites)
    {
        // Copy horizontal bits from t to v
        a_ppu->v.nametable_x = a_ppu->t.nametable_x;
        a_ppu->v.coarse_x = a_ppu->t.coarse_x;
    }
}

static void ppu_t_to_v_vertical(ppu_device_t a_ppu)
{
    if (a_ppu->m_registers.mask.background || a_ppu->m_registers.mask.sprites)
    {
        // Copy vertical bits from t to v
        a_ppu->v.fine_y = a_ppu->t.fine_y;
        a_ppu->v.nametable_y = a_ppu->t.nametable_y;
        a_ppu->v.coarse_y = a_ppu->t.coarse_y;
    }
}

static void ppu_update_shift_registers(ppu_device_t a_ppu)
{
    if (a_ppu->m_registers.mask.background)
    {
        {
            // Shift background registers
            a_ppu->bg_shift_pat_lo <<= 1;
            a_ppu->bg_shift_pat_hi <<= 1;    
        }

        {
            a_ppu->bg_shift_at_lo <<= 1;
            a_ppu->bg_shift_at_hi <<= 1;
        }


        if (PPU_FETCH_CYCLE(a_ppu) == 7)
        {
            {
                // Update pattern registers
                a_ppu->bg_shift_pat_lo = (a_ppu->bg_shift_pat_lo & 0xFF00) | (a_ppu->bg_next_tile_lsb);
                a_ppu->bg_shift_pat_hi = (a_ppu->bg_shift_pat_hi & 0xFF00) | (a_ppu->bg_next_tile_msb);
            }

            {
                // Update attribute registers                
                a_ppu->bg_shift_at_lo = (a_ppu->bg_shift_at_lo & 0xFF00) | ((a_ppu->bg_next_tile_attrib & 0x01) ? 0xFF : 0);
                a_ppu->bg_shift_at_hi = (a_ppu->bg_shift_at_hi & 0xFF00) | ((a_ppu->bg_next_tile_attrib & 0x02) ? 0xFF : 0);
            }
        }
    }
}

static void ppu_vram_fetch_tick(ppu_device_t a_ppu)
{
    switch (PPU_FETCH_CYCLE(a_ppu))
    {
    case 0:
        // Get the nametable data
        a_ppu->bg_next_tile_id = a_ppu->m_bus.read8(0x2000 | (a_ppu->v.raw & 0x0FFF));
        break;
    case 2:
        // Get the attribute table data
        a_ppu->bg_next_tile_attrib = a_ppu->m_bus.read8(0x23C0 | (a_ppu->v.nametable_y << 11) | ((a_ppu->v.nametable_x << 10) | ((a_ppu->v.coarse_y >> 2) << 3) | (a_ppu->v.coarse_x >> 2)));

        if (a_ppu->v.coarse_y & 0x02)
        {
            a_ppu->bg_next_tile_attrib >>= 4;
        }

        if (a_ppu->v.coarse_x & 0x02)
        {
            a_ppu->bg_next_tile_attrib >>= 2;
        }

        a_ppu->bg_next_tile_attrib &= 0x03; // Mask to 2 bits

        break;
    case 4:
        // Get the pattern table address (Low byte)
        a_ppu->bg_next_tile_lsb = a_ppu->m_bus.read8((a_ppu->m_registers.ctrl.background_table << 12) | (a_ppu->bg_next_tile_id << 4) | (a_ppu->v.fine_y + 0));
        break;
    case 6:
        // Get the pattern table address (high byte)
        a_ppu->bg_next_tile_msb = a_ppu->m_bus.read8((a_ppu->m_registers.ctrl.background_table << 12) | (a_ppu->bg_next_tile_id << 4) | (a_ppu->v.fine_y + 8));
        break;
    case 7:
        if ((a_ppu->m_cycle <= 256) || (a_ppu->m_cycle == 328) || (a_ppu->m_cycle == 336))
        {
            ppu_inc_horizontal(a_ppu);
        }
        break;
    }

    // At end of scanline, increment Y position
    if (a_ppu->m_cycle == 256)
    {
        ppu_inc_vertical(a_ppu);
    }
}

static void ppu_scanline_pre_render(ppu_device_t a_ppu)
{
    // At cycle 1 of pre-render scanline, clear VBlank, sprite overflow, and sprite 0 hit flags
    if (a_ppu->m_cycle == 1)
    {
        a_ppu->m_registers.status.vblank = 0;
        a_ppu->m_registers.status.sprite_overflow = 0;
        a_ppu->m_registers.status.sprite_zero_hit = 0;

    }
    else if (a_ppu->m_cycle <= 256 || (a_ppu->m_cycle >= 321 && a_ppu->m_cycle <= 336))
    {
        ppu_update_shift_registers(a_ppu);
        ppu_vram_fetch_tick(a_ppu);
    }
    else if (a_ppu->m_cycle == 257)
    {
        // At cycle 257, copy horizontal bits from t to v
        ppu_t_to_v_horizontal(a_ppu);
    }
    else if ((a_ppu->m_cycle >= 280) && (a_ppu->m_cycle <= 304))
    {
        // At the end of the pre-render scanline, copy horizontal position from t to v
        ppu_t_to_v_vertical(a_ppu);
    }
    else if (a_ppu->m_cycle == 340) // Toggle frame_odd at the end of the pre-render scanline
    {
        // Toggle the frame_odd flag at the end of the pre-render scanline
        a_ppu->frame_odd = !a_ppu->frame_odd;
    }
}

static void ppu_scanline_visible(ppu_device_t a_ppu)
{
    if (a_ppu->m_cycle <= 256 || (a_ppu->m_cycle >= 321 && a_ppu->m_cycle <= 336))
    {
        ppu_update_shift_registers(a_ppu);
        ppu_vram_fetch_tick(a_ppu);
    }
    else if (a_ppu->m_cycle == 257)
    {
        ppu_t_to_v_horizontal(a_ppu);
    }

    // Check if rendering is enabled
    bool rendering = a_ppu->m_registers.mask.background || a_ppu->m_registers.mask.sprites;

    if (!rendering)
    {
        // No rendering, just increment cycle
        return;
    }

    if (a_ppu->m_cycle <= 256)
    {
        // Determine background pixel
        uint8_t bg_pixel = 0;
        uint8_t bg_palette = 0;

        if (a_ppu->m_registers.mask.background)
        {
            // Get pattern bits
            uint16_t bit_mux = 0x8000 >> a_ppu->fine_x;
            uint8_t pixel_lo = (a_ppu->bg_shift_pat_lo & bit_mux) ? 1 : 0;
            uint8_t pixel_hi = (a_ppu->bg_shift_pat_hi & bit_mux) ? 2 : 0;
            bg_pixel = pixel_hi | pixel_lo;

            // Get palette bits
            uint8_t palette_lo = (a_ppu->bg_shift_at_lo & bit_mux) ? 1 : 0;
            uint8_t palette_hi = (a_ppu->bg_shift_at_hi & bit_mux) ? 2 : 0;
            bg_palette = palette_hi | palette_lo;
        }

        if (!a_ppu->m_registers.mask.background_leftmost && (a_ppu->m_cycle <= 8))
        {
            // Disable background for leftmost 8 pixels
            bg_pixel = 0;
        }

        // (Add sprite pixel determination here)

        // Select final pixel based on priority rules 
        uint8_t final_pixel = bg_pixel;
        uint8_t final_palette = bg_palette;

        // (Add sprite priority logic here)

        // Get color from palette
        uint16_t color_address;
        if (final_pixel == 0)
        {
            // Background color (universal background)
            color_address = 0x3F00;
        }
        else
        {
            // Palette entry
            color_address = 0x3F00 + (final_palette << 2) + final_pixel;
        }

        uint8_t color_value = a_ppu->m_palette[color_address & 0x1F];

        // Convert to RGB using the NES palette
        a_ppu->frame[a_ppu->m_scanline][a_ppu->m_cycle - 1] = s_nes_palette[color_value];
    }
}

static void ppu_scanline_post_render(ppu_device_t a_ppu)
{
    // Only render the frame at the end of the post-render scanline
    if (a_ppu->m_cycle == 1)
    {
        // Clear the renderer
        SDL_SetRenderDrawColor(a_ppu->m_renderer, 0, 0, 0, 255);
        SDL_RenderClear(a_ppu->m_renderer);

        // Draw each pixel from the frame buffer to the SDL renderer
        for (int y = 0; y < 240; y++)
        {
            for (int x = 0; x < 256; x++)
            {
                // Set the color for this pixel
                SDL_SetRenderDrawColor(
                    a_ppu->m_renderer,
                    a_ppu->frame[y][x].r, // Red
                    a_ppu->frame[y][x].g, // Green
                    a_ppu->frame[y][x].b, // Blue
                    255                   // Alpha (opaque)
                );

                // Draw the pixel
                SDL_RenderDrawPoint(a_ppu->m_renderer, x, y);
            }
        }

        // Present the renderer (show the frame)
        SDL_RenderPresent(a_ppu->m_renderer);

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
    }
}

static void ppu_scanline_vblank(ppu_device_t a_ppu, uint32_t *a_nmi_out)
{
    if (a_ppu->m_cycle == 1)
    {
        if (a_ppu->m_scanline == 241)
        {
            // Set VBlank flag
            a_ppu->m_registers.status.vblank = 1;

            // Generate NMI if enabled
            if (a_ppu->m_registers.ctrl.nmi)
            {
                *a_nmi_out |= 1; // Set NMI flag
            }
        }
    }
}

static void ppu_scanline_idle_cycle(ppu_device_t a_ppu)
{
}

static void ppu_scanline(ppu_device_t a_ppu, uint32_t *a_nmi_out)
{
    if (a_ppu->m_scanline < 240) // Render scanlines
    {
        ppu_scanline_visible(a_ppu);
    }
    else if (a_ppu->m_scanline == 240) // Post-render scanline
    {
        ppu_scanline_post_render(a_ppu);
    }
    else if (a_ppu->m_scanline < 261) // Vertical blanking line
    {
        ppu_scanline_vblank(a_ppu, a_nmi_out);
    }
    else
    {
        ppu_scanline_pre_render(a_ppu);
    }
}

void ppu_device_tick(bus_device_t a_ppu_device, uint32_t *a_nmi_out)
{
    ppu_device_t ppu = DEVICE_TO_PPU(a_ppu_device);

    // Skip cycle 0 on scanline 0 of odd frames when rendering is enabled
    bool skip_cycle = (ppu->m_scanline == 0 && ppu->m_cycle == 0 && ppu->frame_odd && (ppu->m_registers.mask.background || ppu->m_registers.mask.sprites));

    if (skip_cycle)
    {
        // Skip directly to cycle 1
        ppu->m_cycle = 1;
    }

    if (ppu->m_cycle == 0)
    {
        // Idle cycle
        ppu_scanline_idle_cycle(ppu);
    }
    else
    {
        ppu_scanline(ppu, a_nmi_out);
    }

    if (ppu->m_cycle < 341)
    {
        // Increment the cycle
        ppu->m_cycle++;
    }
    else
    {
        // End of the scanline
        ppu->m_cycle = 0;
        ppu->m_scanline = (ppu->m_scanline + 1) % 262;
    }
}

void ppu_device_attach(bus_device_t a_ppu_device, bus_device_t a_bus_device, uint16_t a_base, uint32_t a_size)
{
    ppu_device_t ppu = DEVICE_TO_PPU(a_ppu_device);
    ppu->m_bus.attach(a_bus_device, a_base, a_size);
}

static uint8_t ppu_read8(bus_device_t a_dev, uint16_t a_addr)
{
    switch (a_addr & 0x7)
    {
    case 2: // PPUSTATUS
        return ppu_status_read(DEVICE_TO_PPU(a_dev));
    case 4: // OAMDATA
        return ppu_oamdata_read(DEVICE_TO_PPU(a_dev));
    case 7: // PPUDATA
        return ppu_data_read(DEVICE_TO_PPU(a_dev));
    }
    assert(0); // For debugging
}

static void ppu_write8(bus_device_t a_dev, uint16_t a_addr, uint8_t a_value)
{
    switch (a_addr & 0x7)
    {
    case 0: // PPUCTRL
        ppu_ctrl_write(DEVICE_TO_PPU(a_dev), a_value);
        break;
    case 1: // PPUMASK
        ppu_mask_write(DEVICE_TO_PPU(a_dev), a_value);
        break;
    case 3: // OAMADDR
        ppu_oamaddr_write(DEVICE_TO_PPU(a_dev), a_value);
        break;
    case 4: // OAMDATA
        ppu_oamdata_write(DEVICE_TO_PPU(a_dev), a_value);
        break;
    case 5: // PPUSCROLL
        ppu_scroll_write(DEVICE_TO_PPU(a_dev), a_value);
        break;
    case 6: // PPUADDR
        ppu_addr_write(DEVICE_TO_PPU(a_dev), a_value);
        break;
    case 7: // PPUDATA
        ppu_data_write(DEVICE_TO_PPU(a_dev), a_value);
        break;
    default:
        assert(0); // For debugging
    }
}

static struct bus_device_ops_data g_ppu_ops =
{
        .read8 = ppu_read8,
        .write8 = ppu_write8
};

bus_device_t ppu_device_create()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) // Initialize SDL
    {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return NULL;
    }

    ppu_device_t ppu = (ppu_device_t)malloc(sizeof(ppu_device_data));
    *ppu = {};
    ppu->m_device.m_ops = &g_ppu_ops;

    ppu->m_bus.initialize();

    // Create SDL window
    ppu->m_window = SDL_CreateWindow("PPU Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 256, 240, SDL_WINDOW_SHOWN);
    if (!ppu->m_window)
    {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        free(ppu);
        SDL_Quit();
        return NULL;
    }

    // Create SDL renderer
    ppu->m_renderer = SDL_CreateRenderer(ppu->m_window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!ppu->m_renderer)
    {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(ppu->m_window);
        free(ppu);
        SDL_Quit();
        return NULL;
    }

    return &ppu->m_device;
}

void ppu_device_destroy(bus_device_t a_ppu_device)
{
    ppu_device_t ppu = DEVICE_TO_PPU(a_ppu_device);

    if (ppu->m_renderer)
    {
        // Destroy SDL renderer
        SDL_DestroyRenderer(ppu->m_renderer);
    }

    if (ppu->m_window)
    {
        // Destroy SDL window
        SDL_DestroyWindow(ppu->m_window);
    }

    free(ppu);

    // Quit SDL
    SDL_Quit();
}