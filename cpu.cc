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

typedef struct opcode_data const *opcode_t;

typedef void (* const opcode_handler_t)(cpu_t, bus_t, opcode_t);

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
    CPU_ADDRESSING_MODE_INDIRECT, // (ind)
    CPU_ADDRESSING_MODE_INDEXED_INDIRECT, // X,ind	X-indexed, indirect	
    CPU_ADDRESSING_MODE_INDIRECT_INDEXED // ind,Y-indexed, indirect
};

static struct opcode_data {
    const char mnemonic[4];
    uint8_t length;
    uint8_t cycles;
    cpu_addressing_mode mode;
} const g_opcodes[0x10][0x10] =
{
    { // 0
        {"BRK", 1, 7, CPU_ADDRESSING_MODE_IMPLIED},
        {"ORA", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"JAM", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // JAM (KIL, HLT)
        {"SLO", 2, 8, CPU_ADDRESSING_MODE_INDEXED_INDIRECT}, // SLO
        {"NOP", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE}, // NOP
        {"ORA", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"ASL", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"SLO", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE}, // SLO zero page
        {"PHP", 1, 3, CPU_ADDRESSING_MODE_IMPLIED},
        {"ORA", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"ASL", 1, 2, CPU_ADDRESSING_MODE_ACCUMULATOR},
        {"ANC", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE}, // ANC
        {"NOP", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE}, // NOP
        {"ORA", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"ASL", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"SLO", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE} // SLO absolute
    },
    { // 1
        {"BPL", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"ORA", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"JAM", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // JAM (KIL, HLT)
        {"SLO", 2, 8, CPU_ADDRESSING_MODE_INDIRECT_INDEXED}, // SLO
        {"NOP", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X}, // NOP
        {"ORA", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"ASL", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"SLO", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X}, // SLO ZPX
        {"CLC", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"ORA", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"NOP", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // NOP
        {"SLO", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"NOP", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X}, // NOP
        {"ORA", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"ASL", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"SLO", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X}
    }, 
    { // 2
        {"JSR", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"AND", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"JAM", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // JAM (KIL, HLT)
        {"RLA", 2, 8, CPU_ADDRESSING_MODE_INDEXED_INDIRECT}, // RLA
        {"BIT", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"AND", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"ROL", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"RLA", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"PLP", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"AND", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"ROL", 1, 2, CPU_ADDRESSING_MODE_ACCUMULATOR},
        {"ANC", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE}, // ANC2
        {"BIT", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"AND", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"ROL", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"RLA", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE}
    },
    { // 3
        {"BMI", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"AND", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"JAM", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // JAM (KIL, HLT)
        {"RAL", 2, 8, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"NOP", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X}, // NOP
        {"AND", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"ROL", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"RLA", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"SEC", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"AND", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"NOP", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // NOP
        {"RLA", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"NOP", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X}, // NOP
        {"AND", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"ROL", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"RLA", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X}
    },
    { // 4
        {"RTI", 1, 6, CPU_ADDRESSING_MODE_IMPLIED},
        {"EOR", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"JAM", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // JAM (KIL, HLT)
        {"SRE", 2, 8, CPU_ADDRESSING_MODE_INDEXED_INDIRECT}, // SRE
        {"NOP", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE}, // NOP
        {"EOR", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"LSR", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"SRE", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE}, // SRE
        {"PHA", 1, 3, CPU_ADDRESSING_MODE_IMPLIED},
        {"EOR", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"LSR", 1, 2, CPU_ADDRESSING_MODE_ACCUMULATOR},
        {"ALR", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE}, // ALR
        {"JMP", 3, 3, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"EOR", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"LSR", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"SRE", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE}
    },
    { // 5
        {"BVC", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"EOR", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"JAM", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // JAM (KIL, HLT)
        {"SRE", 2, 8, CPU_ADDRESSING_MODE_INDIRECT_INDEXED}, // SRE
        {"NOP", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X}, // NOP
        {"EOR", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"LSR", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"SRE", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X}, // SRE
        {"CLI", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"EOR", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"NOP", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // NOP
        {"RLA", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"NOP", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X}, // NOP
        {"EOR", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"LSR", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"SRE", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X}
    },
    { // 6
        {"RTS", 1, 6, CPU_ADDRESSING_MODE_IMPLIED},
        {"ADC", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"JAM", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // JAM (KIL, HLT)
        {"RRA", 2, 8, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"NOP", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE}, // NOP
        {"ADC", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"ROR", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"RRA", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"PLA", 1, 4, CPU_ADDRESSING_MODE_IMPLIED},
        {"ADC", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"ROR", 1, 2, CPU_ADDRESSING_MODE_ACCUMULATOR},
        {"ARR", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE}, // ARR
        {"JMP", 3, 5, CPU_ADDRESSING_MODE_INDIRECT},
        {"ADC", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"ROR", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"RRA", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE}
    },
    { // 7
        {"BVS", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"ADC", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"JAM", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // JAM (KIL, HLT)
        {"RRA", 2, 8, CPU_ADDRESSING_MODE_INDIRECT_INDEXED}, // RRA
        {"NOP", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X}, // NOP
        {"ADC", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"ROR", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"RRA", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X}, // RRA
        {"SEI", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"ADC", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"NOP", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // NOP
        {"RRA", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"NOP", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X}, // NOP
        {"ADC", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"ROR", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"RRA", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X}
    },
    { // 8
        {"NOP", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE}, // NOP
        {"STA", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"NOP", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE}, // NOP
        {"SAX", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"STY", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"STA", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"STX", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"SAX", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"DEY", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"NOP", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE}, // NOP
        {"TXA", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"ANE", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE}, // ANE
        {"STY", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"STA", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"STX", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"SAX", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE}
    },
    { // 9
        {"BCC", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"STA", 2, 6, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"JAM", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // JAM (KIL, HLT)
        {"SHA", 2, 6, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"STY", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"STA", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"STX", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_Y},
        {"SAX", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_Y},
        {"TYA", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"STA", 3, 5, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"TXS", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"TAS", 3, 5, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"SHY", 3, 5, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"STA", 3, 5, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"SHX", 3, 5, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"SHA", 3, 5, CPU_ADDRESSING_MODE_ABSOLUTE_Y}
    },
    { // A
        {"LDY", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"LDA", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"LDX", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"LAX", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT}, // LAX
        {"LDY", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"LDA", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"LDX", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"LAX", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"TAY", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"LDA", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"TAX", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"LXA", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE}, // LXA
        {"LDY", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"LDA", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"LDX", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"LAX", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE}
    },
    { // B
        {"BCS", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"LDA", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"JAM", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // JAM (KIL, HLT)
        {"LAX", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"LDY", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"LDA", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"LDX", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_Y},
        {"LAX", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_Y},
        {"CLV", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"LDA", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"TSX", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"LAS", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"LDY", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"LDA", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"LDX", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"LAX", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y}
    },
    { // C
        {"CPY", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"CMP", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"NOP", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE}, // NOP
        {"DCP", 2, 8, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"CPY", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"CMP", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"DEC", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"DCP", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"INY", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"CMP", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"DEX", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"SBX", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE}, // SBX
        {"CPY", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"CMP", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"DEC", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"DCP", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE}
    },
    { // D
        {"BNE", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"CMP", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"JAM", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // JAM (KIL, HLT)
        {"DCP", 2, 8, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"NOP", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X}, // NOP
        {"CMP", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"DEC", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"DCP", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"CLD", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"CMP", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"NOP", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // NOP
        {"DCP", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_Y}, // DCP
        {"NOP", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X}, // NOP
        {"CMP", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"DEC", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"DCP", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X}
    },
    { // E
        {"CPX", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"SBC", 2, 6, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"NOP", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE}, // NOP
        {"ISC", 2, 8, CPU_ADDRESSING_MODE_INDEXED_INDIRECT},
        {"CPX", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"SBC", 2, 3, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"INC", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"ISC", 2, 5, CPU_ADDRESSING_MODE_ZERO_PAGE},
        {"INX", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"SBC", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE},
        {"NOP", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"SBC", 2, 2, CPU_ADDRESSING_MODE_IMMEDIATE}, /*SBC*/
        {"CPX", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"SBC", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"INC", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE},
        {"ISC", 3, 6, CPU_ADDRESSING_MODE_ABSOLUTE}
    },
    { // F
        {"BEQ", 2, 2, CPU_ADDRESSING_MODE_RELATIVE},
        {"SBC", 2, 5, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"JAM", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // JAM (KIL, HLT)
        {"ISC", 2, 8, CPU_ADDRESSING_MODE_INDIRECT_INDEXED},
        {"NOP", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X}, // NOP
        {"SBC", 2, 4, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"INC", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"ISC", 2, 6, CPU_ADDRESSING_MODE_ZERO_PAGE_X},
        {"SED", 1, 2, CPU_ADDRESSING_MODE_IMPLIED},
        {"SBC", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"NOP", 1, 2, CPU_ADDRESSING_MODE_IMPLIED}, // NOP
        {"ISC", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_Y},
        {"NOP", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X}, // NOP
        {"SBC", 3, 4, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"INC", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X},
        {"ISC", 3, 7, CPU_ADDRESSING_MODE_ABSOLUTE_X}
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
        value = a_bus->read8(a_cpu->m_registers.pc + 1);
    }
    break;
    case CPU_ADDRESSING_MODE_ZERO_PAGE:
    {
        value = a_bus->read8(a_cpu->m_registers.pc + 1);
        value = a_bus->read8(value);
    }
    break;
    case CPU_ADDRESSING_MODE_ZERO_PAGE_X:
    {
        value = a_bus->read8(a_cpu->m_registers.pc + 1);
        value = (value + a_cpu->m_registers.x) & 0xFF;
        value = a_bus->read8(value);
    }
    break;
    case CPU_ADDRESSING_MODE_ZERO_PAGE_Y:
    {
        value = a_bus->read8(a_cpu->m_registers.pc + 1);
        value = (value + a_cpu->m_registers.y) & 0xFF;
        value = a_bus->read8(value);
    }
    break;
    case CPU_ADDRESSING_MODE_ABSOLUTE:
    {
        uint16_t addr = a_bus->read16(a_cpu->m_registers.pc + 1);
        value = a_bus->read8(addr);
    }
    break;
    case CPU_ADDRESSING_MODE_ABSOLUTE_X:
    {
        uint16_t addr = a_bus->read16(a_cpu->m_registers.pc + 1);
        value = a_bus->read8(addr + a_cpu->m_registers.x);

        // Add one cycle if page boundary is crossed
        if ((addr & 0xFF00) != ((addr + a_cpu->m_registers.x) & 0xFF00))
        {
            a_cpu->m_remaining_cycles++;
        }
    }
    break;
    case CPU_ADDRESSING_MODE_ABSOLUTE_Y:
    {
        uint16_t addr = a_bus->read16(a_cpu->m_registers.pc + 1);
        value = a_bus->read8(addr + a_cpu->m_registers.y);

        // Add one cycle if page boundary is crossed
        if ((addr & 0xFF00) != ((addr + a_cpu->m_registers.y) & 0xFF00))
        {
            a_cpu->m_remaining_cycles++;
        }
    }
    break;
    case CPU_ADDRESSING_MODE_INDEXED_INDIRECT:
    {
        uint8_t zp_addr = a_bus->read8(a_cpu->m_registers.pc + 1);
        uint16_t effective_addr = (a_bus->read8((zp_addr + a_cpu->m_registers.x + 1) & 0xFF) << 8) | a_bus->read8((zp_addr + a_cpu->m_registers.x) & 0xFF);
        value = a_bus->read8(effective_addr);
    }
    break;
    case CPU_ADDRESSING_MODE_INDIRECT_INDEXED:
    {
        uint16_t zp_addr = a_bus->read8(a_cpu->m_registers.pc + 1);
        uint16_t effective_address = ((a_bus->read8((zp_addr + 1) & 0xFF) << 8) | a_bus->read8(zp_addr)) + a_cpu->m_registers.y;
    
        // Add one cycle if page boundary is crossed
        if ((effective_address & 0xFF00) != (effective_address & 0xFF00))
        {
            a_cpu->m_remaining_cycles++;
        }
    
        value = a_bus->read8(effective_address);
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
        uint8_t addr = a_bus->read8(a_cpu->m_registers.pc + 1);
        a_bus->write8(addr, a_value);
    }
    break;
    case CPU_ADDRESSING_MODE_ZERO_PAGE_X:
    {
        uint8_t addr = a_bus->read8(a_cpu->m_registers.pc + 1);
        addr = (addr + a_cpu->m_registers.x) & 0xFF;
        a_bus->write8(addr, a_value);
    }
    break;
    case CPU_ADDRESSING_MODE_ZERO_PAGE_Y:
    {
        uint8_t addr = a_bus->read8(a_cpu->m_registers.pc + 1);
        addr = (addr + a_cpu->m_registers.y) & 0xFF;
        a_bus->write8(addr, a_value);
    }
    break;
    case CPU_ADDRESSING_MODE_ABSOLUTE:
    {
        uint16_t addr = a_bus->read16(a_cpu->m_registers.pc + 1);
        a_bus->write8(addr, a_value);
    }
    break;
    case CPU_ADDRESSING_MODE_ABSOLUTE_X:
    {
        uint16_t addr = a_bus->read16(a_cpu->m_registers.pc + 1);
        addr = addr + a_cpu->m_registers.x;
        a_bus->write8(addr, a_value);
    }
    break;
    case CPU_ADDRESSING_MODE_ABSOLUTE_Y:
    {
        uint16_t addr = a_bus->read16(a_cpu->m_registers.pc + 1);
        addr = addr + a_cpu->m_registers.y;
        a_bus->write8(addr, a_value);
    }
    break;
    case CPU_ADDRESSING_MODE_INDEXED_INDIRECT:
    {
        uint8_t zp_addr = a_bus->read8(a_cpu->m_registers.pc + 1);
        uint16_t effective_addr = a_bus->read8((zp_addr + a_cpu->m_registers.x) & 0xFF);
        effective_addr |= (a_bus->read8((zp_addr + a_cpu->m_registers.x + 1) & 0xFF) << 8);
        a_bus->write8(effective_addr, a_value);
    }
    break;
    case CPU_ADDRESSING_MODE_INDIRECT_INDEXED:
    {
        uint16_t zp_addr = a_bus->read8(a_cpu->m_registers.pc + 1);
        uint16_t effective_address = ((a_bus->read8((zp_addr + 1) & 0xFF) << 8) | a_bus->read8(zp_addr)) + a_cpu->m_registers.y;    
        a_bus->write8(effective_address, a_value);
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
    
    uint8_t offset = a_bus->read8(a_cpu->m_registers.pc + 1);

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
    a_bus->write8(0x0100 + a_cpu->m_registers.s, a_value);
    a_cpu->m_registers.s--;
}

static void opcode_push_stack16(cpu_t a_cpu, bus_t a_bus, uint16_t a_value)
{
    // Push high byte first
    a_bus->write8(0x0100 + a_cpu->m_registers.s, a_value >> 8);
    a_cpu->m_registers.s--;

    // Push low byte next
    a_bus->write8(0x0100 + a_cpu->m_registers.s, a_value & 0xFF);
    a_cpu->m_registers.s--;
}

static uint8_t opcode_pop_stack8(cpu_t a_cpu, bus_t a_bus)
{
    a_cpu->m_registers.s++;
    
    uint8_t value = a_bus->read8(0x0100 + a_cpu->m_registers.s);
    
    return value;
}

static uint16_t opcode_pop_stack16(cpu_t a_cpu, bus_t a_bus)
{
    a_cpu->m_registers.s++;
    
    uint16_t value = a_bus->read8(0x0100 + a_cpu->m_registers.s);

    a_cpu->m_registers.s++;
    
    value |= ((uint16_t)a_bus->read8(0x0100 + a_cpu->m_registers.s)) << 8;

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

static void opcode_rra(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // Read the value from memory
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    // Perform the ROR operation (rotate right)
    uint8_t carry = a_cpu->m_registers.status.flag.c; // Save the current Carry flag
    a_cpu->m_registers.status.flag.c = value & 0x01;  // Update Carry flag with LSB of the value
    value = (carry << 7) | (value >> 1);              // Rotate right

    // Write the rotated value back to memory
    opcode_write8(a_cpu, a_bus, a_opcode, value);

    // Perform the ADC operation (accumulator += rotated value + carry)
    uint16_t result = a_cpu->m_registers.a + value + a_cpu->m_registers.status.flag.c;

    // Update the Carry flag (C) if the result exceeds 0xFF
    a_cpu->m_registers.status.flag.c = result > 0xFF;

    // Update the Zero flag (Z) if the result is 0
    a_cpu->m_registers.status.flag.z = (result & 0xFF) == 0;

    // Update the Overflow flag (V) if there is a signed overflow
    a_cpu->m_registers.status.flag.v = !!(~(a_cpu->m_registers.a ^ value) & (a_cpu->m_registers.a ^ result) & 0x80);

    // Update the Negative flag (N) if the MSB of the result is 1
    a_cpu->m_registers.status.flag.n = (result & 0x80) != 0;

    // Store the result in the accumulator
    a_cpu->m_registers.a = result & 0xFF;
}

static void opcode_and(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    a_cpu->m_registers.a &= value;

    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.a == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.a >> 7);
}

static void opcode_rla(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // Read the value from memory
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    // Save the current Carry flag
    uint8_t carry = a_cpu->m_registers.status.flag.c; 

    // Update Carry flag with MSB of the value
    a_cpu->m_registers.status.flag.c = value >> 7; 
    
    // Rotate left
    value = (value << 1) | carry;

    // Write the rotated value back to memory
    opcode_write8(a_cpu, a_bus, a_opcode, value);

    // Perform the AND operation (accumulator &= rotated value)
    a_cpu->m_registers.a &= value;

    // Update the Zero flag (Z) if the accumulator is 0
    a_cpu->m_registers.status.flag.z = (a_cpu->m_registers.a == 0);

    // Update the Negative flag (N) if the MSB of the accumulator is 1
    a_cpu->m_registers.status.flag.n = a_cpu->m_registers.a >> 7;
}

static void opcode_asl(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    a_cpu->m_registers.status.flag.c = value >> 7;
    
    value <<= 1;

    opcode_write8(a_cpu, a_bus, a_opcode, value);

    a_cpu->m_registers.status.flag.z = value == 0;
    a_cpu->m_registers.status.flag.n = value >> 7;
}

static void opcode_anc(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    a_cpu->m_registers.a &= value;
    
    a_cpu->m_registers.status.flag.c = a_cpu->m_registers.a >> 7;
    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.a == 0;
    a_cpu->m_registers.status.flag.n = a_cpu->m_registers.a >> 7;
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
    // Push PC+2 onto the stack
    opcode_push_stack16(a_cpu, a_bus, a_cpu->m_registers.pc + 2);

    // Push status with B flag set
    opcode_push_stack8(a_cpu, a_bus, a_cpu->m_registers.status.raw | CPU_FLAG_BREAK | CPU_FLAG_UNUSED);

    // Set interrupt disable flag
    a_cpu->m_registers.status.flag.i = 1;

    // Compensate for the automatic PC increment in cpu_data::tick
    a_cpu->m_registers.pc = a_bus->read16(0xFFFE) - a_opcode->length;
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

static void opcode_dcp(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // Read the value from memory
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    // Decrement the value
    value--;

    // Write the decremented value back to memory
    opcode_write8(a_cpu, a_bus, a_opcode, value);

    // Compare the decremented value with the accumulator
    uint8_t result = a_cpu->m_registers.a - value;

    // Update the Carry flag (C) if A >= decremented value
    a_cpu->m_registers.status.flag.c = a_cpu->m_registers.a >= value;

    // Update the Zero flag (Z) if A == decremented value
    a_cpu->m_registers.status.flag.z = (result == 0);

    // Update the Negative flag (N) if the MSB of the result is 1
    a_cpu->m_registers.status.flag.n = (result & 0x80) != 0;
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

static void opcode_sbx(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
   // Read the immediate value
   uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

   // Perform (A & X) - value
   uint8_t and_result = a_cpu->m_registers.a & a_cpu->m_registers.x;
   uint8_t result = and_result - value;

   // Set the Carry flag if no borrow occurred
   a_cpu->m_registers.status.flag.c = and_result >= value;

   // Set the Zero flag if the result is 0
   a_cpu->m_registers.status.flag.z = (result == 0);

   // Set the Negative flag if the MSB of the result is 1
   a_cpu->m_registers.status.flag.n = (result & 0x80) != 0;

   // Store the result in the X register
   a_cpu->m_registers.x = result;
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

static void opcode_sre(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
  // Read the value from memory
  uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

  // Set the Carry flag to the LSB of the original value
  a_cpu->m_registers.status.flag.c = value & 0x01;

  // Perform the LSR operation (logical shift right)
  value >>= 1;

  // Write the shifted value back to memory
  opcode_write8(a_cpu, a_bus, a_opcode, value);

  // Perform the EOR operation (accumulator ^= shifted value)
  a_cpu->m_registers.a ^= value;

  // Update the Zero flag (Z) if the accumulator is 0
  a_cpu->m_registers.status.flag.z = (a_cpu->m_registers.a == 0);

  // Update the Negative flag (N) if the MSB of the accumulator is 1
  a_cpu->m_registers.status.flag.n = (a_cpu->m_registers.a >> 7) & 0x01;
}

static void opcode_alr(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    a_cpu->m_registers.a &= value;

    a_cpu->m_registers.status.flag.c = a_cpu->m_registers.a & 0x01;

    a_cpu->m_registers.a >>= 1;

    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.a == 0;
    
    a_cpu->m_registers.status.flag.n = a_cpu->m_registers.a >> 7;
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
        uint16_t addr = a_bus->read16(a_cpu->m_registers.pc + 1);
        a_cpu->m_registers.pc = addr - a_opcode->length;
    }
    else /* if (a_opcode->mode == CPU_ADDRESSING_MODE_INDIRECT) */
    {
        uint16_t addr = a_bus->read16(a_cpu->m_registers.pc + 1);
        
        // Simulate 6502 JMP indirect bug: if address is $xxFF, fetch second byte from $xx00, not $xx+1:00
        if ((addr & 0xFF) == 0xFF) 
        {
            uint8_t lo = a_bus->read8(addr);
            uint8_t hi = a_bus->read8(addr & 0xFF00); // Wrap to same page
            a_cpu->m_registers.pc = ((hi << 8) | lo) - a_opcode->length;
        } 
        else 
        {
            a_cpu->m_registers.pc = a_bus->read16(addr) - a_opcode->length;
        }
    }
}

static void opcode_jsr(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint16_t addr = a_bus->read16(a_cpu->m_registers.pc + 1) - a_opcode->length;
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

static void opcode_lax(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // Read the value from memory
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    // Load the value into both A and X registers
    a_cpu->m_registers.a = value;
    a_cpu->m_registers.x = value;

    // Update the Zero flag (Z) if the value is 0
    a_cpu->m_registers.status.flag.z = (value == 0);

    // Update the Negative flag (N) if the MSB of the value is 1
    a_cpu->m_registers.status.flag.n = (value >> 7) & 0x01;    
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

static void opcode_slo(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // Read the value from memory
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    // Set the Carry flag based on the MSB of the original value
    a_cpu->m_registers.status.flag.c = (value >> 7) & 0x01;

    // Perform the ASL operation (shift left)
    value <<= 1;

    // Write the shifted value back to memory
    opcode_write8(a_cpu, a_bus, a_opcode, value);

    // Perform the ORA operation (accumulator |= shifted value)
    a_cpu->m_registers.a |= value;

    // Update the Zero flag (Z) if the accumulator is 0
    a_cpu->m_registers.status.flag.z = (a_cpu->m_registers.a == 0);

    // Update the Negative flag (N) if the MSB of the accumulator is 1
    a_cpu->m_registers.status.flag.n = (a_cpu->m_registers.a >> 7) & 0x01;
}

static void opcode_pha(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    opcode_push_stack8(a_cpu, a_bus, a_cpu->m_registers.a);
}

static void opcode_php(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // When pushing status to stack via PHP, both B flag and unused flag should be set
    opcode_push_stack8(a_cpu, a_bus, a_cpu->m_registers.status.raw | CPU_FLAG_BREAK | CPU_FLAG_UNUSED);
}

static void opcode_pla(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.a = opcode_pop_stack8(a_cpu, a_bus);

    // Set Z and N flags
    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.a == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.a >> 7);
}

static void opcode_plp(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // When pulling status via PLP, B flag is ignored and kept as 0, unused flag is kept as 1
    a_cpu->m_registers.status.raw = opcode_pop_stack8(a_cpu, a_bus);
}

static void opcode_rol(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    uint8_t carry = a_cpu->m_registers.status.flag.c;

    a_cpu->m_registers.status.flag.c = value >> 7;
    
    value = (value << 1) | carry;

    opcode_write8(a_cpu, a_bus, a_opcode, value);

    // Set Z and N flags
    a_cpu->m_registers.status.flag.z = value == 0;
    a_cpu->m_registers.status.flag.n = value >> 7;
}

static void opcode_ror(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    uint8_t carry = a_cpu->m_registers.status.flag.c;

    a_cpu->m_registers.status.flag.c = value & 0x01;
    
    value = (carry << 7) | (value >> 1);
    
    opcode_write8(a_cpu, a_bus, a_opcode, value);

    // Set Z and N flags
    a_cpu->m_registers.status.flag.z = value == 0;
    a_cpu->m_registers.status.flag.n = value >> 7;    
}

static void opcode_arr(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // Read the immediate value
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    // Perform AND operation with the accumulator
    uint8_t and_result = a_cpu->m_registers.a & value;

    // Perform rotate right (ROR) operation
    uint8_t carry = a_cpu->m_registers.status.flag.c;

    a_cpu->m_registers.a = (carry << 7) | (and_result >> 1);

    // Carry flag: Set to bit 6 of the rotated result
    a_cpu->m_registers.status.flag.c = (a_cpu->m_registers.a >> 6) & 0x01;

    // Overflow flag: Set to bit 5 XOR bit 6 of the rotated result
    a_cpu->m_registers.status.flag.v = ((a_cpu->m_registers.a >> 6) & 0x01) ^ ((a_cpu->m_registers.a >> 5) & 0x01);

    // Set the Zero flag if the result is 0
    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.a == 0;

    // Set the Negative flag if the MSB of the result is 1
    a_cpu->m_registers.status.flag.n = a_cpu->m_registers.a >> 7;
}

static void opcode_rti(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // When pulling status via RTI, B flag is ignored and kept as 0, unused flag is kept as 1
    uint8_t status = opcode_pop_stack8(a_cpu, a_bus);
    a_cpu->m_registers.status.raw = (status & ~CPU_FLAG_BREAK) | CPU_FLAG_UNUSED;
    a_cpu->m_registers.pc = opcode_pop_stack16(a_cpu, a_bus) - a_opcode->length;
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
    // Correct overflow flag calculation for SBC
    a_cpu->m_registers.status.flag.v = !!((a_cpu->m_registers.a ^ value) & (a_cpu->m_registers.a ^ result) & 0x80);
    a_cpu->m_registers.status.flag.n = !!(result & 0x80);

    a_cpu->m_registers.a = result & 0xFF;
}

static void opcode_isc(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // Read the value from memory
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    // Increment the value
    value++;

    // Write the incremented value back to memory
    opcode_write8(a_cpu, a_bus, a_opcode, value);

    // Perform the SBC operation (accumulator -= incremented value + carry)
    uint16_t result = a_cpu->m_registers.a - value - (!a_cpu->m_registers.status.flag.c);

    // Update the Carry flag (C) if no borrow occurred
    a_cpu->m_registers.status.flag.c = !(result > 0xFF);

    // Update the Zero flag (Z) if the result is 0
    a_cpu->m_registers.status.flag.z = (result & 0xFF) == 0;

    // Update the Overflow flag (V) if there is a signed overflow
    a_cpu->m_registers.status.flag.v = !!((a_cpu->m_registers.a ^ value) & (a_cpu->m_registers.a ^ result) & 0x80);

    // Update the Negative flag (N) if the MSB of the result is 1
    a_cpu->m_registers.status.flag.n = (result & 0x80) != 0;

    // Store the result in the accumulator
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

static void opcode_sax(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // Perform A & X
    uint8_t result = a_cpu->m_registers.a & a_cpu->m_registers.x;

    // Write the result to the target memory address
    opcode_write8(a_cpu, a_bus, a_opcode, result);
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

static void opcode_lxa(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    uint8_t value = opcode_read8(a_cpu, a_bus, a_opcode);

    a_cpu->m_registers.a = value;
    a_cpu->m_registers.x = value;

    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.a == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.a >> 7);
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

static void opcode_shy(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // Compute the target address based on the addressing mode
    uint16_t base_addr = a_bus->read16(a_cpu->m_registers.pc + 1);
    uint16_t addr = base_addr + a_cpu->m_registers.x;

    // Compute the value to store: Y & (high byte of the target address + 1)
    uint8_t value = a_cpu->m_registers.y & (((addr >> 8) + 1) & 0xFF);

    // Emulate the "unstable" behavior: drop the value if crossing a page boundary
    if ((base_addr & 0xFF00) != (addr & 0xFF00))
    {
        return; // Do nothing if a page boundary is crossed
    }

    // Write the result to the target memory address
    a_bus->write8(addr, value);
}

static void opcode_shx(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // Compute the target address based on the addressing mode
    uint16_t base_addr = a_bus->read16(a_cpu->m_registers.pc + 1);
    uint16_t addr = base_addr + a_cpu->m_registers.y;

    // Compute the value to store: X & (high byte of the target address + 1)
    uint8_t value = a_cpu->m_registers.x & (((addr >> 8) + 1) & 0xFF);

    // Emulate the "unstable" behavior: drop the value if crossing a page boundary
    if ((base_addr & 0xFF00) != (addr & 0xFF00))
    {
        return; // Do nothing if a page boundary is crossed
    }

    // Write the result to the target memory address
    a_bus->write8(addr, value);
}

static void opcode_sha(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // Read the base address from the opcode (indirect addressing)
    uint16_t base_addr = a_bus->read8(a_cpu->m_registers.pc + 1);

    // Read the effective address from the base address
    uint16_t addr = a_bus->read16(base_addr);

    // Add the Y register to the effective address (indirect,Y addressing mode)
    addr += a_cpu->m_registers.y;

    // Perform (A & X) & (high byte of the target address + 1)
    uint8_t value = (a_cpu->m_registers.a & a_cpu->m_registers.x) & (((addr >> 8) + 1) & 0xFF);

    // Write the result to the target memory address
    a_bus->write8(addr, value);
}

static void opcode_tya(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    a_cpu->m_registers.a = a_cpu->m_registers.y;
    a_cpu->m_registers.status.flag.z = a_cpu->m_registers.a == 0;
    a_cpu->m_registers.status.flag.n = !!(a_cpu->m_registers.a >> 7);
}

static void opcode_jam(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode)
{
    // Jam the CPU
    a_cpu->m_registers.pc = 0xFFFF; // Set PC to an invalid address
    a_cpu->m_registers.status.raw = 0; // Clear all status flags
    __asm__("int $3");
}

static void opcode_inv(cpu_t a_cpu, bus_t a_bus, opcode_t a_opcode) 
{
    // Invalid opcode
    __asm__("int $3");
}

static opcode_handler_t const g_opcode_handlers[] =
{       /*   0          1           2           3            4           5           6           7           8           9           A           B           C           D           E           F   */
/* 0 */ opcode_brk, opcode_ora, opcode_jam, opcode_slo, opcode_nop, opcode_ora, opcode_asl, opcode_slo, opcode_php, opcode_ora, opcode_asl, opcode_anc, opcode_nop, opcode_ora, opcode_asl, opcode_slo,
/* 1 */ opcode_bpl, opcode_ora, opcode_jam, opcode_slo, opcode_nop, opcode_ora, opcode_asl, opcode_slo, opcode_clc, opcode_ora, opcode_nop, opcode_slo, opcode_nop, opcode_ora, opcode_asl, opcode_slo,
/* 2 */ opcode_jsr, opcode_and, opcode_jam, opcode_rla, opcode_bit, opcode_and, opcode_rol, opcode_rla, opcode_plp, opcode_and, opcode_rol, opcode_anc, opcode_bit, opcode_and, opcode_rol, opcode_rla,
/* 3 */ opcode_bmi, opcode_and, opcode_jam, opcode_rla, opcode_nop, opcode_and, opcode_rol, opcode_rla, opcode_sec, opcode_and, opcode_nop, opcode_rla, opcode_nop, opcode_and, opcode_rol, opcode_rla,
/* 4 */ opcode_rti, opcode_eor, opcode_jam, opcode_sre, opcode_nop, opcode_eor, opcode_lsr, opcode_sre, opcode_pha, opcode_eor, opcode_lsr, opcode_alr, opcode_jmp, opcode_eor, opcode_lsr, opcode_sre,
/* 5 */ opcode_bvc, opcode_eor, opcode_jam, opcode_sre, opcode_nop, opcode_eor, opcode_lsr, opcode_sre, opcode_cli, opcode_eor, opcode_nop, opcode_sre, opcode_nop, opcode_eor, opcode_lsr, opcode_sre,
/* 6 */ opcode_rts, opcode_adc, opcode_jam, opcode_rra, opcode_nop, opcode_adc, opcode_ror, opcode_rra, opcode_pla, opcode_adc, opcode_ror, opcode_arr, opcode_jmp, opcode_adc, opcode_ror, opcode_rra,
/* 7 */ opcode_bvs, opcode_adc, opcode_jam, opcode_rra, opcode_nop, opcode_adc, opcode_ror, opcode_rra, opcode_sei, opcode_adc, opcode_nop, opcode_rra, opcode_nop, opcode_adc, opcode_ror, opcode_rra,
/* 8 */ opcode_nop, opcode_sta, opcode_nop, opcode_sax, opcode_sty, opcode_sta, opcode_stx, opcode_sax, opcode_dey, opcode_nop, opcode_txa, opcode_inv, opcode_sty, opcode_sta, opcode_stx, opcode_sax,
/* 9 */ opcode_bcc, opcode_sta, opcode_jam, opcode_sha, opcode_sty, opcode_sta, opcode_stx, opcode_sax, opcode_tya, opcode_sta, opcode_txs, opcode_inv, opcode_shy, opcode_sta, opcode_shx, opcode_sha,
/* A */ opcode_ldy, opcode_lda, opcode_ldx, opcode_lax, opcode_ldy, opcode_lda, opcode_ldx, opcode_lax, opcode_tay, opcode_lda, opcode_tax, opcode_lxa, opcode_ldy, opcode_lda, opcode_ldx, opcode_lax,
/* B */ opcode_bcs, opcode_lda, opcode_jam, opcode_lax, opcode_ldy, opcode_lda, opcode_ldx, opcode_lax, opcode_clv, opcode_lda, opcode_tsx, opcode_inv, opcode_ldy, opcode_lda, opcode_ldx, opcode_lax,
/* C */ opcode_cpy, opcode_cmp, opcode_nop, opcode_dcp, opcode_cpy, opcode_cmp, opcode_dec, opcode_dcp, opcode_iny, opcode_cmp, opcode_dex, opcode_sbx, opcode_cpy, opcode_cmp, opcode_dec, opcode_dcp,
/* D */ opcode_bne, opcode_cmp, opcode_jam, opcode_dcp, opcode_nop, opcode_cmp, opcode_dec, opcode_dcp, opcode_cld, opcode_cmp, opcode_nop, opcode_dcp, opcode_nop, opcode_cmp, opcode_dec, opcode_dcp,
/* E */ opcode_cpx, opcode_sbc, opcode_nop, opcode_isc, opcode_cpx, opcode_sbc, opcode_inc, opcode_isc, opcode_inx, opcode_sbc, opcode_nop, opcode_sbc, opcode_cpx, opcode_sbc, opcode_inc, opcode_isc,
/* F */ opcode_beq, opcode_sbc, opcode_jam, opcode_isc, opcode_nop, opcode_sbc, opcode_inc, opcode_isc, opcode_sed, opcode_sbc, opcode_nop, opcode_isc, opcode_nop, opcode_sbc, opcode_inc, opcode_isc
};

void cpu_data::power_on(bus_t a_bus)
{
    m_registers.a = m_registers.x = m_registers.y = 0;

    m_registers.s = 0xFD;
    
    m_registers.pc = a_bus->read16(0xFFFC);
    m_registers.status.raw = CPU_FLAG_INTERRUPT_DISABLE | CPU_FLAG_UNUSED;

    m_nmi = 0;

    m_remaining_cycles = 0;

    m_tickcount = 0;
}

void cpu_data::nmi()
{
    m_nmi = 1;
}

void cpu_data::stall(uint32_t a_cycles)
{
    m_remaining_cycles += a_cycles;
}

//#define DEBUG

#if defined(DEBUG)
#include <stdio.h>
#endif

void cpu_data::tick(bus_t a_bus)
{
    m_tickcount++;

    if (m_remaining_cycles)
    {
        m_remaining_cycles--;
        return;
    }

    if (m_nmi)
    {
        opcode_push_stack16(this, a_bus, m_registers.pc);
        opcode_push_stack8(this, a_bus, m_registers.status.raw | CPU_FLAG_INTERRUPT_DISABLE | CPU_FLAG_UNUSED);

        m_registers.status.flag.i = 1;
        m_registers.pc = a_bus->read16(0xFFFA);

        m_nmi = 0;

        m_remaining_cycles += 7 - 1;

        return;
    }

    // Print the address and opcode for debugging
    
    uint8_t opcode_number = a_bus->read8(m_registers.pc);
    
    opcode_t opcode = &g_opcodes[opcode_number >> 4][opcode_number & 0xF];

#if defined(DEBUG)
    printf("$%04X: %02X %s\n", m_registers.pc, opcode_number, opcode->mnemonic);
    fflush(stdout); // Force the output to be written immediately

    if (opcode_number == 0x00)
    {
        printf("ZERO OPCODE\n");
        fflush(stdout);
        asm("int $3");
    }
#endif

    g_opcode_handlers[opcode_number](this, a_bus, opcode);

    m_registers.pc += opcode->length;

    m_remaining_cycles += (opcode->cycles - 1);
}