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
    reg pc;
    reg flags;
    union {
        int reg_num;
        reg address;
    } u;
    reg contents;
} undo_log_entry_t;

#define MAX_UNDO_LOGS		100000
undo_log_entry_t undo_logs[MAX_UNDO_LOGS];
int undo_started, undo_instr_first;
int undo_count;
int undo_idx;

#define UNDO_REG		1
#define UNDO_MEM		2


static undo_log_entry_t *undo_record_common(int type)
{
    undo_log_entry_t *u = &undo_logs[undo_count++];

    /*
     * Later, we can alloc the undo_logs and grow them as necessary.
     */

    ASSERT(undo_count < MAX_UNDO_LOGS);

    undo_logs[undo_count].type = 0;

    u->type = type;
    u->pc = arm_get_reg(PC);
    u->flags = arm_get_reg(FLAGS);

    return u;
}

static void undo_start(void)
{
    undo_started = 1;
    undo_instr_first = undo_count;
}

static void undo_another(void)
{
    ASSERT(undo_count > 0);
    undo_logs[undo_count -1].type = - undo_logs[undo_count].type;
}

void undo_record_reg(int reg_num)
{
    undo_log_entry_t *u;

    if (undo_started) undo_another();
    else undo_start();

    u = undo_record_common(UNDO_REG);
    u->u.reg_num = reg_num;
    u->contents = arm_get_reg(reg_num);
}


void undo_record_memory(reg address)
{
    undo_log_entry_t *u;

    if (undo_started) undo_another();
    else undo_start();

    u = undo_record_common(UNDO_MEM);
    u->u.address = address;
    u->contents = mem_load(address, 0);
}

void undo_finish_instr(void)
{
    undo_started = 0;
}

void undo_clear(void)
{
    undo_count = undo_idx;

    while (undo_logs[undo_count].type < 0) {
        undo_count++;
    }
    undo_logs[undo_count].type = 0;
}

int undo(int num_steps);
int redo(int num_steps);
int undo_size(void);
int redo_size(void);
