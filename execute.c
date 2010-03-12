/*
 * execute.c
 *
 * This file has the code to execute ARM instructions.
 */

#include "sim.h"
#include "arm.h"

void init_execution(void)
{
    arm_set_reg(FLAGS, 0x0);
}

static int execute_check_conds(reg conds)
{
    reg flags = arm_get_reg(FLAGS);

#define TF(x)    ((x) ? 1 : 0)
#define CLR(x)   (!TF(x))
#define SET(x)   ( TF(x))
#define C_CLR       CLR(flags & C)
#define C_SET       SET(flags & C)
#define Z_CLR       CLR(flags & Z)
#define Z_SET       SET(flags & Z)
#define N_CLR       CLR(flags & N)
#define N_SET       SET(flags & N)
#define V_CLR       CLR(flags & V)
#define V_SET       SET(flags & V)

    switch (conds & 0xF) {
    case 0:  return Z_SET;
    case 1:  return Z_CLR;
    case 2:  return C_SET;
    case 3:  return C_CLR;
    case 4:  return N_SET;
    case 5:  return N_CLR;
    case 6:  return V_SET;
    case 7:  return V_CLR;
    case 8:  return C_SET && Z_CLR;
    case 9:  return C_CLR && Z_SET;
    case 10: return (N_SET && V_SET) || (N_CLR && V_CLR);
    case 11: return (N_SET && V_CLR) || (N_SET && V_CLR);
    case 12: return Z_CLR && ((N_SET && V_SET) || (N_CLR && V_CLR));
    case 13: return Z_SET || ((N_SET && V_CLR) || (N_CLR && V_SET));
    case 14: return 1;
    case 15:
        warn("Code using ILLEGAL condition flag 0xF");
        return 0;
    }

    printf("ERROR: Shouldn't ever get here: %s:%d", __FILE__, __LINE__);
    assert(0);
}

int execute_one(void)
{
    reg pc = arm_get_reg(PC);
    reg instr = mem_load(pc, 0);
    
    if (instr == BAD_MEMVAL) {
        return 0;
    }

    arm_instr_t op = arm_decode_instr(instr);
    reg cond = IBITS(28,4);

    /*
     * Most instructions step forward one instruction
     * Branch doesn't (necessarily) but it handles it's own case below.
     */
    undo_record_reg(PC);
    arm_set_reg(PC, pc + 4);

    switch (op) {
    case ARM_INSTR_B:
    {
        reg imm24bit = IBITS(0, 24);
        reg dest = decode_dest_addr(pc, imm24bit, 24, 0);
        if (execute_check_conds(cond)) {
            arm_set_reg(PC, dest);
        }
        break;
    }

    default:
        return 0;
    }

    return 1;
}
