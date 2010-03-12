#include "sim.h"
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

int mem_addr_is_valid(reg arm_addr)
{
    if (arm_addr < addr_base) return 0;
    if (arm_addr >= addr_base + addr_size) return 0;
    return 1;
}

int mem_range_is_valid(reg arm_addr, reg size)
{
    if (arm_addr < addr_base) return 0;
    if (arm_addr + size < addr_base) return 0;
    if (arm_addr > addr_base + addr_size) return 0;
    if (arm_addr + size >= addr_base + addr_size) return 0;
    return 1;
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
    char instr[40];
    while (arm_numwords > 0) {
        reg ir = mem_load(arm_addr, 0);
        // reg i = arm_decode_instr(ir);
        // printf("%8.8x: %8.8x - %d\n", arm_addr, ir, i);
        /*
         * Check to see if this address is the beginning of a Forth header.
         */
        reg skip = forth_is_header(arm_addr);
        if (!skip) {
            skip = forth_is_word(arm_addr);
        }
        if (!skip) {
            skip = forth_is_string(arm_addr);
        }
        if (!skip) {
            if (ir == 0) {
                printf("%8.8x: 0\n", arm_addr);
            } else {
                disassemble(arm_addr, ir, instr, sizeof(instr));
                printf("%8.8x: %8.8x %-32s\n", arm_addr, ir, instr);
            }
            skip = 1;
        }

        arm_addr += skip * 4;
        if (skip > arm_numwords) {
            arm_numwords = 0;
        } else {
            arm_numwords -= skip;
        }
    }
}
