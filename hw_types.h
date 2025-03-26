#pragma once

#include <stdint.h>

#define PAGE_SIZE 0x100
#define PAGE_MASK (PAGE_SIZE - 1)
#define PAGE_SHIFT 8
#define BUS_PAGES (0x100)

typedef struct cpu_data *cpu_t;

typedef struct bus_data *bus_t;

typedef struct bus_device_data *bus_device_t;

typedef struct bus_device_ops_data *bus_device_ops_t;