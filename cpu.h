#pragma once

#include "hw_types.h"

struct cpu_data
{
    struct register_data
    {
        // 6502 registers
        uint8_t a; // Accumulator
        uint8_t x; // X register
        uint8_t y; // Y register
        uint8_t s; // Stack pointer
        uint16_t pc; // Program counter
        union 
        {
            struct {
                uint8_t c : 1; // Carry flag
                uint8_t z : 1; // Zero flag
                uint8_t i : 1; // Interrupt disable
                uint8_t d : 1; // Decimal mode
                uint8_t b : 1; // Break command
                uint8_t u : 1; // Unused
                uint8_t v : 1; // Overflow flag
                uint8_t n : 1; // Negative flag
            } flag;
            uint8_t raw; // Processor status
        } status;
    } m_registers;

    uint8_t m_nmi : 1, __unused : 7;

    uint32_t m_remaining_cycles;

    void power_on(bus_t a_bus);

    void nmi();

    void tick(bus_t a_bus);
};