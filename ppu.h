#pragma once

#include "hw_types.h"

bus_device_t ppu_device_create();

void ppu_device_destroy(bus_device_t a_ppu_device);

void ppu_device_tick(bus_device_t a_ppu_device, cpu_t a_cpu);