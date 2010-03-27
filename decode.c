/*
 * This file is part of arm-sim: http://madscientistroom.org/arm-sim
 *
 * Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
 * reversed. (See the file COPYRIGHT for details.)
 */

#include "sim.h"
#include "arm.h"

arm_cond_t arm_decode_cond(reg instr)
{
    return IBITS(28,4);
}

arm_instr_t arm_decode_instr(reg instr)
{
    arm_instr_t t = ARM_INSTR_ILLEGAL;

    if (IBITS(24, 4) == 0xF)                   return (ARM_INSTR_SWI);
    if (IBITS(25, 3) == BPAT3(1,0,1))          return (ARM_INSTR_B);
    if (IBITS(26, 2) == BPAT2(0,0)) {
        if (!IBIT(25) && !IBIT(4))             return (ARM_INSTR_AND + IBITS(21, 4));
        if (!IBIT(25) &&  IBIT(4) && !IBIT(7)) return (ARM_INSTR_AND + IBITS(21, 4));
        if ( IBIT(25))                         return (ARM_INSTR_AND + IBITS(21, 4));
    }
    if (IBITS(24,4) == 0 && IBITS(4,4) == BPAT4(1,0,0,1)) {
        switch(IBITS(21,3)) {
        case BPAT3(0,0,0): return (ARM_INSTR_MUL);
        case BPAT3(0,0,1): return (ARM_INSTR_MLA);
        case BPAT3(1,0,0): return (ARM_INSTR_UMULL);
        case BPAT3(1,0,1): return (ARM_INSTR_UMLAL);
        case BPAT3(1,1,0): return (ARM_INSTR_SMULL);
        case BPAT3(1,1,1): return (ARM_INSTR_SMLAL);
        }
    }

    if (IBITS(25,3) == 2) {
        /* Load/Store 12bit imm */
        if (IBIT(20))      return (ARM_INSTR_LDR);
        else               return (ARM_INSTR_STR);
    }
    
    if (IBITS(25,3) == 3 && !IBIT(4)) {
        if (IBIT(20))      return (ARM_INSTR_LDR);
        else               return (ARM_INSTR_STR);
    }

    if (!IBITS(25,3) && IBIT(7) && IBIT(4)) {
        if (IBIT(20)) {
            if (IBIT(22)) {
                if ( IBIT(6) &&  IBIT(5)) return (ARM_INSTR_LDSH);
                if ( IBIT(6) && !IBIT(5)) return (ARM_INSTR_LDSB);
                if (!IBIT(6) &&  IBIT(5)) return (ARM_INSTR_LDUH);
            }
            if (!IBIT(22) && !IBITS(8,4)) {
                if ( IBIT(6) &&  IBIT(5)) return (ARM_INSTR_LDSH);
                if ( IBIT(6) && !IBIT(5)) return (ARM_INSTR_LDSB);
                if (!IBIT(6) &&  IBIT(5)) return (ARM_INSTR_LDUH);
            }
        } else {
            if (IBIT(22)) {
                if (!IBIT(6) &&  IBIT(5)) return (ARM_INSTR_STH);
            }
            if (!IBIT(22) && !IBITS(8,4)) {
                if (!IBIT(6) &&  IBIT(5)) return (ARM_INSTR_STH);
            }
        }
    }

    if (IBITS(25, 3) == BPAT3(1,0,0)) {
        if (IBIT(20)) return (ARM_INSTR_LDM);
        else          return (ARM_INSTR_STM);
    }

    return t;
}

