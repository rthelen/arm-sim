#include "arm.h"

byte *memory;

reg addr_base;
reg addr_size;

void init_memory(reg base, reg size)
{
    memory = malloc(size);
    assert(memory);

    addr_base = base;
    addr_size = size;

    bzero(memory, size);
}

void *memory_addr(reg arm_addr, reg arm_size)
{
    if (arm_addr < addr_base) {
        warn("simulator address %p outside of memory range", arm_addr);
    }

    if (arm_addr + arm_size >= addr_base + addr_size) {
        warn("simulator range (%p + %d) outside of memory range", arm_addr, arm_size);
    }

    return memory + (arm_addr - addr_base);
}
