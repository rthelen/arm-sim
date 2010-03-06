#include "sim.h"

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

void *memory_range(reg arm_addr, reg arm_size)
{
    if (arm_addr < addr_base) {
        warn("simulator address %p outside of memory range", arm_addr);
    }

    if (arm_addr + arm_size >= addr_base + addr_size) {
        warn("simulator range (%p + %d) outside of memory range", arm_addr, arm_size);
    }

    return memory + (arm_addr - addr_base);
}

static reg *mem_addr(reg arm_addr, reg arm_size)
{
    if (arm_addr & (arm_size -1)) {
            warn("Unaligned referenced: %p of size %d", arm_addr, arm_size);
            return 0;
    }

    reg *addr = memory_range(arm_addr, arm_size);

    return addr;
}

void mem_store(reg arm_addr, reg arm_offset, reg val)
{
    reg *addr = mem_addr(arm_addr + arm_offset, sizeof(reg));

    if (addr) {
        *addr = val;
    }
}

reg mem_load(reg arm_addr, reg arm_offset)
{
    reg *addr = mem_addr(arm_addr + arm_offset, sizeof(reg));

    if (addr) {
        return *addr;
    } else {
        return BAD_MEMVAL;
    }
}

void mem_storeb(reg arm_addr, reg arm_offset, byte val)
{
    reg *addr = mem_addr(arm_addr + arm_offset, 1);

    if (addr) {
        *addr = val;
    }
}

byte mem_loadb(reg arm_addr, reg arm_offset)
{
    reg *addr = mem_addr(arm_addr + arm_offset, 1);

    if (addr) {
        return *addr;
    } else {
        return (byte) BAD_MEMVAL;
    }
}

void mem_dump(reg arm_addr, reg arm_numwords)
{
    while (arm_numwords > 0) {
        printf("%8.8x: %8.8x\n", arm_addr, mem_load(arm_addr, 0));
        arm_addr += 4;
        arm_numwords -= 4;
    }
}
