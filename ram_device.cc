#include <stdlib.h>

#include "ram_device.h"
#include "bus.h"

#define DEVICE_TO_RAM(p) ((ram_device_t)(p))

typedef struct ram_device_data
{
    struct bus_device_data m_device;
    uint16_t m_size;
    uint8_t m_data[2];
} *ram_device_t;

static uint8_t ram_read8(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr)
{
    ram_device_t ram = DEVICE_TO_RAM(a_dev);
    return ram->m_data[a_addr & (ram->m_size - 1)];
}

static uint16_t ram_read16(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr)
{
    return ram_read8(a_dev, a_cpu, a_addr) | (ram_read8(a_dev, a_cpu, a_addr + 1) << 8);
}

static void ram_write8(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr, uint8_t a_value)
{
    ram_device_t ram = DEVICE_TO_RAM(a_dev);

    ram->m_data[a_addr & (ram->m_size - 1)] = a_value;
}

static void ram_write16(bus_device_t a_dev, cpu_t a_cpu, uint16_t a_addr, uint16_t a_value)
{
    ram_write8(a_dev, a_cpu, a_addr, a_value & 0xFF);
    ram_write8(a_dev, a_cpu, a_addr + 1, a_value >> 8);
}

struct bus_device_ops_data g_ram_ops = 
{
    .read8 = ram_read8,
    .read16 = ram_read16,
    .write8 = ram_write8,
    .write16 = ram_write16
};

bus_device_t ram_device_create(uint16_t a_size)
{
    // Round up the size to the nearest page
    a_size = (a_size + PAGE_MASK) & ~PAGE_MASK;

    // Round up to nearest power of 2, makes modulo operations faster by using bitwise AND
    a_size = 1 << (32 - __builtin_clz(a_size - 1));

    ram_device_t ram = (ram_device_t)malloc(sizeof(ram_device_data) + a_size);

    *ram = {};

    ram->m_size = a_size;

    ram->m_device.m_ops = &g_ram_ops;

    for (int i = 0; i < ram->m_size; i++)
    {
        ram->m_data[i] = 0xFF;
    }

    return &ram->m_device;
}

uint16_t ram_device_size(bus_device_t a_device)
{
    ram_device_t ram = DEVICE_TO_RAM(a_device);

    return ram->m_size;
}
