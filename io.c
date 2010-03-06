#include "sim.h"

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
