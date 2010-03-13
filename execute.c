/*
 * execute.c
 *
 * This file has the code to execute ARM instructions.
 */

#include "sim.h"
#include "arm.h"

#define TF(x)    ((x) ? 1 : 0)
#define CLR(x)   (!TF(x))
#define SET(x)   ( TF(x))

void init_execution(void)
{
    arm_set_reg(FLAGS, 0x0);
}

static reg barrel_shifter(reg base, reg shift_type, reg shift)
{
    switch (shift_type) {
    case 0: base <<= shift;  break;
    case 1: base >>= shift;  break;
    case 2: base = (signed) base >> shift; break;
    case 3: base = (base >> shift) | (base << (32 - shift)); break;
    }

    return base;
}

static int execute_check_conds(reg conds)
{
    reg flags = arm_get_reg(FLAGS);

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

reg pre_inc(reg addr, int pre_post, int up_down)
{
    if (!pre_post) return addr;
    if (up_down)   return addr + 4;
    else           return addr - 4;
}

reg post_inc(reg addr, int pre_post, int up_down)
{
    if (pre_post)  return addr;
    if (up_down)   return addr + 4;
    else           return addr - 4;
}

int execute_one(void)
{
    reg pc = arm_get_reg(PC);
    reg instr = mem_load(pc, 0);
    
    if (instr == BAD_MEMVAL) {
        return 0;
    }

    arm_instr_t op = arm_decode_instr(instr);
    reg cond = IBITS(28, 4);
    reg rm = IBITS(0, 4);
//    reg rs = IBITS(8, 4);
    reg rd = IBITS(12, 4);
    reg rn = IBITS(16, 4);
    reg up_down = IBIT(23);
    reg pre_post = IBIT(24);
    reg write_back = IBIT(21);
    reg imm12bit = IBITS(0, 12);
    reg imm5shift = IBITS(7, 5);
    reg shift_type = IBITS(5, 2);
    reg maddr, offset;

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
        if (IBIT(24)) {
            undo_record_reg(LR);
            arm_set_reg(LR, arm_get_reg(PC));
        }
        if (execute_check_conds(cond)) {
            arm_set_reg(PC, dest);
        }
        break;
    }

    case ARM_INSTR_LDR:
        if (!IBIT(25)) {
            if (up_down) offset =  imm12bit;
            else         offset = -imm12bit;
        } else {
            offset = barrel_shifter(arm_get_reg(rm), shift_type, imm5shift);
        }

        maddr = arm_get_reg(rn);
        if (rn == 15) maddr += 4;
        if (pre_post) maddr += offset;
        undo_record_reg(rd);
        arm_set_reg(rd, mem_load(maddr, 0));
        if (write_back) {
            if (!pre_post) maddr += offset;
            undo_record_reg(rn);
            arm_set_reg(rn, maddr);
        }
        break;

    case ARM_INSTR_STR:
        if (!IBIT(25)) {
            if (up_down) offset =  imm12bit;
            else         offset = -imm12bit;
        } else {
            offset = barrel_shifter(arm_get_reg(rm), shift_type, imm5shift);
        }

        maddr = arm_get_reg(rn);
        if (rn == 15) maddr += 4;
        if (pre_post) maddr += offset;
        undo_record_memory(maddr);
        mem_store(maddr, 0, rd);
        if (write_back) {
            if (!pre_post) maddr += offset;
            undo_record_reg(rn);
            arm_set_reg(rn, maddr);
        }
        break;

    case ARM_INSTR_LDM:
        maddr = arm_get_reg(rn);
        if (write_back) {
            undo_record_reg(rn);
        }

        for (rm = 0; rm < 16; rm++) {
            if (IBIT(rm)) {
                maddr = pre_inc(maddr, pre_post, up_down);
                if (rm != rn || !write_back) undo_record_reg(rm);
                arm_set_reg(rm, mem_load(maddr, 0));
                maddr = post_inc(maddr, pre_post, up_down);
            }
        }

        if (write_back) {
            arm_set_reg(rn, maddr);
        }
        break;

    case ARM_INSTR_STM:
        maddr = arm_get_reg(rn);
        if (write_back) {
            undo_record_reg(rn);
        }

        for (rm = 0; rm < 16; rm++) {
            if (IBIT(rm)) {
                maddr = pre_inc(maddr, pre_post, up_down);
                undo_record_memory(maddr);
                mem_store(maddr, 0, arm_get_reg(rm));
                maddr = post_inc(maddr, pre_post, up_down);
            }
        }

        if (write_back) {
            arm_set_reg(rn, maddr);
        }
        break;

    default:
        undo_finish_instr();
        return 0;
    }

    undo_finish_instr();

    return 1;
}
