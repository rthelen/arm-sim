/*
 * This file is part of arm-sim: http://madscientistroom.org/arm-sim
 *
 * Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
 * reversed. (See the file COPYRIGHT for details.)
 */

/*
 * execute.c
 *
 * This file has the code to execute ARM instructions.
 */

#include "sim.h"
#include "arm.h"

#define TF(x)    ((x)  ? 1 : 0)
#define T(x)	 ((x)  ? 1 : 0)
#define F(x)     (!(x) ? 1 : 0)
#define CLR(x)   (!TF(x))
#define SET(x)   ( TF(x))
#define SIGN(x)  ( TF((x) & (1 << 31)))

void init_execution(void)
{
    arm_set_reg(FLAGS, 0x0);
}

static reg barrel_shifter(reg is_reg_shift, reg base, reg shift_type, reg shift, reg *carry_out)
{
    reg result;
    reg result_carry;

    switch (shift_type) {
    case 0:  // LSL
        if (shift > 32) {
            result_carry = 0;
            result = 0;
        } else if (is_reg_shift || shift > 0) {
            result_carry = base >> (32-shift);
            result = base << shift;
        } else {
            result = base;
            result_carry = (arm_get_reg(FLAGS) & C) >> C_SHIFT;
        }
        break;
    case 1:  // LSR
        if (shift > 32) {
            result_carry = 0;
            result = 0;
        } else if (is_reg_shift || shift > 0) {
            result_carry = base >> (shift -1);
            result = base >> shift;
        } else {
            result_carry = base >> 31;
            result = 0;
        }
        break;
    case 2:  // ASR
        if (shift > 32) {
            result_carry = base >> 31;
            result = (signed) base >> 31;
        } else if (is_reg_shift || shift > 0) {
            result_carry = base >> (shift -1);
            result = (signed) base >> shift;
        } else {
            result_carry = base >> 31;
            result = (signed) base >> 31;
        }
        break;
    case 3:  // ROR
        while (shift > 32) {
            shift -= 32;
        }
        if (is_reg_shift || shift > 0) {
            result_carry = base >> (shift -1);
            result = (base >> shift) | (base << (32 - shift));
        } else {
            /*
             * RRX: shift right one bit and insert the carry
             */
            reg carry_in = (arm_get_reg(FLAGS) & C) >> C_SHIFT;
            result = carry_in << 31 | base >> 1;
            result_carry = base;
        }
        break;
    default:
        result_carry = 0;
        result = 0;
    }

    if (carry_out) {
        *carry_out = result_carry & 1;
    }

    return result;
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

extern int interactive, quiet;

int execute_callbacks(reg pc)
{
    undo_record_reg(PC);
    arm_set_reg(PC, arm_get_reg(LR));

    switch (pc) {
    case 1: sim_done = 1; break;
    case 2: io_write(arm_get_reg(R0), arm_get_reg(R1)); break;
    case 3: arm_set_reg(R0, io_readline(arm_get_reg(R0), arm_get_reg(R1))); debug_if(0); break;
    case 4: arm_set_reg(R0, io_readfile(arm_get_reg(R0), arm_get_reg(R1))); break;
    case 5: /* Nothing to do for sync caches */ break;
    }

    return 1;
}

int execute_one(void)
{
    reg pc = arm_get_reg(PC);

    if (pc > 0 && pc < 6) {
        return execute_callbacks(pc);
    }

    reg instr = mem_load(pc, 0);
    
    if (instr == BAD_MEMVAL) {
        return 0;
    }

    arm_instr_t op = arm_decode_instr(instr);
    reg cond = IBITS(28, 4);
    reg rm = IBITS(0, 4);
    reg rs = IBITS(8, 4);
    reg rd = IBITS(12, 4);
    reg rn = IBITS(16, 4);
    reg up_down = IBIT(23);
    reg pre_post = IBIT(24);
    reg write_back = IBIT(21);
    reg imm12bit = IBITS(0, 12);
    reg imm5shift = IBITS(7, 5);
    reg imm8bit = IBITS(0, 8);
    reg imm_rot  = IBITS(8, 4);
    reg shift_type = IBITS(5, 2);
    reg setconds = IBIT(20);
    reg maddr, offset;
    reg d, n, m;
    reg c, z, v, nc;
    reg step;

    /*
     * Most instructions step forward one instruction
     * Branch doesn't (necessarily) but it handles it's own case below.
     */
    undo_record_reg(PC);
    arm_set_reg(PC, pc + 4);

    if (!execute_check_conds(cond)) {
        undo_finish_instr();
        return 1;
    }

    switch (op) {
    case ARM_INSTR_B:
    {
        reg imm24bit = IBITS(0, 24);
        reg dest = decode_dest_addr(pc, imm24bit, 24, 0);
        if (IBIT(24)) {
            undo_record_reg(LR);
            arm_set_reg(LR, arm_get_reg(PC));
        }
        arm_set_reg(PC, dest);
        break;
    }

    case ARM_INSTR_LDR:
        if (!IBIT(25)) {
            if (up_down) offset =  imm12bit;
            else         offset = -imm12bit;
        } else {
            m = arm_get_reg(rm);
            if (m == PC) m += 4;
            offset = barrel_shifter(FALSE, m, shift_type, imm5shift, NULL);
        }

        maddr = arm_get_reg(rn);
        if (rn == PC) maddr += 4;
        if (pre_post) maddr += offset;
        undo_record_reg(rd);
        if (!IBIT(22)) {
            arm_set_reg(rd, mem_load(maddr, 0));
        } else {
            arm_set_reg(rd, mem_loadb(maddr, 0));
        }
        if (write_back || !pre_post) {
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
            m = arm_get_reg(rm);
            if (m == PC) m += 4;
            offset = barrel_shifter(FALSE, arm_get_reg(rm), shift_type, imm5shift, NULL);
        }

        maddr = arm_get_reg(rn);
        if (rn == PC) maddr += 4;
        if (pre_post) maddr += offset;
        undo_record_memory(maddr);
        if (!IBIT(22)) {
            mem_store(maddr, 0, arm_get_reg(rd));
        } else {
            mem_storeb(maddr, 0, arm_get_reg(rd));
        }
        if (write_back || !pre_post) {
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

        if (up_down) {
            rm = 0;
            step = 1;
        } else {
            rm = 15;
            step = -1;
        }
        for (reg count = 0; count < 16; count++, rm += step) {
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

        if (up_down) {
            rm = 0;
            step = 1;
        } else {
            rm = 15;
            step = -1;
        }
        for (reg count = 0; count < 16; count++, rm += step) {
            if (IBIT(rm)) {
                maddr = pre_inc(maddr, pre_post, up_down);
                undo_record_memory(maddr);
                reg m = arm_get_reg(rm);
                if (rm == PC) m += 8;
                mem_store(maddr, 0, arm_get_reg(rm));
                maddr = post_inc(maddr, pre_post, up_down);
            }
        }

        if (write_back) {
            arm_set_reg(rn, maddr);
        }
        break;

    case ARM_INSTR_AND:
    case ARM_INSTR_EOR:
    case ARM_INSTR_SUB:
    case ARM_INSTR_RSB:
    case ARM_INSTR_ADD:
    case ARM_INSTR_ADC:
    case ARM_INSTR_SBC:
    case ARM_INSTR_RSC:
    case ARM_INSTR_TST:
    case ARM_INSTR_TEQ:
    case ARM_INSTR_CMP:
    case ARM_INSTR_CMN:
    case ARM_INSTR_ORR:
    case ARM_INSTR_MOV:
    case ARM_INSTR_BIC:
    case ARM_INSTR_MVN:
        if (op == ARM_INSTR_TST ||
            op == ARM_INSTR_TEQ ||
            op == ARM_INSTR_CMP ||
            op == ARM_INSTR_CMN) {
            setconds = 1;
        }

        reg flags = arm_get_reg(FLAGS);
        n = arm_get_reg(rn);
        if (rn == PC) n += 4;

        if (IBIT(25)) {
            m = imm8bit << (imm_rot << 1);
            if (imm_rot > 0) {
                nc = (imm8bit << ((imm_rot << 1) -1)) >> 31;
            } else {
                nc = imm8bit & 1;  // XXX: Is this right? 
            }
        } else {
            m = arm_get_reg(rm);
            if (!IBIT(4)) {
                if (rm == PC) m += 4;
                m = barrel_shifter(FALSE, m, shift_type, imm5shift, &nc);
            } else {
                if (rm == PC) m += 8;
                reg s = arm_get_reg(rs);
                if (rs == PC) s += 8;
                s &= 0xFF;  // Only one byte of register is used
                m = barrel_shifter(TRUE, m, shift_type, s, &nc);
            }
        }

        c = (arm_get_reg(FLAGS) & C) >> C_SHIFT;
        switch (op) {
        case ARM_INSTR_AND:         d = n & m    ; break;
        case ARM_INSTR_EOR:         d = n ^ m    ; break;
        case ARM_INSTR_SUB: m = ~m; d = n + m + 1; break;
        case ARM_INSTR_RSB: n = ~n; d = m + n + 1; break;
        case ARM_INSTR_ADD:         d = n + m    ; break;
        case ARM_INSTR_ADC: m =  m; d = n + m + c; break;
        case ARM_INSTR_SBC: m = ~m; d = n + m + c; break;
        case ARM_INSTR_RSC: n = ~n; d = m + n + c; break;
        case ARM_INSTR_TST:         d = n & m    ; break;
        case ARM_INSTR_TEQ:         d = n ^ m    ; break;
        case ARM_INSTR_CMP: m = ~m; d = n + m + 1; break;
        case ARM_INSTR_CMN:         d = n + m    ; break;
        case ARM_INSTR_ORR:         d = n | m    ; break;
        case ARM_INSTR_MOV:         d =     m    ; break;
        case ARM_INSTR_BIC: m = ~m; d = n & m    ; break;
        case ARM_INSTR_MVN: m = ~m; d =     m    ; break;
        default:                    d =0xEEBADADD; break;
        }

        switch (op) {
        case ARM_INSTR_AND:
        case ARM_INSTR_EOR:
        case ARM_INSTR_TST:
        case ARM_INSTR_TEQ:
        case ARM_INSTR_ORR:
        case ARM_INSTR_MOV:
        case ARM_INSTR_BIC:
        case ARM_INSTR_MVN:
            if (setconds && rd != PC) {
                undo_record_reg(FLAGS);
                // V := V
                v = (flags & V) >> V_SHIFT;
                // C := carry out from the barrel shifter, or C if shift is LSL #0
                c = nc;  // LSL #0 handled by barrel shift logic
                // Z := if d == 0
                z = d == 0;
                // N := if d & (1<<31)
                n = SIGN(d);

                arm_set_reg(FLAGS, z << Z_SHIFT | v << V_SHIFT | n << N_SHIFT | c << C_SHIFT);
            }
            break;

        case ARM_INSTR_SUB:
        case ARM_INSTR_RSB:
        case ARM_INSTR_ADD:
        case ARM_INSTR_ADC:
        case ARM_INSTR_SBC:
        case ARM_INSTR_RSC:
        case ARM_INSTR_CMP:
        case ARM_INSTR_CMN:
            if (setconds && rd != PC) {
                undo_record_reg(FLAGS);
                if (!SIGN(n ^ m)) {
                    /*
                     * If the signs of the two operands are the same, then
                     * the overflow bit is set when the result has a sign
                     * different from either of the operands.
                     */
                    v = TF(SIGN(d ^ m));
                } else {
                    /*
                     * If the signs of the two operands are different, then
                     * there isn't any possibility of an overflow.
                     */
                    v = 0;
                }
                // C := carry out of the ALU
                if (SIGN(n | m)) {
                    c = !(SIGN(d));
                } else {
                    c = 0;
                }
                // Z := if d == 0
                z = d == 0;
                // N := if d & (1<<31)
                n = TF(SIGN(d));

                arm_set_reg(FLAGS, z << Z_SHIFT | v << V_SHIFT | n << N_SHIFT | c << C_SHIFT);
            }
            break;
        default: break;
        }

        switch (op) {
        case ARM_INSTR_AND:
        case ARM_INSTR_EOR:
        case ARM_INSTR_SUB:
        case ARM_INSTR_RSB:
        case ARM_INSTR_ADD:
        case ARM_INSTR_ADC:
        case ARM_INSTR_SBC:
        case ARM_INSTR_RSC:
        case ARM_INSTR_ORR:
        case ARM_INSTR_MOV:
        case ARM_INSTR_BIC:
        case ARM_INSTR_MVN:
            undo_record_reg(rd);
            arm_set_reg(rd, d);
            break;
        default: break;
        }
        break;

    default:
        undo_finish_instr();
        warn("Unimplemented instruction: %8.8x", instr);
        return 0;
    }

    undo_finish_instr();

    return 1;
}
