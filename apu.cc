#include <malloc.h>
#include <assert.h>
#include <stdio.h>

#include "apu.h"
#include "bus.h"

#define REG_SQ1_VOL    0x00 // Square Wave 1 Volume/Envelope Control
#define REG_SQ1_SWEEP  0x01 // Square Wave 1 Sweep Unit Control
#define REG_SQ1_LO     0x02 // Square Wave 1 Timer Low Byte
#define REG_SQ1_HI     0x03 // Square Wave 1 Timer High Byte and Length Counter Load
#define REG_SQ2_VOL    0x04 // Square Wave 2 Volume/Envelope Control
#define REG_SQ2_SWEEP  0x05 // Square Wave 2 Sweep Unit Control
#define REG_SQ2_LO     0x06 // Square Wave 2 Timer Low Byte
#define REG_SQ2_HI     0x07 // Square Wave 2 Timer High Byte and Length Counter Load
#define REG_TRI_LINEAR 0x08 // Triangle Wave Linear Counter Control
#define REG_TRI_LO     0x0A // Triangle Wave Timer Low Byte
#define REG_TRI_HI     0x0B // Triangle Wave Timer High Byte and Length Counter Load
#define REG_NOISE_VOL  0x0C // Noise Channel Volume/Envelope Control
#define REG_NOISE_LO   0x0E // Noise Channel Timer/Period Control
#define REG_NOISE_HI   0x0F // Noise Channel Length Counter Load
#define REG_DMC_CTRL   0x10 // DMC (Delta Modulation Channel) Control and Frequency
#define REG_DMC_DAC    0x11 // DMC Direct Load for DAC
#define REG_DMC_ADDR   0x12 // DMC Sample Address
#define REG_DMC_LEN    0x13 // DMC Sample Length
#define REG_OAMDMA     0x14 // DMA Transfer to PPU OAM (Object Attribute Memory)
#define REG_APU_STATUS 0x15 // APU and DMC Status
#define REG_JOY1       0x16 // Controller 1 (Joypad 1) Input and Strobe
#define REG_JOY2       0x17 // Controller 2 (Joypad 2) Input and Frame Counter Control

// 0x4018–0x401F: Reserved for APU and I/O functionality, normally disabled
#define APU_TEST_MODE_1 0x18 // APU Test Mode (normally disabled)
#define APU_TEST_MODE_2 0x19 // APU Test Mode (normally disabled)
#define APU_TEST_MODE_3 0x1A // APU Test Mode (normally disabled)
#define APU_TEST_MODE_4 0x1B // APU Test Mode (normally disabled)
#define APU_TEST_MODE_5 0x1C // APU Test Mode (normally disabled)
#define APU_TEST_MODE_6 0x1D // APU Test Mode (normally disabled)
#define APU_TEST_MODE_7 0x1E // APU Test Mode (normally disabled)
#define APU_TEST_MODE_8 0x1F // APU Test Mode (normally disabled)


#define DEVICE_TO_APU(p) ((apu_device_t)(p))

typedef struct apu_device_data {
    struct bus_device_data m_device;
    union apu_joypad_data m_joypad[2];
    uint8_t m_poll_joypad;
    uint16_t m_oam_dma_addr;
} *apu_device_t;

static uint8_t apu_read8(bus_device_t a_dev, uint16_t a_addr)
{
    apu_device_t apu = DEVICE_TO_APU(a_dev);

    uint8_t retval = 0xFF;

    switch (a_addr & 0x1F)
    {
    case REG_JOY1: // Read controller 1
        //printf("APU read controller 1\n");
        retval = apu->m_joypad[0].raw >> 7;
        apu->m_joypad[0].raw <<= 1;
        break;
    case REG_JOY2: // Read controller 2
        //printf("APU read controller 2\n");
        retval = apu->m_joypad[1].raw >> 7;
        apu->m_joypad[1].raw <<= 1;
        break;
    
    default:
        printf("APU read: %04X\n", a_addr);
        break;
    }

    // TODO: Implement APU read
    return retval;
}

static void apu_write8(bus_device_t a_dev, uint16_t a_addr, uint8_t a_value)
{
    apu_device_t apu = DEVICE_TO_APU(a_dev);

    switch (a_addr & 0x1F)
    {
    case REG_JOY1:
        if (a_value & 7)
        {
            //printf("APU start controller pooling\n");
            apu->m_poll_joypad = 1;
        }
        else
        {
            //printf("APU stop controller polling\n");
            apu->m_poll_joypad = 0;
        }
        break;
    case REG_OAMDMA:
        //printf("APU OAMDMA: %02X\n", a_value);
        apu->m_oam_dma_addr = ((uint16_t)a_value) << 8;
        break;
    case REG_APU_STATUS:
        if (a_value != 0) // 0 disables all APU channels
        {
            printf("APU status: %02X\n", a_value);
        }
        break;
        
    default:
        printf("APU write: %04X = %02X\n", a_addr, a_value);
        break;
    }

    return;
}

static struct bus_device_ops_data g_apu_ops =
{
        .read8 = apu_read8,
        .write8 = apu_write8
};

void apu_device_tick(bus_device_t a_dev, bus_t a_cpu_bus, apu_device_tick_state_t a_state)
{
    apu_device_t apu = DEVICE_TO_APU(a_dev);

    if (apu->m_poll_joypad)
    {
        if (a_state->out.poll_joypad == 0)
        {
            a_state->out.poll_joypad = 1;
            a_state->in.joypad1.raw = 0;
            a_state->in.joypad2.raw = 0;
        }
    }
    else
    {
        if (a_state->out.poll_joypad == 1)
        {
            a_state->out.poll_joypad = 0;
            apu->m_joypad[0].raw = a_state->in.joypad1.raw;
            apu->m_joypad[1].raw = a_state->in.joypad2.raw;
        }
    }

    if (apu->m_oam_dma_addr && (a_state->out.oam_dma == 0))
    {
        if ((apu->m_oam_dma_addr & 0xFF) == 0)
        {
            // Start of OAM DMA transfer, will stall CPU for 513/514 cycles
            a_state->out.oam_dma = 1; // Will be set to 0 by the when the CPU is stalled
        }

        uint8_t a_oam_byte = a_cpu_bus->read8(apu->m_oam_dma_addr);

        a_cpu_bus->write8(0x2004 /*PPU OAMDATA*/, a_oam_byte);

        if ((++apu->m_oam_dma_addr & 0xFF) == 0)
        {
            // End of OAM DMA transfer
            apu->m_oam_dma_addr = 0;
        }
    }
}

bus_device_t apu_device_create()
{
    apu_device_t apu = (apu_device_t)malloc(sizeof(struct apu_device_data));
    *apu = {};
    apu->m_device.m_ops = &g_apu_ops;

    return &apu->m_device;
}

void apu_device_destroy(bus_device_t a_apu_device)
{
    free(a_apu_device);
}