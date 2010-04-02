/*
 * This file is part of arm-sim: http://madscientistroom.org/arm-sim
 *
 * Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
 * reversed. (See the file COPYRIGHT for details.)
 */

/*
 * FORTH
 *
 * This file implements a Forth for use in an ARM simulator.  It's narrowly
 * focused, so don't get your hopes up.  But, here's the basic outline:
 *
 * 1. There are built in functions.  These functions are your basic + - * /
 *    and or xor 0= if then do loop +loop etc.  These words are written in
 *    C.  They're added to a dictionary called "builtins".
 *
 * 2. There are words defined because they're loaded along with the kernel
 *    image to debug.  These words are added to a special dictionary called
 *    "forth".
 *
 * 3. There are words defined by the user of the debugger.  Those words are
 *    added to the dictionary called "user".
 *
 * Note that all the builtin functions are written directly in C; often
 * using macros to make the C code look just like FORTH.
 *
 * However, the words defined in 2 and 3 are either variables or colon
 * words.  There is no support for do-does or builds words to the user.
 * (Nor cons, but that's a small price.)
 *
 * A simple ITC model is used.  Each word has an entry that begins with
 * either "do_builtin", "do_var", "do_array", "do_cons", or "do_colon".
 * Also provided is an address to the (malloc'd) memory for the word.  In
 * the case of builtin words then there isn't a supplied parameter.
 * However, all other words have memory allocated to them.
 *
 * Maybe I'll add a string type.  And an array of strings type?  Let's not
 * go crazy.
 */

#include "sim.h"
#include "forth.h"
#include <setjmp.h>

as_header_t *dict_head;
as_header_t *dict_bps[MAX_BREAK_POINTS];

jmp_buf forth_jmpbuf;

int SP;            /* parameter stack pointer */
int RP;            /* return stack pointer */
int LP;            /* loop stack pointer */
as_body_t *IP;     /* instruction pointer */

cell        stack[STACK_SIZE];
as_body_t *rstack[RSTACK_SIZE];
int        lstack[LSTACK_SIZE];

static void stack_check(int sp, int sz)
{
    if ((sp <= 0) || (sp > sz)) longjmp(forth_jmpbuf, 1);
}

void rpush(as_body_t *p) { stack_check(RP, RSTACK_SIZE); rstack[--RP] = p; }
as_body_t *rpop(void)    { stack_check(RP, RSTACK_SIZE); return rstack[RP++]; }

void lpush(int n) { stack_check(LP, LSTACK_SIZE); lstack[--LP] = n; }
int lpop(void)    { stack_check(LP, LSTACK_SIZE); return lstack[LP++]; }

static void push(cell v) { stack_check(SP, STACK_SIZE);  stack[--SP] = v; }
cell as_pop(void)        { stack_check(SP, STACK_SIZE);  return stack[SP++]; }

scell as_spop(void)   { return (scell) as_pop(); }
void as_dup(void) { cell a = POP; PUSH(a); PUSH(a); }
void as_drop(void) { POP; }
void as_swap(void) { cell a = POP; cell b = POP; PUSH(a); PUSH(b); }
void as_nip(void) { SWAP; DROP; }

void as_plus(void) { PUSH(SPOP + SPOP); }
void as_sub(void)  { PUSH(-SPOP + SPOP); }
void as_star()     { PUSH(SPOP * SPOP); }
void as_slash()    { SWAP; PUSH(SPOP / SPOP); }
void as_and(void)  { PUSH(POP & POP); }
void as_or(void)   { PUSH(POP | POP); }
void as_xor(void)  { PUSH(POP ^ POP); }

void as_negate(void)   { PUSH(-SPOP); }
void as_invert(void)   { PUSH(~SPOP); }

void as_2star(void)    { PUSH(SPOP << 1); }
void as_2slash(void)   { PUSH(SPOP >> 1); }
void as_u2slash(void)  { PUSH(POP >> 1); }

void as_shift_left(void)    { cell cnt = POP,       n =  POP; PUSH(n << cnt); }
void as_shift_right(void)   { cell cnt = POP; scell n = SPOP; PUSH(n >> cnt); }
void as_ushift_right(void)  { cell cnt = POP; cell  n =  POP; PUSH(n >> cnt); }

void as_fetch(void)   { PUSH(mem_load(POP, 0)); }
void as_cfetch(void)  { PUSH(mem_loadb(POP, 0)); }

void as_store(void)       { cell addr = POP, v = POP; mem_store(addr, 0, v); }
void as_cstore(void)      { cell addr = POP, v = POP; mem_storeb(addr, 0, v); }
void as_plus_store(void)  { cell addr = POP, v = POP; mem_store(mem_load(addr, 0), 0, v); }

void as_2drop(void)  { POP; POP; }
void as_over(void)   { cell b = POP; cell a = POP; PUSH(a); PUSH(b); PUSH(a); } /* a b -> a b a */

void as_uless(void)  {  cell b =  POP;  cell a =  POP; PUSH(a < b ? -1 : 0); }
void as_less(void)   { scell b = SPOP; scell a = SPOP; PUSH(a < b ? -1 : 0); }

void as_zero_less(void)   { PUSH(SPOP <  0 ? -1 : 0); }
void as_zero_equal(void)  { PUSH( POP == 0 ? -1 : 0); }

void as_uslash_mod(void)  /* u1 u2 -- um uq */
{
    cell top = POP;
    cell st1 = POP;

    cell uquot = st1 / top;
    cell umod  = st1 % top;

    PUSH(umod);
    PUSH(uquot);
}

/*
 * Of course, [David Frech] is not giving up floored division. ;-)
 *
 * Most processors do symmetric division. To fix this (to make it _FLOOR_)
 * we have to adjust the quotient and remainder when rem != 0 and the
 * divisor and dividend are different signs. (This is NOT the same as
 * quotient < 0, because the quotient could have been truncated to zero by
 * symmetric division when the actual (floored) quotient is < 0!) The
 * adjustment is:
 *
 *  quot_floored = quot_symm - 1
 *   mod_floored =  rem_symm + divisor
 *
 * This preserves the invariant a / b => (r,q) s.t. (q * b) + r = a.
 *
 *   (q' * b) + r' = (q - 1) * b + (r + b) = (q * b) - b + r + b
 *            = (q * b) + r
 *            = a
 *
 * where q',r' are the _floored_ quotient and remainder (really, modulus),
 * and q,r are the symmetric quotient and remainder.
 *
 */
void as_slash_mod(void)  /* n1 n2 -- m q */
{
    scell top = POP;
    scell st1 = POP;

    scell quot = st1 / top;
    scell mod  = st1 % top;

#ifndef HOST_DIVIDE_FLOORS
    /*
     * We now have the results of a stupid symmetric division, which we
     * must convert to floored. We only do this if the modulus was non-zero
     * and if the dividend and divisor had opposite signs.
     */
    if (mod != 0 && (st1 ^ top) < 0)
    {
        quot -= 1;
        mod  += top;
    }
#endif

    PUSH(mod);
    PUSH(quot);
}
