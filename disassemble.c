/*
 * This file is part of arm-sim: http://madscientistroom.org/arm-sim
 *
 * Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
 * reversed. (See the file COPYRIGHT for details.)
 */

#include "sim.h"
#include "arm.h"

char *regs[] = {"r0", "r1", "r2", "r3", "ip", "rp", "top", "count", "r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc"};

reg decode_dest_addr(reg addr, reg offset, int offset_sz, int half_flag)
{
    if (offset >> (offset_sz -1)) {
        /*
         * Set all the high-bits
         */
        reg sign = 0xffffffff - ((1 << offset_sz) -1);
        offset |= sign;
    }

    offset <<= 2;
    if (half_flag) {
        offset |= 2;
    }

    return (addr + 8 + offset) & 0xffffffff;
}

#define MNEMONIC_FIELD_SZ	8
static void print_mnemonic(char *buff, int sz, const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    int len = vsnprintf(buff, sz, fmt, va);
    while (len < MNEMONIC_FIELD_SZ) {
        if (len >= (sz -1)) return;
        buff[len++] = ' ';
        buff[len] = '\0';
    }
}

static void append_operands(char *buff, int sz, const char *fmt, ...)
{
    va_list va;

    int len = strlen(buff);

    if (len >= (sz -1)) return;

    va_start(va, fmt);
    vsnprintf(buff + len, sz - len, fmt, va);
}

void disassemble(reg addr, reg instr, char *buff, int sz)
{
    reg cond = IBITS(28, 4);
    reg rm = IBITS(0, 4);
    reg rs = IBITS(8, 4);
    reg rd = IBITS(12, 4);
    reg rn = IBITS(16, 4);
    reg imm12bit = IBITS(0, 12);
    reg imm24bit = IBITS(0, 24);
    reg imm5shift = IBITS(7, 5);
    reg opcode = IBITS(21, 4);
    reg setconds = IBIT(20);
    reg imm8bit = IBITS(0, 8);
    reg imm_rot  = IBITS(8, 4);
    reg shift_type = IBITS(5, 2);
    reg write_back = IBIT(21);
    reg up_down = IBIT(23);
    reg pre_post = IBIT(24);
#if 0
    char *muls[] = {"mul", "mla", "??", "??", "umull", "umlal", "smull", "smlal"};
#endif

    char temp1[128];
    int t1sz = sizeof(temp1);
    char temp2[128];
    int t2sz = sizeof(temp2);
    char temp3[128];
    int t3sz = sizeof(temp3);

    char *conds[] = {"eq", "ne", "hs", "lo", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le", "", "??"};
    char *opcodes[] = {"and", "eor", "sub", "rsb", "add", "adc", "sbc", "rsc", "tst", "teq", "cmp", "cmn", "orr", "mov", "bic", "mvn"};
    char *shifts[] = {"lsl", "lsr", "asr", "??"};

    arm_instr_t op = arm_decode_instr(instr);
    reg dest;

    if (instr == 0xe494f004) {
        print_mnemonic(buff, sz, "next");
        return;
    }

    switch (op) {
    case ARM_INSTR_B:
        print_mnemonic(buff, sz, "b%s%s",
                       IBIT(24) ? "l" : "",
                       conds[cond]);
        dest = decode_dest_addr(addr, imm24bit, 24, 0);
        append_operands(buff, sz, "%x", dest);
        if (dest == dovar_addr) append_operands(buff, sz, " ; dovar");
        if (dest == docons_addr) append_operands(buff, sz, " ; docons");
        if (dest == dodoes_addr) append_operands(buff, sz, " ; dodoes");
        if (dest == docolon_addr) append_operands(buff, sz, " ; docolon");
        
        break;

    case ARM_INSTR_SWI:
        print_mnemonic(buff, sz, "swi");
        append_operands(buff, sz, "%x", imm24bit);
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
            setconds = 0;
        }

        print_mnemonic(buff, sz, "%s%s%s", opcodes[opcode], conds[cond], setconds ? "s" : "");

        if (IBIT(25)) {
            snprintf(temp1, t1sz, "#%8.8x", imm8bit << (imm_rot << 1));
        } else if (!IBIT(4)) {
            if (!imm5shift) {
                snprintf(temp1, t1sz, "%s", regs[rm]);
            } else {
                snprintf(temp1, t1sz, "%s %s #%d", regs[rm], shifts[shift_type], imm5shift);
            }
        } else {
            snprintf(temp1, t1sz, "%s %s %s", regs[rm], shifts[shift_type], regs[rs]);
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
        case ARM_INSTR_BIC:
            append_operands(buff, sz, "%s, %s, %s", regs[rd], regs[rn], temp1);
            break;

        case ARM_INSTR_TST:
        case ARM_INSTR_TEQ:
        case ARM_INSTR_CMP:
        case ARM_INSTR_CMN:
            append_operands(buff, sz, "%s, %s", regs[rn], temp1);
            break;

        case ARM_INSTR_MOV:
        case ARM_INSTR_MVN:
            append_operands(buff, sz, "%s, %s", regs[rd], temp1);
            break;
        default:
            ASSERT(0);
        }

        break;

    case ARM_INSTR_STR:
    case ARM_INSTR_LDR:
        print_mnemonic(buff, sz, "%s%s%s",
                       IBIT(20) ? "ldr" : "str",
                       conds[cond],
                       IBIT(22) ? "b" : "");

        if (!IBIT(25)) {
            if (imm12bit) {
                snprintf(temp2, t2sz, ", %s%d", !up_down ? "-" : "", imm12bit);
            } else {
                temp2[0] = '\0';
            }
        } else {
            if (imm5shift) {
                snprintf(temp2, t2sz, ", %s %s %d", regs[rm], shifts[shift_type], imm5shift);
            } else {
                snprintf(temp2, t2sz, ", %s", regs[rm]);
            }
        }
        if (!IBIT(22) && rn == 15 && !IBIT(25)) {
            reg t;
            if (!up_down) t = -imm12bit;
            else          t =  imm12bit;
            reg v = mem_load(addr + 8 + t, 0);
            snprintf(temp3, t3sz, ";  # %#8.8x", v);
        } else if (IBIT(22) && rn == 15 && !IBIT(25)) {
            reg t;
            if (!up_down) t = -imm12bit;
            else          t =  imm12bit;
            reg v = mem_load(addr + 8 + t, 0) & 0xff;
            snprintf(temp3, t3sz, ";  # %#2.2x", v);
        } else {
            temp3[0] = '\0';
        }

        if (pre_post) {
            append_operands(buff, sz, "%s, [%s%s]%s%s", regs[rd], regs[rn], temp2,
                            write_back ? "!" : "", temp3);
        } else {
            append_operands(buff, sz, "%s, [%s]%s%s", regs[rd], regs[rn], temp2, temp3);
        }
        break;

    case ARM_INSTR_STM:
    case ARM_INSTR_LDM:
        print_mnemonic(buff, sz, "%s%s%s%s",
                       (op == ARM_INSTR_STM) ? "stm" : "ldm",
                       conds[cond],
                       up_down ? "i" : "d",
                       pre_post ? "b" : "a");
        {
            temp2[0] = '\0';
            strcat(temp2, "{");
            int flag = 0;
            for(int i = 0; i < 16; i++) {
                if (!IBIT(i)) continue;
                if (flag) {
                    strcat(temp2, ", ");
                } else {
                    flag = 1;
                }
                strcat(temp2, regs[i]);
            }
            strcat(temp2, "}");
        }
        append_operands(buff, sz, "%s%s, %s",
                        regs[rn],
                        write_back ? "!" : "",
                        temp2);
        break;

    default:
        print_mnemonic(buff, sz, "(unknown instr)");
    }
}
