#pragma once

#include "hw_types.h"

typedef struct ppu_rgb_color_data
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} *ppu_rgb_color_t;

typedef void (*ppu_frame_callback_t)(ppu_rgb_color_t a_frame_buffer, void *a_user_data);

bus_device_t ppu_device_create();

void ppu_device_destroy(bus_device_t a_ppu_device);

void ppu_device_tick(bus_device_t a_ppu_device, ppu_frame_callback_t a_frame_cb, void *a_frame_cb_user_data, uint32_t *a_nmi_out);

void ppu_device_attach(bus_device_t a_ppu_device, bus_device_t a_bus_device, uint16_t a_base, uint32_t a_size);