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

