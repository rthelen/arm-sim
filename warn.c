/*
 * This file is part of arm-sim: http://madscientistroom.org/arm-sim
 *
 * Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
 * reversed. (See the file COPYRIGHT for details.)
 */

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

void error(const char *fmt, ...)
{
    va_list ap;

    printf("Error: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    arm_dump_registers();
    exit(-1);
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
