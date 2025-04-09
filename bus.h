#pragma once

#include <stdint.h>

#include "hw_types.h"

struct bus_data
{
    bus_device_t m_device_map[BUS_PAGES];

    void initialize();

    void attach(bus_device_t a_device, uint16_t a_base, uint32_t a_size);

    uint8_t read8(uint16_t a_addr);
    uint16_t read16(uint16_t a_addr);
    void write8(uint16_t a_addr, uint8_t a_value);
    void write16(uint16_t a_addr, uint16_t a_value);
};

struct bus_device_data 
{
    bus_device_t m_next;
    bus_device_ops_t m_ops;
    uint16_t m_base;
    uint16_t m_size;
};

struct bus_device_ops_data
{
    uint8_t (*read8)(bus_device_t a_dev, uint16_t a_addr);
    void (*write8)(bus_device_t a_dev, uint16_t a_addr, uint8_t a_value);
};