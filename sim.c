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

void brkpoint(void)
{
    /*
     * You can set a breakpoint on this function and call it conditionally
     * from code to help debug things.
     */
}

int main(int argc, char *argv[])
{
    init_memory(0x80000000, MB(16));

    if (image_load("/private/tftpboot/FORTH/FORTH.img")) {
        fprintf(stderr, "ERROR: Couldn't load image /private/tftpboot/FORTH/FORTH.img\n");
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

    reg pc = forth_init(0);

    printf("Initial PC = %8.8x\n", pc);

    mem_dump(pc, 64);

    return 0;
}


