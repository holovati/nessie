#include <malloc.h>
#include <string.h>
#include <stddef.h>

#define MAPPER_IMPL
#include "ram_device.h"
#include "bus.h"
#include "ppu.h"
#include "mapper.h"

#define PRG_ROM_DEVICE_TO_MMC1(p) ((mmc1_t)(((char *)p) - offsetof(struct mmc1_data, m_prg_rom_device)))
#define PPU_CHR_DEVICE_TO_MMC1(p) ((mmc1_t)(((char *)p) - offsetof(struct mmc1_data, m_ppu_chr_device)))
#define MEMORY_BANK(sz) { uint8_t at[sz * 1024]; }

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
| CPU $6000-$7FFF   | PRG RAM                       | 8 KiB of PRG RAM, typically battery-backed                                                                       |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| CPU $8000-$FFFF   | PRG ROM                       | Bank-switchable PRG ROM. MMC1 supports 16 KiB or 32 KiB banks.                                                   |
|                   |                               | - $8000-$BFFF: Switchable bank (16 KiB mode) or fixed bank (32 KiB mode).                                        |
|                   |                               | - $C000-$FFFF: Fixed bank (16 KiB mode) or mirror of $8000-$BFFF (32 KiB mode).                                  |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| PPU $0000-$0FFF   | CHR ROM/RAM                   | Bank-switchable CHR ROM/RAM. MMC1 supports 4 KiB or 8 KiB banks.                                                 |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| PPU $1000-$1FFF   | CHR ROM/RAM                   | Bank-switchable CHR ROM/RAM. MMC1 supports 4 KiB or 8 KiB banks.                                                 |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| PPU $2000-$2FFF   | Nametables                    | 2 KiB of VRAM for nametables. MMC1 supports horizontal, vertical, single-screen, or four-screen mirroring.       |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| PPU $3000-$3EFF   | Mirrors of $2000-$2FFF        | Mirrored nametables                                                                                              |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
| PPU $3F00-$3FFF   | Palette RAM                   | 32 bytes of palette RAM, mirrored every 32 bytes                                                                 |
+-------------------+-------------------------------+------------------------------------------------------------------------------------------------------------------+
*/

typedef struct mmc1_data
{
    struct bus_device_data m_prg_rom_device;
    struct bus_device_data m_ppu_chr_device;

    bus_device_t m_prg_ram; // 8 KiB of PRG RAM
    
    struct MEMORY_BANK(16) m_prg_rom[16]; // 256 KiB of PRG ROM (16x16K)
    struct MEMORY_BANK(4)  m_chr_ram[32]; // 128 KiB of CHR RAM (32x4K)
    
    uint8_t m_prg_rom_16k_banks;

    union 
    {
        struct 
        {
            uint8_t shift_register : 5; // Shift register for loading data
            uint8_t counter : 3;
        };
        uint8_t raw;
    } m_load_register; // $8000-$FFFF

    union 
    {
        struct 
        {
            uint8_t nametable_arrangement : 2; // 0: one-screen, lower bank, 1: one-screen, upper bank, 2: horizontal arrangement ("vertical mirroring", PPU A10), 3: vertical arrangement ("horizontal mirroring", PPU A11)
            uint8_t prg_rom_bank_mode : 2;     // 0,1: switch 32 KB at $8000, ignoring low bit of bank number; 2: fix first bank at $8000 and switch 16 KB bank at $C000, 3: fix last bank at $C000 and switch 16 KB bank at $8000)
            uint8_t chr_rom_bank_mode : 1;     // 0: switch 8 KB at a time; 1: switch two separate 4 KB banks
            uint8_t unused : 3;             
        };
        uint8_t raw;
    } m_control_register; // $8000-$9FFF

    uint8_t m_chr_bank0_register; // $A000-$BFFF Select 4 KB or 8 KB CHR bank at PPU $0000 (low bit ignored in 8 KB mode)
    uint8_t m_chr_bank1_register; // $C000-$DFFF Select 4 KB or 8 KB CHR bank at PPU $1000 (low bit ignored in 8 KB mode)

    union
    {
        struct 
        {
            uint8_t prg_bank : 4; // SSelect 16 KB PRG-ROM bank (low bit ignored in 32 KB mode)
            uint8_t r : 1; // MMC1B and later: PRG-RAM chip enable (0: enabled; 1: disabled; ignored on MMC1A) MMC1A: Bit 3 bypasses fixed bank logic in 16K mode (0: fixed bank affects A17-A14; 1: fixed bank affects A16-A14 and bit 3 directly controls A17)
        };
        uint8_t raw;
    } m_prg_bank_register; // $E000-$FFFF

} *mmc1_t;

static uint8_t mmc1_prg_rom_read8(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr)
{
    mmc1_t mmc1 = PRG_ROM_DEVICE_TO_MMC1(a_dev);

    uint8_t bank = 0;
    uint16_t mask = 0x3FFF;

    switch (mmc1->m_control_register.prg_rom_bank_mode & 3)
    {
        case 0:
        case 1: // switch 32 KB at $8000
            bank = (mmc1->m_prg_bank_register.prg_bank & 0xF) & ~0x1;
            mask = 0x7FFF;
        break;
    
        case 2: // fix first bank at $8000 and switch 16 KB bank at $C000
            // Switch bank at 0xC000, if below bank is already set to 0
            if (a_addr >= 0x4000)
            {
                bank = mmc1->m_prg_bank_register.prg_bank & 0xF; 
            }
        break;
        case 3: // fix last bank at $C000 and switch 16 KB bank at $8000    
            if (a_addr < 0x4000)
            {
                bank = mmc1->m_prg_bank_register.prg_bank & 0xF; 
            }
            else
            {
                bank = mmc1->m_prg_rom_16k_banks - 1;
            }
    }

    uint8_t retval = mmc1->m_prg_rom[bank].at[a_addr & mask];

    return retval;
}

static uint16_t mmc1_prg_rom_read16(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr)
{
    asm("int3");
    return 0;
}

static void mmc1_prg_rom_write8(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr, uint8_t a_value)
{
    mmc1_t mmc1 = PRG_ROM_DEVICE_TO_MMC1(a_dev);

    if (a_value >> 7)
    {
        // Clear to initial state
        mmc1->m_load_register.raw = 0;
        
        mmc1->m_control_register.raw = 0x0C;
        mmc1->m_prg_bank_register.raw = 0;

        // Reset CHR bank registers too
        mmc1->m_chr_bank0_register = 0;
        mmc1->m_chr_bank1_register = 0;        
        return;
    }

    mmc1->m_load_register.shift_register |= (a_value & 0x01) << mmc1->m_load_register.counter;
        
    if (++mmc1->m_load_register.counter < 5)
    {
        return;
    }

    switch ((a_addr >> 13) & 3)
    {
        case 0: // 0x8000 - 0x9FFF -- Control register
            mmc1->m_control_register.raw = mmc1->m_load_register.shift_register;
        break;
        case 1: // 0xA000 - 0xBFFF -- CHR bank 0 register
            mmc1->m_chr_bank0_register = mmc1->m_load_register.shift_register;
        break;
        case 2: // 0xC000 - 0xDFFF -- CHR bank 1 register
            mmc1->m_chr_bank1_register = mmc1->m_load_register.shift_register;
        break;
        case 3: // 0xE000 - 0xFFFF -- PRG bank register
            mmc1->m_prg_bank_register.raw = mmc1->m_load_register.shift_register;
        break;
    }

    mmc1->m_load_register.raw = 0;
}

static void mmc1_prg_rom_write16(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr, uint16_t a_value)
{
    asm("int3");
}

static struct bus_device_ops_data s_prg_rom_ops =
{
    .read8 = mmc1_prg_rom_read8,
    .read16 = mmc1_prg_rom_read16,
    .write8 = mmc1_prg_rom_write8,
    .write16 = mmc1_prg_rom_write16,
};

static uint8_t mmc1_ppu_pt0_read8(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr)
{
    mmc1_t mmc1 = PPU_CHR_DEVICE_TO_MMC1(a_dev);

    uint8_t retval = 0;

    if (mmc1->m_control_register.chr_rom_bank_mode == 0) // 8 KB mode
    {
        retval = mmc1->m_chr_ram[mmc1->m_chr_bank0_register & ~0x01].at[a_addr & 0x1FFF];
    }
    else
    {
        // 4 KB mode
        if (a_addr < 0x1000)
        {
            retval = mmc1->m_chr_ram[mmc1->m_chr_bank0_register].at[a_addr & 0xFFF];
        }
        else
        {
            retval = mmc1->m_chr_ram[mmc1->m_chr_bank1_register].at[a_addr & 0xFFF];
        }        
    }

    return retval;
}

static uint16_t mmc1_ppu_pt0_read16(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr)
{
    asm("int3");
    return 0;
}

static void mmc1_ppu_pt0_write8(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr, uint8_t a_value)
{
    mmc1_t mmc1 = PPU_CHR_DEVICE_TO_MMC1(a_dev);

    if (mmc1->m_control_register.chr_rom_bank_mode == 0) // 8 KB mode
    {
        // Low bit ignored in 8 KB mode
        mmc1->m_chr_ram[mmc1->m_chr_bank0_register & ~0x01].at[a_addr & 0x1FFF] = a_value;
    }
    else // 4 KB mode
    {
        if (a_addr < 0x1000)
        {
            mmc1->m_chr_ram[mmc1->m_chr_bank0_register].at[a_addr & 0xFFF] = a_value;
        }
        else
        {
            mmc1->m_chr_ram[mmc1->m_chr_bank1_register].at[a_addr & 0xFFF] = a_value;
        }
    }    
}

static void mmc1_ppu_pt0_write16(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr, uint16_t a_value)
{
    asm("int3");
}

static struct bus_device_ops_data s_ppu_pt0_ops =
{
    .read8 = mmc1_ppu_pt0_read8,
    .read16 = mmc1_ppu_pt0_read16,
    .write8 = mmc1_ppu_pt0_write8,
    .write16 = mmc1_ppu_pt0_write16,
};

mapper_return_t MMC1_probe_ines(ines_header_t a_ines_hdr)
{
    if ((a_ines_hdr->m_prg_rom_size == 0) || (a_ines_hdr->m_prg_rom_size > (256 * 1024)))
    {
        return MAPPER_INES_VALUE_INVALID;
    }

    if (a_ines_hdr->m_chr_rom_size > (128 * 1024))
    {
        return MAPPER_INES_VALUE_INVALID;
    }

    return MAPPER_OK;
}

mapper_return_t MMC1_map_ines(ines_header_t a_ines_hdr, bus_t a_bus, bus_device_t a_ppu)
{
    uint8_t *ines_file = (uint8_t *)&a_ines_hdr[1];

    // If there is a trainer, skip 512 bytes
    if (a_ines_hdr->m_flags_6 & INES_FLAG_6_TRAINER)
    {
        ines_file += 512;
    }

    mmc1_t mmc1 = (mmc1_t)malloc(sizeof(struct mmc1_data));

    *mmc1 = {};

    mmc1->m_control_register.prg_rom_bank_mode = 3; // Fix last bank at $C000 and switch 16 KB bank at $8000

    memset(&mmc1->m_prg_rom, 0xF2, sizeof(mmc1->m_prg_rom)); // Fill with 0xF2, which is a JAM instruction(Good for debugging)

    // Create a PRG RAM device, they are usually  2 or 4 KiB and are mirrored to fill the entire 8 KiB range. We just create a 8 KiB device, no mirroring
    mmc1->m_prg_ram = ram_device_create(0x2000);
    
    // Map the PRG RAM to the bus at 0x6000
    a_bus->attach(mmc1->m_prg_ram, 0x6000, 0x2000);

    mmc1->m_prg_rom_16k_banks = a_ines_hdr->m_prg_rom_size;
    size_t prg_rom_size_in_bytes = mmc1->m_prg_rom_16k_banks * 0x4000;
    memcpy(&mmc1->m_prg_rom, ines_file, prg_rom_size_in_bytes);

    // Create a PRG ROM device and map over the whole 32 KiB range
    mmc1->m_prg_rom_device = {};
    mmc1->m_prg_rom_device.m_ops = &s_prg_rom_ops;
    a_bus->attach(&mmc1->m_prg_rom_device, 0x8000, 0x8000);

    if (a_ines_hdr->m_chr_rom_size > 0)
    {
        memcpy(&mmc1->m_chr_ram, ines_file + prg_rom_size_in_bytes, (a_ines_hdr->m_chr_rom_size * 0x2000));
    }

    mmc1->m_ppu_chr_device = {};
    mmc1->m_ppu_chr_device.m_ops = &s_ppu_pt0_ops;
    ppu_device_attach(a_ppu, &mmc1->m_ppu_chr_device, 0x0000, 0x2000);

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