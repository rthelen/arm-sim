/*
 * This file is part of arm-sim: http://madscientistroom.org/arm-sim
 *
 * Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
 * reversed. (See the file COPYRIGHT for details.)
 */

#include "sim.h"
#include "arm.h"

/*
 * arm.c
 *
 * This file implements the ARM registers and instruction execution engine.
 */

reg r[NUM_REGS];

reg arm_get_reg(int reg_num)
{
    ASSERT(reg_num < NUM_REGS);
    ASSERT(reg_num >= 0);

    return r[reg_num];
}


/*
 * arm_set_reg()
 *
 * This will set an ARM processor register to some value.
 *
 * NOTE: This routine should only be called by the undo/redo logic.  All
 * processor instruction execution logic calls the undo routines to set a
 * register.  That way, the modifications are recorded.
 */

void arm_set_reg(int reg_num, reg val)
{
    ASSERT(reg_num < NUM_REGS);
    ASSERT(reg_num >= 0);

    r[reg_num] = val;
}

void arm_dump_registers(void)
{
    for (int i = 0; i < 16; i++) {
        printf("%5s: %8.8x", regs[i], arm_get_reg(i));
        if ((i & 3) == 3) printf("\n");
        else              printf("   ");
    }
    printf("Flags: ");
    reg flags = arm_get_reg(FLAGS);
    if (flags & N) printf("N");
    else           printf("n");
    if (flags & C) printf("C");
    else           printf("c");
    if (flags & V) printf("V");
    else           printf("v");
    if (flags & Z) printf("Z");
    else           printf("z");
    printf("\n");
    forth_backtrace();
    forth_show_stack();

}
