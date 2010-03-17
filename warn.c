#include "sim.h"
#include "arm.h"

void unpredictable(const char *fmt, ...)
{
    va_list ap;

    printf("UNPREDICTABLE INSTRUCTION BEHAVIOR: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    arm_dump_registers();
}

void warn(const char *fmt, ...)
{
    va_list ap;

    printf("Warning: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    arm_dump_registers();
}
