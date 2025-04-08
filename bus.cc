#include "bus.h"

void bus_data::initialize()
{
    for (int i = 0; i < BUS_PAGES; i++)
    {
        m_device_map[i] = nullptr;
    }
}
            
void bus_data::attach(bus_device_t a_device, uint16_t a_base, uint32_t a_size)
{
    // TODO
    a_device->m_next = nullptr;
    
    // Round down the base adress to the nearest page
    a_device->m_base = a_base & ~PAGE_MASK;
    
    // Round up the size to the nearest page
    a_device->m_size = (a_size + PAGE_MASK) & ~PAGE_MASK;;

    // Attach the device to the bus
    for (uint16_t page_number = a_base >> PAGE_SHIFT; page_number < (a_base + a_size) >> PAGE_SHIFT; page_number++)
    {
        m_device_map[page_number] = a_device;
    }
}

uint8_t bus_data::read8(cpu_t a_cpu, uint16_t a_addr)
{
    bus_device_t dev = m_device_map[a_addr >> PAGE_SHIFT];

    if (dev)
    {
        return dev->m_ops->read8(dev, a_cpu, a_addr - dev->m_base);
    }

    return 0xFF;
}

// This is a convienience function, in reallity we just do two 8 bit reads
uint16_t bus_data::read16(cpu_t a_cpu, uint16_t a_addr)
{
    bus_device_t dev = m_device_map[a_addr >> PAGE_SHIFT];

    if (dev)
    {
        a_addr -= dev->m_base;
        return (((uint16_t)dev->m_ops->read8(dev, a_cpu, a_addr + 1)) << 8) | ((uint16_t)dev->m_ops->read8(dev, a_cpu, a_addr));
    }

    return 0xFFFF;
}

void bus_data::write8(cpu_t a_cpu, uint16_t a_addr, uint8_t a_value)
{
    bus_device_t dev = m_device_map[a_addr >> PAGE_SHIFT];

    if (dev)
    {
        dev->m_ops->write8(dev, a_cpu, a_addr - dev->m_base, a_value);
    }
}

void bus_data::write16(cpu_t a_cpu, uint16_t a_addr, uint16_t a_value)
{
    bus_device_t dev = m_device_map[a_addr >> PAGE_SHIFT];

    if (dev)
    {
        dev->m_ops->write16(dev, a_cpu, a_addr - dev->m_base, a_value);
    }
}
