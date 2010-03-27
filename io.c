/*
 * This file is part of arm-sim: http://madscientistroom.org/arm-sim
 *
 * Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
 * reversed. (See the file COPYRIGHT for details.)
 */

#include "sim.h"

reg io_readfile(reg filename, reg len)
{
    char *s = malloc(len + 1);
    char *p = s;

    for (int i = 0; i < len; i++) {
        *p++ = mem_loadb(filename, i);
    }
    *p = '\0';

    file_t *f = file_load(s);

    if (!f) return 0;

    static reg getfiles = GB(2) + MB(16);

    reg fp = getfiles;
    mem_store(getfiles, 0, f->image_size);
    getfiles += 4;

    file_put_in_memory(f, getfiles);
    getfiles += (f->image_size + 63) & ~63;

    return fp;
}

void io_write(reg str, reg len)
{
    char *s;

    while (len-- > 0) {
        s = memory_range(str++, 1);
        if (!s) {
            /*
             * str + len exceeded the bounds of memory assigned to the process.
             * Log the error and continue.
             */
            warn("write from sim address %p of length %d out of bounds", str, len);
            return;
        }

        printf("%c", *s);
    }
}

reg io_readline(reg buffer, reg len)
{
    char *s = memory_range(buffer, len);

    if (!s) {
        /*
         * str + len exceeded the bounds of memory assigned to the process.
         * Log the error and continue.
         */
        warn("readline to sim address %p of length %d out of bounds", buffer, len);
        return 0;
    }

    fgets(s, len, stdin);

    return strlen(s);
}
