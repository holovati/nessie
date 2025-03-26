#pragma once

#include "hw_types.h"

bus_device_t ram_device_create(uint16_t a_size);

uint16_t ram_device_size(bus_device_t a_ram_device);

