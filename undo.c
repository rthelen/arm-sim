/*
 * This file is part of arm-sim: http://madscientistroom.org/arm-sim
 *
 * Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
 * reversed. (See the file COPYRIGHT for details.)
 */

#include "sim.h"
#include "arm.h"

/*
 * undo architecture
 *
 * The basic model for the undo architecture is that any instruction can be
 * undo back to the beginning of time.  One or more undo log entries
 * represent each instruction that has executed.  The current machine state
 * represents the sum affect all previous instructions have had on the
 * machine.  And, if there are undo logs off into the future, then they
 * could be applied to step forward some number of instructions.
 *
 * NOTE: When undoing, each previous undo log entry is read and its
 * contents are converted.  First, its contents are read and the affected
 * register of the simulator state is stored into the undo log entry.
 * Then, the stored state of the undo log entry is applied to the machine.
 * Thus, an undo log entry becomes a redo log entry.  And, when going
 * forward, the reverse is true.
 *
 * So, there are three pieces to the puzzle.  All of the undo log entries
 * (i.e., all entries before the current state of the machine); the current
 * state of the machine; and, all of the redo logs after the current state.
 *
 * In other words, if the emulator state were to be written out, the undo
 * logs are not sufficient.  The machines current state also needs to be
 * written out.
 */

typedef struct {
    int type;		// Negative if the next log entry is part of same instruction
    union {
        int reg_num;
        reg address;
    } u;
    reg contents;
} undo_log_entry_t;

/*
 * Add logic to use head/tail pointers so that the system can back up N
 * instructions but can an arbitrary number of instructions.
 */

#define MAX_UNDO_LOGS		2000
undo_log_entry_t undo_logs[MAX_UNDO_LOGS];
int undo_started;
int undo_count;
int undo_disable;
int undo_head, undo_tail;

/*
 * undo_head points to the beginning of a complete undo log sequence.
 * undo_tail points to the next log entry to use.  When undo_tail points at
 * undo_head, i.e., the log is full, then when a new log entry is needed,
 * undo_head is pushed forward past the undo log sequence to point to the
 * first log entry in the next log sequence.
 *
 * undo_count is the number of sequences contained between undo_head and
 * undo_tail.
 */

#define UNDO_REG		1
#define UNDO_MEM		2

#define NEXT(v)		(((v) + 1) % MAX_UNDO_LOGS)
#define PREV(v)		(((v) + MAX_UNDO_LOGS - 1) % MAX_UNDO_LOGS)

static int undo_skip_forward(int seq_index)
{
    while (undo_logs[seq_index].type < 0) {
        seq_index = NEXT(seq_index);
    }

    return NEXT(seq_index);
}

static undo_log_entry_t *undo_record_common(int type)
{
    undo_log_entry_t *u = &undo_logs[undo_tail];
    u->type = type;

    undo_tail = NEXT(undo_tail);

    if (undo_tail == undo_head) {
        undo_head = undo_skip_forward(undo_head);
        undo_count --;
    }

    return u;
}

static void undo_start(void)
{
    undo_started = 1;
    undo_count += 1;
}

static void undo_another(void)
{
    undo_log_entry_t *u = &undo_logs[PREV(undo_tail)];
    u->type = -u->type;
}

void undo_record_reg(int reg_num)
{
    undo_log_entry_t *u;

    if (undo_disable) return;

    if (!undo_started) undo_start();
    else undo_another();

    u = undo_record_common(UNDO_REG);
    u->u.reg_num = reg_num;
    u->contents = arm_get_reg(reg_num);
}


void undo_record_memory(reg address)
{
    undo_log_entry_t *u;

    if (undo_disable) return;

    if (!undo_started) undo_start();
    else undo_another();

    u = undo_record_common(UNDO_MEM);
    u->u.address = address;
    u->contents = mem_load(address, 0);
}

void undo_record_byte(reg address)
{
    undo_log_entry_t *u;

    if (undo_disable) return;

    if (!undo_started) undo_start();
    else undo_another();

    u = undo_record_common(UNDO_MEM);
    u->u.address = address;
    u->contents = mem_loadb(address, 0);
}

void undo_finish_instr(void)
{
    undo_started = 0;
}

void undo_clear(void)
{
}

int undo(int num_steps);
int redo(int num_steps);

int undo_size(void)
{
    return undo_count;
}

int redo_size(void);
