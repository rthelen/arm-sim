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
    fprintf(stderr, "-d           -- Dump (print) the dictionary as assembly and FORTH words.\n");
    fprintf(stderr, "-q           -- Quiet output; i.e., don't list each instr. & reg values.\n");
    fprintf(stderr, "-no-undo     -- Don't enable the undo logic.\n");
    fprintf(stderr, "-v           -- Verbose output; print each instr. and reg values.\n");
    fprintf(stderr, "-u           -- Enable the undo logic.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "The undo logic is a system by which the processor can be backed up some\n");
    fprintf(stderr, "number of instructions.  It is off by default.\n");
}

extern reg image_ncells;

int main(int argc, char *argv[])
{
    char *filename = "/private/tftpboot/FORTH/FORTH.img";
    int dump;
    int quiet;
    char **save_argv;

    prog_name = argv[0];

    init_memory(0x80000000, MB(16));

    undo_disable = 1;
    quiet = 1;
    dump = 0;

    argv += 1;
    do {
        save_argv = argv;

        if (!*argv) break;

        if (strcmp(*argv, "-f") == 0 && argv[1]) {
            filename = argv[1];
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
        } else /* usage() */ ;
    } while (save_argv != argv);

    if (image_load(filename)) {
        fprintf(stderr, "ERROR: Couldn't load image %s\n", filename);
        exit(-1);
    }

    if (forth_parse_image()) {
        fprintf(stderr, "ERROR: Kernel image appears incorrect\n");
        exit(-1);
    }

    if (forth_relocate_image(0)) {
        fprintf(stderr, "ERROR: Kernel image failed relocation\n");
        exit(-1);
    }

    arm_set_reg(PC, forth_init(0));
    arm_set_reg(R0, 0 + addr_base);

    if (!dump) {
        if (!quiet) printf("Initial PC = %8.8x, image_ncells = %x\n", arm_get_reg(PC), image_ncells * 4);
        sim_done = 0;
        do {
            if (!quiet) {
                char buff[256];
                int sz = sizeof(buff);
                reg instr = mem_load(arm_get_reg(PC), 0);
                disassemble(arm_get_reg(PC), instr, buff, sz);
                printf("%8.8x: %8.8x  %s\n", arm_get_reg(PC), instr, buff);
            }
            if (!execute_one()) break;
            if (!quiet) {
                for (int i = 0; i < 16; i++) {
                    printf("%5s: %8.8x", regs[i], arm_get_reg(i));
                    if ((i & 3) == 3) printf("\n");
                    else              printf("   ");
                }
            }
        } while (!sim_done);
    } else {
        mem_dump(0x80000038, image_ncells - (0x38/4));
    }

    return 0;
}


