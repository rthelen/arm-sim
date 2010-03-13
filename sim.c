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

extern reg image_ncells;

int main(int argc, char *argv[])
{
    char *filename = "/private/tftpboot/FORTH/FORTH.img";
    int dump = 0;
    int quiet = 0;
    char **save_argv;

    init_memory(0x80000000, MB(16));

    argv += 1;
    do {
        save_argv = argv;

        if (!*argv) break;

        if (strcmp(*argv, "-f") == 0 && argv[1]) {
            filename = argv[1];
            argv += 2;
        }

        if (strcmp(*argv, "-d") == 0) {
            dump = 1;
            argv += 1;
        }

        if (strcmp(*argv, "-q") == 0) {
            quiet = 1;
            argv += 1;
        }
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


