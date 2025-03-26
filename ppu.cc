#include "ppu.h"
#include "bus.h"

#include <stdlib.h>
#include <assert.h>

#define DEVICE_TO_PPU(p) ((ppu_device_t)(p))

typedef struct ppu_device_data *ppu_device_t;

struct ppu_device_data
{
    struct bus_device_data m_device;
    struct
    {
        uint8_t ppuctrl;
        uint8_t ppumask;
        uint8_t ppustatus;
        uint8_t oamaddr;
        uint8_t oamdata;
        uint8_t ppuscroll;
        uint8_t ppuaddr;
        uint8_t ppudata;
    } m_registers;
};

static void ppu_ctrl_write(ppu_device_t a_ppu, uint8_t a_value)
{
    __asm__ __volatile__("int $3");
    a_ppu->m_registers.ppuctrl = a_value;
}

static void ppu_mask_write(ppu_device_t a_ppu, uint8_t a_value)
{
    __asm__ __volatile__("int $3");
    a_ppu->m_registers.ppumask = a_value;
}

static uint8_t ppu_status_read(ppu_device_t a_ppu)
{
    __asm__ __volatile__("int $3");
    return a_ppu->m_registers.ppustatus;
}

static void ppu_oamaddr_write(ppu_device_t a_ppu, uint8_t a_value)
{
    __asm__ __volatile__("int $3");
    a_ppu->m_registers.oamaddr = a_value;
}

static uint8_t ppu_oamdata_read(ppu_device_t a_ppu)
{
    __asm__ __volatile__("int $3");
    return a_ppu->m_registers.oamdata;
}

static void ppu_oamdata_write(ppu_device_t a_ppu, uint8_t a_value)
{
    __asm__ __volatile__("int $3");
    a_ppu->m_registers.oamdata = a_value;
}

static void ppu_ppuscroll_write(ppu_device_t a_ppu, uint8_t a_value)
{
    __asm__ __volatile__("int $3");
    a_ppu->m_registers.ppuscroll = a_value;
}

static void ppu_ppuaddr_write(ppu_device_t a_ppu, uint8_t a_value)
{
    __asm__ __volatile__("int $3");
    a_ppu->m_registers.ppuaddr = a_value;
}

static uint8_t ppu_ppudata_read(ppu_device_t a_ppu)
{
    __asm__ __volatile__("int $3");
    return a_ppu->m_registers.ppudata;
}

static void ppu_ppudata_write(ppu_device_t a_ppu, uint8_t a_value)
{
    __asm__ __volatile__("int $3");
    a_ppu->m_registers.ppudata = a_value;
}

void ppu_device_tick(bus_device_t a_ppu_device, cpu_t a_cpu)
{
    ppu_device_t ppu = DEVICE_TO_PPU(a_ppu_device);
    __asm__ __volatile__("int $3");
}

static uint8_t ppu_read8(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr)
{
    switch (a_addr & 0x7)
    {
        case 2: // PPUSTATUS
        return ppu_status_read(DEVICE_TO_PPU(a_dev));    
        case 4: // OAMDATA
        return ppu_oamdata_read(DEVICE_TO_PPU(a_dev));
        case 7: // PPUDATA
        return ppu_ppudata_read(DEVICE_TO_PPU(a_dev));
    }
    assert(0); // For debugging
}

static uint16_t ppu_read16(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr)
{
    assert(0);
    return 0;
}

static void ppu_write8(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr, uint8_t a_value)
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
        ppu_ppuscroll_write(DEVICE_TO_PPU(a_dev), a_value);
        break;
        case 6: // PPUADDR
        ppu_ppuaddr_write(DEVICE_TO_PPU(a_dev), a_value);
        break;
        case 7: // PPUDATA
        ppu_ppudata_write(DEVICE_TO_PPU(a_dev), a_value);
        break;
        default:
        assert(0); // For debugging
    }
}

static void ppu_write16(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr, uint16_t a_value)
{
    assert(0);
}

static struct bus_device_ops_data g_ppu_ops = 
{
    .read8 = ppu_read8,
    .read16 = ppu_read16,
    .write8 = ppu_write8,
    .write16 = ppu_write16
};

bus_device_t ppu_device_create()
{
    ppu_device_t ppu = (ppu_device_t)malloc(sizeof(ppu_device_data));
    *ppu = {};
    ppu->m_device.m_ops = &g_ppu_ops;
    return &ppu->m_device;
}

void ppu_device_destroy(bus_device_t a_ppu_device)
{
    ppu_device_t ppu = DEVICE_TO_PPU(a_ppu_device);   
    free(ppu);
}