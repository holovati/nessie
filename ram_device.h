#pragma once

#include "hw_types.h"

bus_device_t ram_device_create(uint16_t a_size);

uint16_t ram_device_size(bus_device_t a_ram_device);

// Utility function to write a buffer to the RAM device
// Returns the number of bytes written
size_t ram_device_write_buffer(bus_device_t a_ram_device, uint16_t a_addr, const uint8_t *a_buffer, size_t a_size);