#define MAPPER_IMPL
#include "mapper.h"
#include "hw_types.h"

static mapper_return_t (*s_mapper_probe[])(ines_header_t) = 
{
#define X(name, id) name##_probe_ines,
MAPPERS
#undef X
};

static mapper_return_t (*s_mapper_map[])(ines_header_t, bus_t, bus_device_t) = 
{
#define X(name, id) name##_map_ines,
MAPPERS
#undef X
};

static const int s_mapper_ids[] = 
{
#define X(name, id) id,
MAPPERS
#undef X
};

static const char *s_mapper_names[] = 
{
#define X(name, id) #name,
MAPPERS
#undef X
};

mapper_return_t mapper_map_ines(ines_header_t a_ines_hdr, bus_t a_bus, bus_device_t a_ppu)
{
    if (a_ines_hdr->m_magic[0] != 'N' || a_ines_hdr->m_magic[1] != 'E' || a_ines_hdr->m_magic[2] != 'S' || a_ines_hdr->m_magic[3] != 0x1A)
    {
        return MAPPER_INES_HEADER_INVALID;
    }

    mapper_return_t retval = MAPPER_UNSUPPORTED;
    for (unsigned i = 0; i < (sizeof(s_mapper_probe) / sizeof(s_mapper_probe[0])); i++)
    {
        // Extract mapper number from flags 6 and 7
        uint8_t mapper = (a_ines_hdr->m_flags_6 >> 4) | (a_ines_hdr->m_flags_7 & 0xF0);

        if (mapper != s_mapper_ids[i])
        {
            continue;
        }

        // Check if the mapper is supported
        if (s_mapper_probe[i](a_ines_hdr) == MAPPER_OK)
        {
            retval = s_mapper_map[i](a_ines_hdr, a_bus, a_ppu);
            break;
        }
    }

    return retval;
}
