#include "cpu.h"
#include "bus.h"

#define CPU_FLAG_CARRY 0x01
#define CPU_FLAG_ZERO 0x02
#define CPU_FLAG_INTERRUPT_DISABLE 0x04
#define CPU_FLAG_DECIMAL 0x08
#define CPU_FLAG_BREAK 0x10
#define CPU_FLAG_UNUSED 0x20
#define CPU_FLAG_OVERFLOW 0x40
#define CPU_FLAG_NEGATIVE 0x80

typedef struct opcode_data *opcode_t;

enum cpu_addressing_mode : uint16_t
{
    CPU_ADDRESSING_MODE_IMPLIED,
    CPU_ADDRESSING_MODE_ACCUMULATOR,
    CPU_ADDRESSING_MODE_IMMEDIATE,
    CPU_ADDRESSING_MODE_ZERO_PAGE,
    CPU_ADDRESSING_MODE_ZERO_PAGE_X,
    CPU_ADDRESSING_MODE_ZERO_PAGE_Y,
    CPU_ADDRESSING_MODE_RELATIVE,
    CPU_ADDRESSING_MODE_ABSOLUTE,
    CPU_ADDRESSING_MODE_ABSOLUTE_X,
    CPU_ADDRESSING_MODE_ABSOLUTE_Y,
    CPU_ADDRESSING_MODE_INDIRECT,
    CPU_ADDRESSING_MODE_INDEXED_INDIRECT,
    CPU_ADDRESSING_MODE_INDIRECT_INDEXED
};

static struct opcode_data {
    const char mnemonic[4];
    uint8_t length;
    uint8_t cycles;
    cpu_addressing_mode mode;
} g_opcodes[0x10][0x10] =
{
    {
        {"BRK", 1, 7, CPU_ADDRESSING_MODE_IMPLIED},
        {"ORA", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 8, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 3, CPU_ADDRESSING_MODE_IMPLIED},
        {"ORA", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"ASL", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"???", 1, 5, CPU_ADDRESSING_MODE_IMPLIED},
        {"PHP", 1, 3, CPU_ADDRESSING_MODE_IMPLIED},
        {"ORA", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"ASL", 1, 2, CPU_ADDRESSING_MODE_ACCUMULATOR},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"ORA", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"ASL", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"BPL", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"ORA", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 8, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"ORA", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"ASL", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED},
        {"CLC", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"ORA", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 7, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"ORA", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"ASL", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"???", 1, 7, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"JSR", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"AND", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 8, CPU_ADDRESSING_MODE_IMPLIED},
        {"BIT", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"AND", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"ROL", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"???", 1, 5, CPU_ADDRESSING_MODE_IMPLIED},
        {"PLP", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"AND", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"ROL", 1, 2, CPU_ADDRESSING_MODE_ACCUMULATOR},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"BIT", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"AND", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"ROL", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"BMI", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"AND", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 8, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"AND", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"ROL", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED},
        {"SEC", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"AND", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 7, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"AND", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"ROL", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"???", 1, 7, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"RTI", 1, 6, CPU_ADDRESSING_MODE_IMPLIED},
        {"EOR", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 8, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 3, CPU_ADDRESSING_MODE_IMPLIED},
        {"EOR", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"LSR", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"???", 1, 5, CPU_ADDRESSING_MODE_IMPLIED},
        {"PHA", 1, 3, CPU_ADDRESSING_MODE_IMPLIED},
        {"EOR", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"LSR", 1, 2, CPU_ADDRESSING_MODE_ACCUMULATOR},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"JMP", 3, 3, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"EOR", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"LSR", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"BVC", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"EOR", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 8, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"EOR", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"LSR", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED},
        {"CLI", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"EOR", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 7, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"EOR", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"LSR", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"???", 1, 7, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"RTS", 1, 6, CPU_ADDRESSING_MODE_IMPLIED},
        {"ADC", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 8, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 3, CPU_ADDRESSING_MODE_IMPLIED},
        {"ADC", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"ROR", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"???", 1, 5, CPU_ADDRESSING_MODE_IMPLIED},
        {"PLA", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"ADC", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"ROR", 1, 2, CPU_ADDRESSING_MODE_ACCUMULATOR},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"JMP", 3, 3, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"ADC", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"ROR", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"BVS", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"ADC", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 8, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"ADC", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"ROR", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED},
        {"SEI", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"ADC", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 7, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"ADC", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"ROR", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"???", 1, 7, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"STA", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED},
        {"STY", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"STA", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"STX", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"???", 1, 3, CPU_ADDRESSING_MODE_IMPLIED},
        {"DEY", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"TXA", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"STY", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"STA", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"STX", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"BCC", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"STA", 2, 6, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED},
        {"STY", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"STA", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"STX", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_Y},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"TYA", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"STA", 3, 5, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"TXS", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 5, CPU_ADDRESSING_MODE_IMPLIED},
        {"STY", 3, 5, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"STA", 3, 5, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"STX", 3, 5, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"???", 1, 5, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"LDY", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"LDA", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"LDX", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED},
        {"LDY", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"LDA", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"LDX", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"???", 1, 3, CPU_ADDRESSING_MODE_IMPLIED},
        {"TAY", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"LDA", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"TAX", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"LDY", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"LDA", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"LDX", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"BCS", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"LDA", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 5, CPU_ADDRESSING_MODE_IMPLIED},
        {"LDY", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"LDA", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"LDX", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_Y},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"CLV", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"LDA", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"TSX", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"LDY", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"LDA", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"LDX", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"CPY", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"CMP", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 8, CPU_ADDRESSING_MODE_IMPLIED},
        {"CPY", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"CMP", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"DEC", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"???", 1, 5, CPU_ADDRESSING_MODE_IMPLIED},
        {"INY", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"CMP", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"DEX", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"CPY", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"CMP", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"DEC", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"BNE", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"CMP", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 8, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"CMP", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"DEC", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED},
        {"CLD", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"CMP", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 7, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 1, CPU_ADDRESSING_MODE_ABSOLUTE}, // ?
        {"CMP", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"DEC", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"???", 1, 7, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"CPX", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"SBC", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 8, CPU_ADDRESSING_MODE_IMPLIED},
        {"CPX", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"SBC", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"INC", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"???", 1, 5, CPU_ADDRESSING_MODE_IMPLIED},
        {"INX", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"SBC", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"NOP", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"CPX", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"SBC", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"INC", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED}
    },
    {
        {"BEQ", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"SBC", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 8, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"SBC", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"INC", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"???", 1, 6, CPU_ADDRESSING_MODE_IMPLIED},
        {"SED", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"SBC", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"???", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 7, CPU_ADDRESSING_MODE_IMPLIED},
        {"???", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"SBC", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"INC", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"???", 1, 7, CPU_ADDRESSING_MODE_IMPLIED}
    }
};

static uint8_t opcode_read8(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = 0;
    
    switch (a_opcode->mode)
    {
    case CPU_ADDRESSING_MODE_ACCUMULATOR:
    {
        value = a_cpu->m_registers.a;
    }
    break;
    case CPU_ADDRESSING_MODE_IMMEDIATE:
    {
        value = a_bus->read8(a_cpu, a_cpu->m_registers.pc + 1);
    }
    break;
    case CPU_ADDRESSING_MODE_ZERO_PAGE:
    {
        value = a_bus->read8(a_cpu, a_cpu->m_registers.pc + 1);
        value = a_bus->read8(a_cpu, value);
    }
    break;
    case CPU_ADDRESSING_MODE_ZERO_PAGE_X:
    {
        value = a_bus->read8(a_cpu, a_cpu->m_registers.pc + 1);
        value = (value + a_cpu->m_registers.x) & 0xFF;
        value = a_bus->read8(a_cpu, value);
    }
    break;
    case CPU_ADDRESSING_MODE_ZERO_PAGE_Y:
    {
        value = a_bus->read8(a_cpu, a_cpu->m_registers.pc + 1);
        value = (value + a_cpu->m_registers.y) & 0xFF;
        value = a_bus->read8(a_cpu, value);
    }
    break;
    case CPU_ADDRESSING_MODE_ABSOLUTE:
    {
        uint16_t addr = a_bus->read16(a_cpu, a_cpu->m_registers.pc + 1);
        value = a_bus->read8(a_cpu, addr);
    }
    break;
    case CPU_ADDRESSING_MODE_ABSOLUTE_X:
    {
        uint16_t addr = a_bus->read16(a_cpu, a_cpu->m_registers.pc + 1);
        value = a_bus->read8(a_cpu, addr + a_cpu->m_registers.x);

        // Add one cycle if page boundary is crossed
        if ((addr & 0xFF00) != ((addr + a_cpu->m_registers.x) & 0xFF00))
        {
            a_cpu->m_remaining_cycles++;
        }
    }
    break;
    case CPU_ADDRESSING_MODE_ABSOLUTE_Y:
    {
        uint16_t addr = a_bus->read16(a_cpu, a_cpu->m_registers.pc + 1);
        value = a_bus->read8(a_cpu, addr + a_cpu->m_registers.y);

        // Add one cycle if page boundary is crossed
        if ((addr & 0xFF00) != ((addr + a_cpu->m_registers.y) & 0xFF00))
        {
            a_cpu->m_remaining_cycles++;
        }
    }
    break;
    case CPU_ADDRESSING_MODE_INDEXED_INDIRECT:
    {
        uint16_t addr = a_bus->read8(a_cpu, a_cpu->m_registers.pc + 1);
        addr = (addr + a_cpu->m_registers.x) & 0xFF;
        addr = a_bus->read16(a_cpu, addr);
        
        value = a_bus->read8(a_cpu, addr);
    }
    break;
    case CPU_ADDRESSING_MODE_INDIRECT_INDEXED:
    {
        uint16_t addr = a_bus->read8(a_cpu, a_cpu->m_registers.pc + 1);
        addr = a_bus->read16(a_cpu, addr);
        addr = addr + a_cpu->m_registers.y;

        // Add one cycle if page boundary is crossed
        if ((addr & 0xFF00) != ((addr + a_cpu->m_registers.y) & 0xFF00))
        {
            a_cpu->m_remaining_cycles++;
        }

        value = a_bus->read8(a_cpu, addr);
    }
    break;
    default:
    break;
    }

    return value;
}

static void opcode_write8(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode, uint8_t a_value)
{
    switch (a_opcode->mode)
    {
    case CPU_ADDRESSING_MODE_ACCUMULATOR:
    {
        a_cpu->m_registers.a = a_value;
    }
    break;
    case CPU_ADDRESSING_MODE_ZERO_PAGE:
    {
        uint8_t addr = a_bus->read8(a_cpu, a_cpu->m_registers.pc + 1);
        a_bus->write8(a_cpu, addr, a_value);
    }
    break;
    case CPU_ADDRESSING_MODE_ZERO_PAGE_X:
    {
        uint8_t addr = a_bus->read8(a_cpu, a_cpu->m_registers.pc + 1);
        addr = (addr + a_cpu->m_registers.x) & 0xFF;
        a_bus->write8(a_cpu, addr, a_value);
    }
    break;
    case CPU_ADDRESSING_MODE_ZERO_PAGE_Y:
    {
        uint8_t addr = a_bus->read8(a_cpu, a_cpu->m_registers.pc + 1);
        addr = (addr + a_cpu->m_registers.y) & 0xFF;
        a_bus->write8(a_cpu, addr, a_value);
    }
    break;
    case CPU_ADDRESSING_MODE_ABSOLUTE:
    {
        uint16_t addr = a_bus->read16(a_cpu, a_cpu->m_registers.pc + 1);
        a_bus->write8(a_cpu, addr, a_value);
    }
    break;
    case CPU_ADDRESSING_MODE_ABSOLUTE_X:
    {
        uint16_t addr = a_bus->read16(a_cpu, a_cpu->m_registers.pc + 1);
        addr = addr + a_cpu->m_registers.x;
        a_bus->write8(a_cpu, addr, a_value);
    }
    break;
    case CPU_ADDRESSING_MODE_ABSOLUTE_Y:
    {
        uint16_t addr = a_bus->read16(a_cpu, a_cpu->m_registers.pc + 1);
        addr = addr + a_cpu->m_registers.y;
        a_bus->write8(a_cpu, addr, a_value);
    }
    break;
    case CPU_ADDRESSING_MODE_INDEXED_INDIRECT:
    {
        uint8_t addr = a_bus->read8(a_cpu, a_cpu->m_registers.pc + 1);
        addr = (addr + a_cpu->m_registers.x) & 0xFF;
        addr = a_bus->read16(a_cpu, addr);
        a_bus->write8(a_cpu, addr, a_value);
    }
    break;
    case CPU_ADDRESSING_MODE_INDIRECT_INDEXED:
    {
        uint8_t addr = a_bus->read8(a_cpu, a_cpu->m_registers.pc + 1);
        addr = a_bus->read16(a_cpu, addr);
        addr = addr + a_cpu->m_registers.y;
        a_bus->write8(a_cpu, addr, a_value);
    }
    break;
    default:
    break;
    }
}

static void opcode_branch(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // If the branch is taken, add an extra cycle
    a_cpu->m_remaining_cycles++;
    
    uint8_t offset = a_bus->read8(a_cpu, a_cpu->m_registers.pc + 1);

    // We need the old pc for the page boundary check
    uint16_t old_pc = a_cpu->m_registers.pc;
    
    // Branch, relative addressing
    if (offset >> 7)
    {
        a_cpu->m_registers.pc -= (0x100 - offset);
    }
    else
    {
        a_cpu->m_registers.pc += offset;
    }

    // This is the actual new pc when this instruction is done executing
    uint16_t new_pc = a_cpu->m_registers.pc + a_opcode->length;

    // Add an extra cycle if the branch crossed a page boundary
    if ((old_pc & 0xFF00) != (new_pc & 0xFF00))
    {
        a_cpu->m_remaining_cycles++;
    }
}

static void opcode_push_stack8(cpu_t a_cpu, bus_t a_bus, uint8_t a_value)
{
    a_cpu->m_registers.s--;
    a_bus->write8(a_cpu, 0x0100 + a_cpu->m_registers.s, a_value);
}

static void opcode_push_stack16(cpu_t a_cpu, bus_t a_bus, uint16_t a_value)
{
    a_cpu->m_registers.s -= 2;
    a_bus->write16(a_cpu, 0x0100 + a_cpu->m_registers.s, a_value);
}

static uint8_t opcode_pop_stack8(cpu_t a_cpu, bus_t a_bus)
{
    uint8_t value = a_bus->read8(a_cpu, 0x0100 + a_cpu->m_registers.s);
    a_cpu->m_registers.s++;
    return value;
}

static uint16_t opcode_pop_stack16(cpu_t a_cpu, bus_t a_bus)
{
    uint16_t value = a_bus->read16(a_cpu, 0x0100 + a_cpu->m_registers.s);
    a_cpu->m_registers.s += 2;
    return value;
}

static void opcode_adc(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);
    uint16_t result = a_cpu->m_registers.a + value + a_cpu->m_registers.status.flag.c;

    a_cpu->m_registers.status.flag.c = result > 0xFF;
    a_cpu->m_registers.status.flag.z = (result & 0xFF) == 0;
    a_cpu->m_registers.status.flag.v = !!(~(a_cpu->m_registers.a ^ value) & (a_cpu->m_registers.a ^ result) & 0x80);
    a_cpu->m_registers.status.flag.n = !!(result & 0x80);

    a_cpu->m_registers.a = result & 0xFF;
}

static void opcode_and(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    a_cpu->m_registers.a &= value;

    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.a == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.a >> 7);
}

static void opcode_asl(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    a_cpu->m_registers.status.flag.c = !!(value >> 7);
    value <<= 1;

    opcode_write8(a_cpu, a_bus, a_opcode, value);

    a_cpu->m_registers.status.flag.z = value == 0;
    a_cpu->m_registers.status.flag.n = !!(value >> 7);
}

static void opcode_bcc(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    if (!a_cpu->m_registers.status.flag.c)
    {
        opcode_branch(a_cpu, a_bus, a_opcode);
    }
}

static void opcode_bcs(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    if (a_cpu->m_registers.status.flag.c)
    {
        opcode_branch(a_cpu, a_bus, a_opcode);
    }
}

static void opcode_beq(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    if (a_cpu->m_registers.status.flag.z)
    {
        opcode_branch(a_cpu, a_bus, a_opcode);
    }
}

static void opcode_bit(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    uint8_t result = a_cpu->m_registers.a & value;

    a_cpu->m_registers.status.flag.z = result == 0;
    a_cpu->m_registers.status.flag.v = (value >> 6) & 1;
    a_cpu->m_registers.status.flag.n = (value >> 7) & 1;
}

static void opcode_bmi(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    if (a_cpu->m_registers.status.flag.n)
    {
        opcode_branch(a_cpu, a_bus, a_opcode);
    }
}

static void opcode_bne(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    if (!a_cpu->m_registers.status.flag.z)
    {
        opcode_branch(a_cpu, a_bus, a_opcode);
    }
}

static void opcode_bpl(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    if (!a_cpu->m_registers.status.flag.n)
    {
        opcode_branch(a_cpu, a_bus, a_opcode);
    }
}

static void opcode_brk(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    opcode_push_stack16(a_cpu, a_bus, a_cpu->m_registers.pc + a_opcode->length);
    opcode_push_stack8(a_cpu, a_bus, a_cpu->m_registers.status.raw | CPU_FLAG_BREAK);
    a_cpu->m_registers.pc = a_bus->read16(a_cpu, 0xFFFE) - a_opcode->length;
}

static void opcode_bvc(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    if (!a_cpu->m_registers.status.flag.v)
    {
        opcode_branch(a_cpu, a_bus, a_opcode);
    }
}

static void opcode_bvs(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    if (a_cpu->m_registers.status.flag.v)
    {
        opcode_branch(a_cpu, a_bus, a_opcode);
    }
}

static void opcode_clc(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.status.flag.c = 0;
}

static void opcode_cld(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.status.flag.d = 0;   
}

static void opcode_cli(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.status.flag.i = 0;
}

static void opcode_clv(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.status.flag.v = 0;
}

static void opcode_cmp(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    uint8_t result = a_cpu->m_registers.a - value;

    a_cpu->m_registers.status.flag.c = a_cpu->m_registers.a >= value;
    a_cpu->m_registers.status.flag.z = result == 0;
    a_cpu->m_registers.status.flag.n = !!(result >> 7);
}

static void opcode_cpx(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    uint8_t result = a_cpu->m_registers.x - value;

    a_cpu->m_registers.status.flag.c = a_cpu->m_registers.x >= value;
    a_cpu->m_registers.status.flag.z = result == 0;
    a_cpu->m_registers.status.flag.n = !!(result >> 7);
}

static void opcode_cpy(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    uint8_t result = a_cpu->m_registers.y - value;

    a_cpu->m_registers.status.flag.c = a_cpu->m_registers.y >= value;
    a_cpu->m_registers.status.flag.z = result == 0;
    a_cpu->m_registers.status.flag.n = !!(result >> 7);
}

static void opcode_dec(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);
    value--;
    opcode_write8(a_cpu, a_bus, a_opcode, value);

    a_cpu->m_registers.status.flag.z = value == 0;
    a_cpu->m_registers.status.flag.n = !!(value >> 7);
}

static void opcode_dex(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.status.flag.z = ((--a_cpu->m_registers.x) == 0);
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.x >> 7);
}

static void opcode_dey(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.status.flag.z = (--a_cpu->m_registers.y) == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.y >> 7);
}

static void opcode_eor(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    a_cpu->m_registers.a ^= value;

    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.a == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.a >> 7);
}

static void opcode_inc(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);
    value++;
    opcode_write8(a_cpu, a_bus, a_opcode, value);

    a_cpu->m_registers.status.flag.z = value == 0;
    a_cpu->m_registers.status.flag.n = !!(value >> 7);
}

static void opcode_inx(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.status.flag.z = ((++a_cpu->m_registers.x) == 0);
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.x >> 7);
}

static void opcode_iny(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.status.flag.z = (++a_cpu->m_registers.y) == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.y >> 7);
}

static void opcode_jmp(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    if (a_opcode->mode == CPU_ADDRESSING_MODE_ABSOLUTE)
    {
        a_cpu->m_registers.pc = a_bus->read16(a_cpu, a_cpu->m_registers.pc + 1) - a_opcode->length;
    }
    else /* if (a_opcode->mode == CPU_ADDRESSING_MODE_INDIRECT) */
    {
        uint16_t addr = a_bus->read16(a_cpu, a_cpu->m_registers.pc + 1);
        a_cpu->m_registers.pc = a_bus->read16(a_cpu, addr);
    }
}

static void opcode_jsr(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint16_t addr = a_bus->read16(a_cpu, a_cpu->m_registers.pc + 1) - a_opcode->length;
    uint16_t return_addr = a_cpu->m_registers.pc + a_opcode->length - 1;

    opcode_push_stack16(a_cpu, a_bus, return_addr);

    a_cpu->m_registers.pc = addr;
}

static void opcode_lda(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.a = opcode_read8(a_cpu, a_bus, a_opcode);

    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.a == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.a >> 7);
}

static void opcode_ldx(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.x = opcode_read8(a_cpu, a_bus, a_opcode);

    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.x == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.x >> 7);
}

static void opcode_ldy(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.y = opcode_read8(a_cpu, a_bus, a_opcode);

    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.y == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.y >> 7);
}

static void opcode_lsr(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    a_cpu->m_registers.status.flag.c = value & 0x01;
    value >>= 1;

    opcode_write8(a_cpu, a_bus, a_opcode, value);

    a_cpu->m_registers.status.flag.z = value == 0;
    a_cpu->m_registers.status.flag.n = !!(value >> 7);
}

static void opcode_nop(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // Do nothing
}

static void opcode_ora(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    a_cpu->m_registers.a |= value;

    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.a == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.a >> 7);
}

static void opcode_pha(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_bus->write8(a_cpu, 0x0100 + (a_cpu->m_registers.s--), a_cpu->m_registers.a);
}

static void opcode_php(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_bus->write8(a_cpu, 0x0100 + (a_cpu->m_registers.s--), a_cpu->m_registers.status.raw);
}

static void opcode_pla(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.a = a_bus->read8(a_cpu, 0x0100 + (++a_cpu->m_registers.s));
    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.a == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.a >> 7);
}

static void opcode_plp(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.status.raw = a_bus->read8(a_cpu, 0x0100 + (++a_cpu->m_registers.s));
}

static void opcode_rol(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    uint8_t carry = !!(a_cpu->m_registers.status.flag.c);
    a_cpu->m_registers.status.flag.c = value >> 7;
    value = (carry << 7) | (value << 1);

    // We don't want to increment the cycle count twice.
    uint8_t remaining_cycles = a_cpu->m_remaining_cycles;
    
    opcode_write8(a_cpu, a_bus, a_opcode, value);
    
    // Restore the cycle count
    a_cpu->m_remaining_cycles = remaining_cycles;
}

static void opcode_ror(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    uint8_t carry = !!(a_cpu->m_registers.status.flag.c);
    a_cpu->m_registers.status.flag.c = value & 0x01;
    value = (carry << 7) | (value >> 1);

    // We don't want to increment the cycle count twice.
    uint8_t remaining_cycles = a_cpu->m_remaining_cycles;
    
    opcode_write8(a_cpu, a_bus, a_opcode, value);
    
    // Restore the cycle count
    a_cpu->m_remaining_cycles = remaining_cycles;
}

static void opcode_rti(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.status.raw = opcode_pop_stack8(a_cpu, a_bus);
    a_cpu->m_registers.pc = opcode_pop_stack16(a_cpu, a_bus);
}

static void opcode_rts(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.pc = opcode_pop_stack16(a_cpu, a_bus);
}

static void opcode_sbc(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);
    uint16_t result = a_cpu->m_registers.a - value - (!a_cpu->m_registers.status.flag.c);

    a_cpu->m_registers.status.flag.c = !(result > 0xFF);
    a_cpu->m_registers.status.flag.z = (result & 0xFF) == 0;
    a_cpu->m_registers.status.flag.v = !((a_cpu->m_registers.a ^ value) & (a_cpu->m_registers.a ^ result) & 0x80);
    a_cpu->m_registers.status.flag.n = !!(result & 0x80);

    a_cpu->m_registers.a = result & 0xFF;
}

static void opcode_sec(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.status.flag.c = 1;
}

static void opcode_sed(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.status.flag.d = 1;
}

static void opcode_sei(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.status.flag.i = 1;
}

static void opcode_sta(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    opcode_write8(a_cpu, a_bus, a_opcode, a_cpu->m_registers.a);
}

static void opcode_stx(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    opcode_write8(a_cpu, a_bus, a_opcode, a_cpu->m_registers.x);
}

static void opcode_sty(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    opcode_write8(a_cpu, a_bus, a_opcode, a_cpu->m_registers.y);
}

static void opcode_tax(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.x = a_cpu->m_registers.a;
    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.x == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.x >> 7);
}

static void opcode_tay(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.y = a_cpu->m_registers.a;
    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.y == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.y >> 7);
}

static void opcode_tsx(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.x = a_cpu->m_registers.s;
    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.x == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.x >> 7);
}

static void opcode_txa(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.a = a_cpu->m_registers.x;
    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.a == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.a >> 7);
}

static void opcode_txs(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.s = a_cpu->m_registers.x;
}

static void opcode_tya(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.a = a_cpu->m_registers.y;
    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.a == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.a >> 7);
}

static void opcode_inv(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode) 
{
    // Invalid opcode
    __asm__("int $3");
}

static void (*g_opcode_handlers[])(cpu_t, bus_t, opcode_t) =
{
    opcode_brk, opcode_ora, opcode_inv, opcode_inv, opcode_inv, opcode_ora, opcode_asl, opcode_inv,
    opcode_php, opcode_ora, opcode_asl, opcode_inv, opcode_inv, opcode_ora, opcode_asl, opcode_inv,
    opcode_bpl, opcode_ora, opcode_inv, opcode_inv, opcode_inv, opcode_ora, opcode_asl, opcode_inv,
    opcode_clc, opcode_ora, opcode_inv, opcode_inv, opcode_inv, opcode_ora, opcode_asl, opcode_inv,
    opcode_jsr, opcode_and, opcode_inv, opcode_inv, opcode_bit, opcode_and, opcode_rol, opcode_inv,
    opcode_plp, opcode_and, opcode_rol, opcode_inv, opcode_bit, opcode_and, opcode_rol, opcode_inv,
    opcode_bmi, opcode_and, opcode_inv, opcode_inv, opcode_inv, opcode_and, opcode_rol, opcode_inv,
    opcode_sec, opcode_and, opcode_inv, opcode_inv, opcode_inv, opcode_and, opcode_rol, opcode_inv,
    opcode_rti, opcode_eor, opcode_inv, opcode_inv, opcode_inv, opcode_eor, opcode_lsr, opcode_inv,
    opcode_pha, opcode_eor, opcode_lsr, opcode_inv, opcode_jmp, opcode_eor, opcode_lsr, opcode_inv,
    opcode_bvc, opcode_eor, opcode_inv, opcode_inv, opcode_inv, opcode_eor, opcode_lsr, opcode_inv,
    opcode_cli, opcode_eor, opcode_inv, opcode_inv, opcode_inv, opcode_eor, opcode_lsr, opcode_inv,
    opcode_rts, opcode_adc, opcode_inv, opcode_inv, opcode_inv, opcode_adc, opcode_ror, opcode_inv,
    opcode_pla, opcode_adc, opcode_ror, opcode_inv, opcode_jmp, opcode_adc, opcode_ror, opcode_inv,
    opcode_bvs, opcode_adc, opcode_inv, opcode_inv, opcode_inv, opcode_adc, opcode_ror, opcode_inv,
    opcode_sei, opcode_adc, opcode_inv, opcode_inv, opcode_inv, opcode_adc, opcode_ror, opcode_inv,
    opcode_inv, opcode_sta, opcode_inv, opcode_inv, opcode_sty, opcode_sta, opcode_stx, opcode_inv,
    opcode_dey, opcode_inv, opcode_txa, opcode_inv, opcode_sty, opcode_sta, opcode_stx, opcode_inv,
    opcode_bcc, opcode_sta, opcode_inv, opcode_inv, opcode_sty, opcode_sta, opcode_stx, opcode_inv,
    opcode_tya, opcode_sta, opcode_txs, opcode_inv, opcode_inv, opcode_sta, opcode_inv, opcode_inv,
    opcode_ldy, opcode_lda, opcode_ldx, opcode_inv, opcode_ldy, opcode_lda, opcode_ldx, opcode_inv,
    opcode_tay, opcode_lda, opcode_tax, opcode_inv, opcode_ldy, opcode_lda, opcode_ldx, opcode_inv,
    opcode_bcs, opcode_lda, opcode_inv, opcode_inv, opcode_ldy, opcode_lda, opcode_ldx, opcode_inv,
    opcode_clv, opcode_lda, opcode_tsx, opcode_inv, opcode_ldy, opcode_lda, opcode_ldx, opcode_inv,
    opcode_cpy, opcode_cmp, opcode_inv, opcode_inv, opcode_cpy, opcode_cmp, opcode_dec, opcode_inv,
    opcode_iny, opcode_cmp, opcode_dex, opcode_inv, opcode_cpy, opcode_cmp, opcode_dec, opcode_inv,
    opcode_bne, opcode_cmp, opcode_inv, opcode_inv, opcode_inv, opcode_cmp, opcode_dec, opcode_inv,
    opcode_cld, opcode_cmp, opcode_inv, opcode_inv, opcode_inv, opcode_cmp, opcode_dec, opcode_inv,
    opcode_cpx, opcode_sbc, opcode_inv, opcode_inv, opcode_cpx, opcode_sbc, opcode_inc, opcode_inv,
    opcode_inx, opcode_sbc, opcode_nop, opcode_inv, opcode_cpx, opcode_sbc, opcode_inc, opcode_inv,
    opcode_beq, opcode_sbc, opcode_inv, opcode_inv, opcode_inv, opcode_sbc, opcode_inc, opcode_inv,
    opcode_sed, opcode_sbc, opcode_inv, opcode_inv, opcode_inv, opcode_sbc, opcode_inc, opcode_inv
};

void cpu_data::power_on(bus_t a_bus)
{
    m_registers.a = m_registers.x = m_registers.y = m_registers.s = 0;
    
    opcode_push_stack16(this, a_bus, 0);
    opcode_push_stack8(this, a_bus, 0);
    
    m_registers.pc = a_bus->read16(this, 0xFFFC);
    m_registers.status.raw = CPU_FLAG_INTERRUPT_DISABLE;

    m_remaining_cycles = 0;
}

void cpu_data::tick(bus_t a_bus)
{
    if (m_remaining_cycles)
    {
        m_remaining_cycles--;
        return;
    }

    uint8_t opcode_number = a_bus->read8(this, m_registers.pc);

    opcode_t opcode = &g_opcodes[opcode_number >> 4][opcode_number & 0xF];

    g_opcode_handlers[opcode_number](this, a_bus, opcode);

    m_registers.pc += opcode->length;

    m_remaining_cycles += (opcode->cycles - 1);
}