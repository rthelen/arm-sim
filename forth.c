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
#include <stdarg.h>
#include <stdio.h>
#include <setjmp.h>

static void fth_err(F f, int err_num)
{
    f->err_num = err_num;
    longjmp(f->jmpbuf, err_num);
}

static void fth_assert(F f, int condition, int err, const char *fmt, ...)
{
    va_list ap;

    if (condition == 0) return;

    va_start(ap, fmt);
    vasprintf(&f->err_str, fmt, ap);
    va_end(ap);

    fth_err(f, err);
}

static void fth_stack_check(F f, int stack_num)
{
    int err_under, err_over, sp, sz;
    const char *name;

    switch (stack_num) {
    case F_DATA_STACK: // Data stack
        err_under = F_STACK_UNDERRUN;
        err_over  = F_STACK_OVERFLOW;
        sp = f->sp;
        sz = STACK_SIZE;
        name = "data";
        break;

    case F_RETURN_STACK: // Data stack
        err_under = F_RETURN_STACK_UNDERRUN;
        err_over  = F_RETURN_STACK_OVERFLOW;
        sp = f->sp;
        sz = RSTACK_SIZE;
        name = "return";
        break;

    case F_LOOP_STACK: // Data stack
        err_under = F_LOOP_STACK_UNDERRUN;
        err_over  = F_LOOP_STACK_OVERFLOW;
        sp = f->sp;
        sz = LSTACK_SIZE;
        name = "loop";
        break;
        
    default:
        fprintf(stderr, "INVALID stack number used in fth_stack_check(%d)\n", stack_num);
        exit(-1);
    }
        
    fth_assert(f, sp <= 0, err_over, "%s stack overflow", name);
    fth_assert(f, sp > sz, err_under, "%s stack underflow", name);
}

void rpush(F f, fth_body_t *p) { fth_stack_check(f, F_RETURN_STACK); rstack[--RP] = p; }
fth_body_t *rpop(F f)          { fth_stack_check(f, F_RETURN_STACK); return rstack[RP++]; }

void lpush(F f, int n)         { fth_stack_check(f, F_LOOP_STACK); lstack[--LP] = n; }
int lpop(F f)                  { fth_stack_check(f, F_LOOP_STACK); return lstack[LP++]; }

static void push(F f, cell v)  { fth_stack_check(f, F_DATA_STACK);  stack[--SP] = v; }
cell fth_pop(F f)              { fth_stack_check(f, F_DATA_STACK);  return stack[SP++]; }

scell fth_spop(F f)   { return (scell) POP; }
void fth_dup(F f)     { cell a = POP; PUSH(a); PUSH(a); }
void fth_drop(F f)    { POP; }
void fth_swap(F f)    { cell a = POP; cell b = POP; PUSH(a); PUSH(b); }
void fth_nip(F f)     { SWAP; DROP; }

void fth_plus(F f)    { PUSH(SPOP + SPOP); }
void fth_sub(F f)     { PUSH(-SPOP + SPOP); }
void fth_star(F f)    { PUSH(SPOP * SPOP); }
void fth_slash(F f)   { SWAP; PUSH(SPOP / SPOP); }
void fth_and(F f)     { PUSH(POP & POP); }
void fth_or(F f)      { PUSH(POP | POP); }
void fth_xor(F f)     { PUSH(POP ^ POP); }

void fth_negate(F f)   { PUSH(-SPOP); }
void fth_invert(F f)   { PUSH(~SPOP); }

void fth_2star(F f)    { PUSH(SPOP << 1); }
void fth_2slash(F f)   { PUSH(SPOP >> 1); }
void fth_u2slash(F f)  { PUSH(POP >> 1); }

void fth_shift_left(F f)    { cell cnt = POP,       n =  POP; PUSH(n << cnt); }
void fth_shift_right(F f)   { cell cnt = POP; scell n = SPOP; PUSH(n >> cnt); }
void fth_ushift_right(F f)  { cell cnt = POP; cell  n =  POP; PUSH(n >> cnt); }

void fth_fetch(F f)   { PUSH(mem_load(POP, 0)); }
void fth_cfetch(F f)  { PUSH(mem_loadb(POP, 0)); }

void fth_store(F f)       { cell addr = POP, v = POP; mem_store(addr, 0, v); }
void fth_cstore(F f)      { cell addr = POP, v = POP; mem_storeb(addr, 0, v); }
void fth_plus_store(F f)  { cell addr = POP, v = POP; mem_store(mem_load(addr, 0), 0, v); }

void fth_2drop(F f)  { POP; POP; }
void fth_over(F f)   { cell b = POP; cell a = POP; PUSH(a); PUSH(b); PUSH(a); } /* a b -> a b a */

void fth_uless(F f)  {  cell b =  POP;  cell a =  POP; PUSH(a < b ? -1 : 0); }
void fth_less(F f)   { scell b = SPOP; scell a = SPOP; PUSH(a < b ? -1 : 0); }

void fth_zero_less(F f)   { PUSH(SPOP <  0 ? -1 : 0); }
void fth_zero_equal(F f)  { PUSH( POP == 0 ? -1 : 0); }

void fth_uslash_mod(F f)  /* u1 u2 -- um uq */
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
void fth_slash_mod(F f)  /* n1 n2 -- m q */
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

void fth_dot(F f) { printf("%x ", POP); }

void fth_do_var(fth_header_t *v) { PUSH(v->n.var); }
void fth_do_cons(fth_header_t *c) { PUSH(v->n.cons); }
void fth_do_array(fth_header_t *a)
{
    cell idx = POP;
    if (idx >= a->n.dim) {
        longjmp(fth_jmpbuf, 3);
    }

    PUSH(a->p.body[idx]);
}

static cell *fth_get_var_address(F f)
{
    fth_header_t *v = *IP++;
    if (v->func != fth_do_var) {
        longjmp(fth_jmpbuf, 2);
    }

    return &v->n.var;
}

static cell *fth_get_array_address(F f)
{
    fth_header_t *v = *IP++;
    if (v->func != fth_do_array) {
        longjmp(fth_jmpbuf, 2);
    }

    cell idx = POP;
    if (idx >= v->n.dim) {
        longjmp(fth_jmpbuf, 3);
    }

    return &v->p.body[idx];
}

void fth_set(F f)  {       *fth_get_var_address() = POP; }
void fth_get(F f)  {  PUSH(*fth_get_var_address()); }
void fth_seti(F f) {       *fth_get_array_address() = POP; }
void fth_geti(F f) {  PUSH(*fth_get_array_address()); }

static void fth_get_input_char(F f)
{
    static last_c;
    char c;

    // Line numbers aren't bumped until we're actually reading
    // the character -after- a new line.
    if (last_c == '\n') {
        f->input_line_cnt ++;
        f->input_char_cnt = 0;
    }
    f->input_char_cnt ++;

    if (f->input_offset >= f->input_len) return EOF;
    last_c = c = f->input[f->input_offset++];

    return c;
}

static void fth_parse_end(F f, int cons_char)
{
    if (cons_char) {
        f->token_end = f->input_offset -1;
    } else {
        f->token_end = f->input_offset;
    }
}

static void fth_parse(F f, char delim) // delim --
{
    f->token_start = f->input_offset;

    do {
        c = fth_get_input_char(f);
        if (c == EOF)
            return fth_parse_end(f, 0);
        if (delim == ' ' && isspace(c))
            return fth_parse_end(f, 1);
        else if (delim == c)
            return fth_parse_end(f, 1);
    } while (1);
}

static void fth_token(F f)
{
    char c;

    f->token_start = f->token_end = -1;

    do {
        c = fth_get_input_char(f);

        if (c == EOF) return;
    } while (isspace(c));

    fth_parse(f, ' ');

    int i = f->token_start;
    char *p = f->token 
}

void fth_process_input_line(F f, char *input, int len)
{
    f->input = input;
    f->input_len = len;

    f->input_offset = 0;
    f->input_line_cnt = 1;
    f->input_char_cnt = 0;

    f->in_colon = 0;
    f->code_offset = 0;

    while (f->input_offset < f->input_len) {
        fth_token(f);
        if (f->token_start == -1) break;
        fth_header_t *w = fth_lookup_token(f);
        if (!w) {
            cell n;
            fth_assert(f, fth_number_token(f, &n), FERR_INVALID_TOKEN,
                       "Word %s wasn't found in the dictionary and doesn't look like a number", 
                
        }
        fth_process_token(f);
    }

    fth_execute_input(f);
}
