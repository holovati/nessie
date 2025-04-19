#pragma once

#include "hw_types.h"

typedef union apu_joypad_data
{
    struct 
    {
        uint8_t right : 1;
        uint8_t left : 1;
        uint8_t down : 1;
        uint8_t up : 1;
        uint8_t start : 1;
        uint8_t select : 1;
        uint8_t b : 1;
        uint8_t a : 1;
    } button;

    uint8_t raw;
} *apu_joypad_t;

typedef struct apu_device_tick_state_data
{
    struct 
    {
        union apu_joypad_data joypad1;
        union apu_joypad_data joypad2;
    } in;
    struct 
    {
        uint8_t poll_joypad : 1;
        uint8_t oam_dma : 1;
    } out;
} *apu_device_tick_state_t;

bus_device_t apu_device_create();

void apu_device_destroy(bus_device_t a_apu_device);

void apu_device_tick(bus_device_t a_apu_device, bus_t a_cpu_bus, apu_device_tick_state_t a_state);