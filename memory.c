/*
 * This file is part of arm-sim: http://madscientistroom.org/arm-sim
 *
 * Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
 * reversed. (See the file COPYRIGHT for details.)
 */

#include "sim.h"
#include "arm.h"

typedef struct memory_s {
    byte *memory;
    reg base, end, size;
} memory_t;

#define MAX_NUM_RANGES		5
static int num_mem_ranges;
static memory_t mem_range[MAX_NUM_RANGES];

#define WITHIN(a, s, e) (((a) >= (s)) && ((a) < (e)))

void memory_more(reg base, reg size)
{
    for (int i = 0; i < num_mem_ranges; i++) {
        memory_t *p = &mem_range[i];
        reg end = base + size;
        if (WITHIN(base, p->base, p->end) ||
            WITHIN(end,  p->base, p->end) ||
            (base <  p->base && end  >= p->end)) {
            error("Overlapping memory region: new region %8.8x - %8.8x; existing overlapping region %8.8x - %8.8x",
                  base, end, p->base, p->end);
        }
    }

    if (num_mem_ranges >= MAX_NUM_RANGES) {
        error("Out of memory ranges");
    }

    memory_t *m = &mem_range[num_mem_ranges++];

    m->memory = malloc(size);
    assert(m->memory);

    m->base = base;
    m->end  = base + size;
    m->size = size;

    bzero(m->memory, size);
}

int mem_addr_is_valid(reg arm_addr)
{
    for (int i = 0; i < num_mem_ranges; i++) {
        memory_t *p = &mem_range[i];
        if (WITHIN(arm_addr, p->base, p->end)) return 1;
    }
    return 0;
}

static int mem_range_index(reg base, reg size)
{
    reg end = base + size;
    for (int i = 0; i < num_mem_ranges; i++) {
        memory_t *p = &mem_range[i];
        if (WITHIN(base, p->base, p->end) &&
            WITHIN(end,  p->base, p->end))
            return i;
    }
    return -1;
}

int mem_range_is_valid(reg base, reg size)
{
    if (mem_range_index(base, size) == -1)
        return 0;
    return 1;
}

void *memory_range(reg base, reg size)
{
    int i = mem_range_index(base, size);
    if (i < 0) {
        warn("simulator address %p outside of memory range", base);
        return NULL;
    }

    return mem_range->memory + (base - mem_range[i].base);
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
    byte *addr = (byte *) mem_addr(arm_addr + arm_offset, 1);

    if (addr) {
        *addr = val;
    }
}

byte mem_loadb(reg arm_addr, reg arm_offset)
{
    byte *addr = (byte *) mem_addr(arm_addr + arm_offset, 1);

    if (addr) {
        return *addr;
    } else {
        return (byte) BAD_MEMVAL;
    }
}

void mem_dump(reg arm_addr, reg arm_numwords)
{
    char instr[240];
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
