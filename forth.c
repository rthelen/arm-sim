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

/*
 * A test routine to drive the forth environment.
 */
void ftest(void)
{
    F f = forth_new();
    char *input = "4 5 + .";
    forth_process_input_line(f, input, strlen(input));
}

/**********************************************************
 *
 * Error handling and Error checking words
 *
 **********************************************************/

static void forth_err(F f, int err_num)
{
    fprintf(stderr, "%s\n", f->err_str);
    f->err_num = err_num;
    fprintf(stderr, "ERROR: About to longjmp(%d) to infinity\n", err_num);
    exit(-1);
//    longjmp(f->forth_jmpbuf, err_num);
}

static void forth_assert(F f, int condition, int err, const char *fmt, ...)
{
    va_list ap;

    if (condition == 1) return;

    va_start(ap, fmt);
    vasprintf(&f->err_str, fmt, ap);
    va_end(ap);

    forth_err(f, err);
}

static void forth_stack_check(F f, int stack_num)
{
    int err_under, err_over, sp, sz;
    const char *name;

    switch (stack_num) {
    case F_DATA_STACK: // Data stack
        err_under = FERR_STACK_UNDERRUN;
        err_over  = FERR_STACK_OVERFLOW;
        sp = SP;
        sz = STACK_SIZE;
        name = "data";
        break;

    case F_RETURN_STACK: // Data stack
        err_under = FERR_RETURN_STACK_UNDERRUN;
        err_over  = FERR_RETURN_STACK_OVERFLOW;
        sp = RP;
        sz = RSTACK_SIZE;
        name = "return";
        break;

    case F_LOOP_STACK: // Data stack
        err_under = FERR_LOOP_STACK_UNDERRUN;
        err_over  = FERR_LOOP_STACK_OVERFLOW;
        sp = LP;
        sz = LSTACK_SIZE;
        name = "loop";
        break;
        
    default:
        fprintf(stderr, "INVALID stack number used in forth_stack_check(%d)\n", stack_num);
        exit(-1);
    }
        
    forth_assert(f, sp > 0, err_over, "%s stack overflow", name);
    forth_assert(f, sp <= sz, err_under, "%s stack underflow", name);
}

/*
 **********************************************************
 *
 * Stack Push/Pop routines
 *
 **********************************************************
 **/

void rpush(F f, forth_body_t *p) { forth_stack_check(f, F_RETURN_STACK); f->rstack[--RP] = p; }
forth_body_t *rpop(F f)          { forth_stack_check(f, F_RETURN_STACK); return f->rstack[RP++]; }

void lpush(F f, int n)           { forth_stack_check(f, F_LOOP_STACK); f->lstack[--LP] = n; }
int lpop(F f)                    { forth_stack_check(f, F_LOOP_STACK); return f->lstack[LP++]; }

static void push(F f, cell v)    { forth_stack_check(f, F_DATA_STACK);  f->stack[--SP] = v; }
cell pop(F f)              { forth_stack_check(f, F_DATA_STACK);  return f->stack[SP++]; }

scell spop(F f)   { return (scell) POP; }

void fword_dup(F f)     { cell a = POP; PUSH(a); PUSH(a); }
void fword_drop(F f)    { POP; }
void fword_swap(F f)    { cell a = POP; cell b = POP; PUSH(a); PUSH(b); }
void fword_nip(F f)     { SWAP; DROP; }

void fword_2drop(F f)  { POP; POP; }
void fword_over(F f)   { cell b = POP; cell a = POP; PUSH(a); PUSH(b); PUSH(a); } /* a b -> a b a */

/**********************************************************
 *
 * Arithmetic operators
 *
 **********************************************************/

void fword_plus(F f)    { PUSH(SPOP + SPOP); }
void fword_sub(F f)     { PUSH(-SPOP + SPOP); }
void fword_star(F f)    { PUSH(SPOP * SPOP); }
void fword_slash(F f)   { SWAP; PUSH(SPOP / SPOP); }
void fword_and(F f)     { PUSH(POP & POP); }
void fword_or(F f)      { PUSH(POP | POP); }
void fword_xor(F f)     { PUSH(POP ^ POP); }

void fword_negate(F f)   { PUSH(-SPOP); }
void fword_invert(F f)   { PUSH(~SPOP); }

void fword_2star(F f)    { PUSH(SPOP << 1); }
void fword_2slash(F f)   { PUSH(SPOP >> 1); }
void fword_u2slash(F f)  { PUSH(POP >> 1); }

void fword_shift_left(F f)    { cell cnt = POP,       n =  POP; PUSH(n << cnt); }
void fword_shift_right(F f)   { cell cnt = POP; scell n = SPOP; PUSH(n >> cnt); }
void fword_ushift_right(F f)  { cell cnt = POP; cell  n =  POP; PUSH(n >> cnt); }

void fword_uless(F f)  {  cell b =  POP;  cell a =  POP; PUSH(a < b ? -1 : 0); }
void fword_less(F f)   { scell b = SPOP; scell a = SPOP; PUSH(a < b ? -1 : 0); }

void fword_zero_less(F f)   { PUSH(SPOP <  0 ? -1 : 0); }
void fword_zero_equal(F f)  { PUSH( POP == 0 ? -1 : 0); }

/*
 **********************************************************
 *
 * Multiply and Divide
 *
 **********************************************************
 **/

void fword_uslash_mod(F f)  /* u1 u2 -- um uq */
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
void fword_slash_mod(F f)  /* n1 n2 -- m q */
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

/*
 **********************************************************
 *
 * Store and Fetch
 *
 **********************************************************
 **/
void fword_fetch(F f)   { PUSH(mem_load(POP, 0)); }
void fword_cfetch(F f)  { PUSH(mem_loadb(POP, 0)); }

void fword_store(F f)       { cell addr = POP, v = POP; mem_store(addr, 0, v); }
void fword_cstore(F f)      { cell addr = POP, v = POP; mem_storeb(addr, 0, v); }
void fword_plus_store(F f)  { cell addr = POP, v = POP; mem_store(mem_load(addr, 0), 0, v); }


/*
 **********************************************************
 *
 * Printing routines
 *
 **********************************************************
 **/

void fword_dot(F f) { printf("%x ", POP); }


/*
 * Declare static words
 */
static int forth_number_token(F f, cell *n);


/*
 **********************************************************
 *
 * The Dictionary
 *
 **********************************************************
 **/

struct dict_s {
    char *name;
    void (*code)(F f);
};

#include "fword_decls.c"
static struct dict_s fword_dict[] = {
#include "fword_dict.c"
#include "fword_imm_dict.c"
    { NULL, NULL}
};

/*
 * Headers
 */


/*
 **********************************************************
 *
 * Compiling Words
 *
 **********************************************************
 **/

void forth_compile_word(F f, forth_header_t *w)
{
    f->code[f->code_offset++].word = w;
}

void forth_compile_cons(F f, cell n)
{
    f->code[f->code_offset++].cons = n;
}


/*
 **********************************************************
 *
 * Run time words for var, cons, lit, etc.
 *
 **********************************************************
 **/

void forth_do_var(F f, forth_header_t *v) { PUSH(v->n.var); }
void forth_do_cons(F f, forth_header_t *c) { PUSH(c->n.cons); }
void forth_do_lit(F f, forth_header_t *u) { PUSH(IP++ -> cons); }
forth_header_t forth_do_lit_header = { "(lit)", forth_do_lit };
void forth_immediate(F f, forth_header_t *w)
{
    cell n;
    
    int is_number = forth_number_token(f, &n);

    /*
     * This is a last chance word.  If it's not a number, then
     * break out.
     */

    forth_assert(f, is_number, FERR_INVALID_TOKEN,
                 "Word %s wasn't found in the dictionary "
                 "and doesn't look like a number", f->token_string);

    /*
     * Else, put code into the stream to push the number at run time
     */

    forth_compile_word(f, &forth_do_lit_header);
    forth_compile_cons(f, n);
}
    
void forth_do_array(F f, forth_header_t *a)
{
    cell idx = POP;
    if (idx >= a->n.dim) {
        forth_err(f, 55);
    }

    PUSH(a->p.array[idx]);
}

static cell *forth_get_var_address(F f)
{
    forth_header_t *v = IP++ -> word;
    if (v->code != forth_do_var) {
        forth_err(f, 55);
    }

    return &v->n.var;
}

static cell *forth_get_array_address(F f)
{
    forth_header_t *v = IP++ -> word;
    if (v->code != forth_do_array) {
        forth_err(f, 55);
    }

    cell idx = POP;
    if (idx >= v->n.dim) {
        forth_err(f, 55);
    }

    return &v->p.array[idx];
}

void forth_set(F f)  {       *forth_get_var_address(f) = POP; }
void forth_get(F f)  {  PUSH(*forth_get_var_address(f)); }
void forth_seti(F f) {       *forth_get_array_address(f) = POP; }
void forth_geti(F f) {  PUSH(*forth_get_array_address(f)); }

static int forth_get_input_char(F f)
{
    static char last_c;
    char c;

    // Line numbers aren't bumped until we're actually reading
    // the character -after- a new line.
    if (last_c == '\n') {
        f->input_line_cnt ++;
        f->input_line_begin_offset = f->input_offset;
    }

    if (f->input_offset >= f->input_len) return EOF;
    last_c = c = f->input[f->input_offset++];

    return c;
}

static void forth_parse_end(F f, int cons_char)
{
    if (cons_char) {
        f->token_end = f->input_offset -1;
    } else {
        f->token_end = f->input_offset;
    }
}

static void forth_parse(F f, char delim) // delim --
{
    f->token_start = f->input_offset;

    do {
        int c = forth_get_input_char(f);
        if (c == EOF)
            return forth_parse_end(f, 0);
        if (delim == ' ' && isspace(c))
            return forth_parse_end(f, 1);
        else if (delim == c)
            return forth_parse_end(f, 1);
    } while (1);
}

static void forth_token(F f)
{
    int i;

    f->token_start = f->token_end = -1;

    /*
     * Skip white space
     */

    i = f->input_offset;
    while (i < f->input_len && isspace(f->input[i])) {
        if (f->input[i++] == '\n') {
            f->input_line_cnt ++;
            f->input_line_begin_offset = i;
        }
    }
    f->input_offset = i;

    /*
     * Grab a token
     */

    forth_parse(f, ' ');

    i = f->token_start;
    int e = f->token_end;
    if (e - i >= MAX_INPUT_TOKEN_SZ) {
        e = i + MAX_INPUT_TOKEN_SZ -1;
    }

    char *p = f->token_string;
    while (i < e) {
        *p++ = f->input[i++];
    }
    *p = '\0';
}

static forth_header_t *forth_lookup_token(F f)
{
    forth_header_t *p = f->dictionary_head;

    while (p) {
        if (strcmp(f->token_string, p->name) == 0) {
            return p;
        }
        p = p->prev;
    }

    return NULL;
}

static int forth_number_token(F f, cell *n)
{
    int i;
    char *p;

    if (strlen(f->token_string) == 8) {
        for (i = 0; i < 8; i++) {
            if (!isxdigit(f->token_string[i]))
                break;
        }

        if (i == 8) {
            // Assume it's a hex humber
            *n = strtoll(f->token_string, NULL, 16);
            return 1;
        }
    }

    *n = strtoll(f->token_string, &p, 0);
    if (*p == '\0') return 1;
    else            return 0;
}


/*
 * forth_do_colon()
 *
 * Continue executing words until an exit pops an IP and the RP
 * finally returns to its value on entry.
 */

void forth_do_colon(F f, forth_header_t *w)
{
    int rp_saved = RP;

    NEST;
    IP = w->p.body;
    do {
        forth_header_t *w = IP++ -> word;
        CALL(w);
    } while (RP < rp_saved);
}


void forth_do_exit(F f, forth_header_t *w)
{
    UNNEST;
}


forth_header_t forth_exit_header = { "(exit)", forth_do_exit };

void forth_imm_semicolon(F f, forth_header_t *w)
{
    // Compile an exit word
    forth_compile_word(f, &forth_exit_header);
    // Malloc space for the freshly compiled word.
    
}

forth_header_t forth_immediate_header = { "(lit)", forth_immediate, 1 };
forth_header_t forth_colon_header = { "(do-colon)", forth_do_colon };
forth_header_t forth_imm_semicolon_header = { ";", forth_imm_semicolon };


void forth_process_input_line(F f, char *input, int len)
{
    f->input = input;
    f->input_len = len;

    f->input_offset = 0;
    f->input_line_cnt = 1;
    f->input_line_begin_offset = 0;

    f->in_colon = 0;
    f->code_offset = 0;

    while (f->input_offset < f->input_len) {
        forth_token(f);
        if (f->token_start == -1) break;
        forth_header_t *w = forth_lookup_token(f);
        if (!w) w = &forth_immediate_header;
        if (w->immediate)
            w->code(f, w);
        else
            forth_compile_word(f, w);
    }

    forth_compile_word(f, &forth_exit_header);

    forth_header_t anon_input_word = { /* name */ "(input-buffer)",
                                       /* code */ forth_do_colon,
                                       /* immediate */ 0,
                                       /* prev ptr */ NULL,
                                       /* n: var */{ 0 },
                                       /* p: body */ { f->code } };
    CALL(&anon_input_word);
}

F forth_new(void)
{
    F f = calloc(1, sizeof(*f));

    struct dict_s *d = fword_dict;
    forth_header_t *last_p = NULL;
    while (d->name && d->code) {
        forth_header_t *p = calloc(1, sizeof(*p));
        assert(strlen(d->name) + 1 < MAX_HEADER_NAME_SZ);
        memcpy(p->name, d->name, strlen(d->name) + 1);
        p->prev = last_p;
        p->code = (forth_code_word_t) (d->code);
        p->n.var = 0;
        p->p.body = NULL;
        last_p = p;
        d++;
    }

    f->dictionary_head = last_p;
    f->sp = STACK_SIZE;
    f->rp = RSTACK_SIZE;
    f->lp = LSTACK_SIZE;

    return f;
}
