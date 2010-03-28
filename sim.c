/*
 * This file is part of arm-sim: http://madscientistroom.org/arm-sim
 *
 * Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
 * reversed. (See the file COPYRIGHT for details.)
 */

/*
 * ARM Simulator
 *
 * This program simulates an ARM processor.  It simulates only the bare
 * instructions.  It was originally crafted to simulate enough instructions
 * to allow a FORTH version created by David Frech's muForth to execute.
 *
 * Things to know about the environment:
 *
 * 1) The Forth doesn't use any of the instructions introduced after
 * ARM7TDMI.  Therefore, the instruction set supported is significantly
 * limited.
 *
 * 2) The Forth runs in user or kernel mode only and is therefore immune to
 * processor mode switches.  In addition, this simulator doesn't handle
 * exceptions or interrupts.
 *
 * 3) The program is loaded by a loader. That loader is included in this
 * code to put the Forth kernel in memory.
 *
 * 4) The interface is such that the Forth kernel calls back into an
 * environment for services such as printing and character reading.  Thus,
 * the simulator provides those services and no other device services to
 * the Forth.
 *
 */

#include "sim.h"
#include "arm.h"

void brkpoint(void)
{
    /*
     * You can set a breakpoint on this function and call it conditionally
     * from code to help debug things.
     */
}

const char *prog_name;
void usage(void)
{
    fprintf(stderr, "%s [-dqvu] [-no-undo] [-f filename]\n", prog_name);
    fprintf(stderr, "\n");
    fprintf(stderr, "%s will simulate an ARM processor where the input is the\n", prog_name);
    fprintf(stderr, "dictionary of a FORTH environment.  The program can be tailored\n");
    fprintf(stderr, "for different FORTH systems.  This ARM simulator can also be\n");
    fprintf(stderr, "modified to support non-FORTH systems, but that would be more work.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "-f filename  -- This is the name of the FORTH dictionary to load.\n");
    fprintf(stderr, "-p path      -- Relative or absolute path to FORTH files to load.\n");
    fprintf(stderr, "-d           -- Dump (print) the dictionary as assembly and FORTH words.\n");
    fprintf(stderr, "-b           -- Generate a backtrace.\n");
    fprintf(stderr, "-q           -- Quiet output; i.e., don't list each instr. & reg values.\n");
    fprintf(stderr, "-no-undo     -- Don't enable the undo logic.\n");
    fprintf(stderr, "-v           -- Verbose output; print each instr. and reg values.\n");
    fprintf(stderr, "-u           -- Enable the undo logic.\n");
    fprintf(stderr, "-i           -- Interactive mode.  This also enables: verbose and undo.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "The undo logic is a system by which the processor can be backed up some\n");
    fprintf(stderr, "number of instructions.  It is off by default.\n");
    exit(-1);
}

extern reg image_ncells;
int dump, interactive, quiet, backtrace;

void debug_if(int f)
{
    if (f) {
        interactive = 1;
        quiet = 0;
    }
}

/*
 * If path contains more than one character and ends with '/', then strip
 * the trailing '/'.
 */
void canonicalise_path(char *path)
{
    int len = strlen(path);
    if (len > 1 && path[len-1] == '/')
        path[len-1] = '\0';
}

int main(int argc, char *argv[])
{
    char *filename = "FORTH.img";
    char *fp_env;
    char **save_argv;

    prog_name = argv[0];

    undo_disable = 1;
    quiet = 1;
    dump = 0;
    interactive = 0;
    forth_path = ".";  // I.e., the local directory
    if ((fp_env = getenv("MUFORTH_PATH"))) {
        forth_path = fp_env;
    }

    argv += 1;
    do {
        save_argv = argv;

        if (!*argv) break;

        if (strcmp(*argv, "-f") == 0 && argv[1]) {
            filename = argv[1];
            argv += 2;
        } else if (strcmp(*argv, "-p") == 0 && argv[1]) {
            forth_path = argv[1];
            argv += 2;
        } else if (strcmp(*argv, "-d") == 0) {
            dump = 1;
            argv += 1;
        } else if (strcmp(*argv, "-q") == 0) {
            quiet = 1;
            argv += 1;
        } else if (strcmp(*argv, "-v") == 0) {
            quiet = 0;
            argv += 1;
        } else if (strcmp(*argv, "-no-undo") == 0) {
            undo_disable = 1;
            argv += 1;
        } else if (strcmp(*argv, "-u") == 0) {
            undo_disable = 0;
            argv += 1;
        } else if (strcmp(*argv, "-i") == 0) {
            interactive = 1;
            undo_disable = 0;
            quiet = 0;
            argv += 1;
        } else if (strcmp(*argv, "-b") == 0) {
            backtrace = 1;
            argv += 1;
        } else {
            usage();
        }
    } while (save_argv != argv);

    canonicalise_path(forth_path);

    memory_more(GB(2), MB(20));

    file_t *forth_image = forth_init(filename, GB(2), MB(16));
    reg pc = forth_entry(forth_image);

    arm_set_reg(PC, pc);
    arm_set_reg(R0, GB(2));

    if (!dump) {
        if (!quiet) arm_dump_registers();
        sim_done = 0;
        do {
            if (!quiet) {
                char buff[256];
                int sz = sizeof(buff);
                reg pc = arm_get_reg(PC);
                if (mem_addr_is_valid(pc)) {
                    reg instr = mem_load(pc, 0);
                    disassemble(pc, instr, buff, sz);
                    printf("%8.8x: %8.8x  %s\n", pc, instr, buff);
                }
            }
            if (interactive) {
                char command[256]; // Ignored today.  Will parse later.
                printf("SIM> ");
                fgets(command, sizeof(command), stdin);
            }
            if (!execute_one()) break;
            if (backtrace) forth_backtrace();
            if (!quiet) arm_dump_registers();
        } while (!sim_done);
        printf("Simulator terminated with sim_done == TRUE\n");
    } else {
        mem_dump(forth_image->base + 0x38, (forth_image->size - 0x38)/4);
    }

    return 0;
}
