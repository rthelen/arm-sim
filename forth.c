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
    char *input =
        ": cr 10 emit 13 emit ; "
        "\" Hello\" 32 charcat \" world!\" strcat type cr "
        ".\" Printing 1 - 4\" cr "
        "5 1 do i . loop cr "
        ".\" Printing a 9x9 pyramid of stars\" cr "
        ": star \" \" 42 charcat type ; "
        ": spaces dup 0 > if 0 do 32 emit loop else drop then ; "
        ": stars  dup 0 > if 0 do   star  loop else drop then ; "
        "10 0 do 9 i - spaces i 2* 1 + stars cr loop "
        "3 0 do 9 spaces star cr loop "
        ;
    forth_process_input(f, input, strlen(input));
}

/**********************************************************
 *
 * Some forward references
 *
 **********************************************************/

static int forth_token(F f);
static int forth_parse(F f, char delim);

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

    case F_RETURN_STACK: // Return stack
        err_under = FERR_RETURN_STACK_UNDERRUN;
        err_over  = FERR_RETURN_STACK_OVERFLOW;
        sp = RP;
        sz = RSTACK_SIZE;
        name = "return";
        break;

    case F_LOOP_STACK: // Loop stack
        err_under = FERR_LOOP_STACK_UNDERRUN;
        err_over  = FERR_LOOP_STACK_OVERFLOW;
        sp = LP;
        sz = LOOP_STACK_SIZE;
        name = "loop";
        break;
        
    case F_STATE_STACK: // State stack
        err_under = FERR_STATE_STACK_UNDERRUN;
        err_over  = FERR_STATE_STACK_OVERFLOW;
        sp = f->state_sp;
        sz = STATE_STACK_SIZE;
        name = "state";
        break;
        
    case F_STRING_STACK: // State stack
        err_under = FERR_STRING_STACK_UNDERRUN;
        err_over  = FERR_STRING_STACK_OVERFLOW;
        sp = f->string_sp;
        sz = STRING_STACK_SIZE;
        name = "string";
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

void lpush(F f, forth_loop_t l)  { forth_stack_check(f, F_LOOP_STACK); f->lstack[--LP] = l; }
forth_loop_t lpop(F f)           { forth_stack_check(f, F_LOOP_STACK); return f->lstack[LP++]; }

static void push(F f, cell v)    { forth_stack_check(f, F_DATA_STACK);  f->stack[--SP] = v; }
cell pop(F f)              { forth_stack_check(f, F_DATA_STACK);  return f->stack[SP++]; }

scell spop(F f)   { return (scell) POP; }

static void state_push(F f, forth_state_t state)
{
    forth_stack_check(f, F_STATE_STACK);
    f->state_stack[--(f->state_sp)] = state;
}

static forth_state_t state_pop(F f)
{
    forth_stack_check(f, F_STATE_STACK);
    return f->state_stack[(f->state_sp)++];
}

static int forth_state(F f)
{
    if (f->state_sp == STATE_STACK_SIZE) {
        return 0;
    }

    return f->state_stack[f->state_sp].state;
}

static void string_push(F f, forth_string_t str)
{
    forth_stack_check(f, F_STRING_STACK);
    f->string_stack[--(f->string_sp)] = str;
}

static forth_string_t string_pop(F f)
{
    forth_stack_check(f, F_STRING_STACK);
    return f->string_stack[(f->string_sp)++];
}

static void string_free(F f, forth_string_t str)
{
    free(str.buf);
}
    

/**********************************************************
 *
 * The Most Rudimentary Stack Operators
 *
 **********************************************************/

FWORD(dup)      { cell a = POP; PUSH(a); PUSH(a); }
FWORD(drop)     { POP; }
FWORD(swap)     { cell a = POP; cell b = POP; PUSH(a); PUSH(b); }
FWORD(nip)      { SWAP; DROP; }
FWORD(2drop)    { POP; POP; }

            /* over:  a b -> a b a */
FWORD(over)         { cell b = POP; cell a = POP; PUSH(a); PUSH(b); PUSH(a); }

/**********************************************************
 *
 * Arithmetic operators
 *
 **********************************************************/

FWORD2(plus, "+")    { PUSH(SPOP + SPOP); }
FWORD2(sub, "-")     { scell b = SPOP; scell a = SPOP; PUSH(a - b); }
FWORD2(star, "*")    { PUSH(SPOP * SPOP); }
FWORD2(slash, "/")   { scell b = SPOP; scell a = SPOP; PUSH(a / b); }
FWORD2(and, "and")   { PUSH(POP & POP); }
FWORD2(or, "or")     { PUSH(POP | POP); }
FWORD2(xor, "xor")   { PUSH(POP ^ POP); }

FWORD2(negate, "negate")   { PUSH(-SPOP); }
FWORD2(invert, "invert")   { PUSH(~SPOP); }

FWORD2(2star, "2*")        { PUSH(SPOP << 1); }
FWORD2(2slash, "2/")       { PUSH(SPOP >> 1); }
FWORD2(u2slash, "u2/")     { PUSH(POP >> 1); }

FWORD2(shift_left, "<<")
{ cell cnt = POP,       n =  POP; PUSH(n << cnt); }

FWORD2(shift_right, ">>")
{ cell cnt = POP; scell n = SPOP; PUSH(n >> cnt); }

FWORD2(ushift_right, "u>>")
{ cell cnt = POP; cell  n =  POP; PUSH(n >> cnt); }

FWORD2(uless, "u<")
{  cell b =  POP;  cell a =  POP; PUSH(a < b ? -1 : 0); }

FWORD2(less, "<")
{ scell b = SPOP; scell a = SPOP; PUSH(a < b ? -1 : 0); }

FWORD2(greater, ">")
{ scell b = SPOP; scell a = SPOP; PUSH(a > b ? -1 : 0); }

FWORD2(zero_less, "0<")
{ PUSH(SPOP <  0 ? -1 : 0); }

FWORD2(zero_equal, "=")
{ PUSH( POP == 0 ? -1 : 0); }

/*
 **********************************************************
 *
 * Multiply and Divide
 *
 **********************************************************
 **/

FWORD2(uslash_mod, "u/mod")  /* u1 u2 -- um uq */
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
FWORD2(slash_mod, "/mod")  /* n1 n2 -- m q */
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

FWORD2(fetch, "@")    { PUSH(mem_load(POP, 0)); }
FWORD2(cfetch, "c@")  { PUSH(mem_loadb(POP, 0)); }

FWORD2(store, "!")        { cell addr = POP, v = POP; mem_store(addr, 0, v); }
FWORD2(cstore, "c!")      { cell addr = POP, v = POP; mem_storeb(addr, 0, v); }
FWORD2(plus_store, "+!")  { cell addr = POP, v = POP; mem_store(mem_load(addr, 0), 0, v); }


/*
 **********************************************************
 *
 * Printing routines
 *
 **********************************************************
 **/

FWORD2(dot, ".")      { printf("%x ", POP); fflush(stdout); }
FWORD(emit)           { printf("%c", POP); }
FWORD(type)
{
    forth_string_t str = string_pop(f);
    for (int i = 0; i < str.len; i++) {
        printf("%c", str.buf[i]);
    }
    string_free(f, str);
}


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

void forth_compile_offset(F f, int n)
{
    f->code[f->code_offset++].n = n;
}


/*
 **********************************************************
 *
 * Run time words for var, cons, lit, etc.
 *
 **********************************************************
 **/

FWORD_DO(var)   { PUSH(w->n.var); }
FWORD_DO(cons)  { PUSH(w->n.cons); }
FWORD_DO(lit)   { PUSH(IP++ -> cons); }

static cell *forth_get_var_address(F f)
{
    forth_header_t *v = IP++ -> word;
    if (v->code != fword_do_var) {
        forth_err(f, 55);
    }

    return &v->n.var;
}

static cell *forth_get_array_address(F f)
{
    forth_header_t *v = IP++ -> word;
    cell idx = POP;

    if (idx >= v->n.dim) {
        forth_err(f, 55);
    }

    return &v->p.array[idx];
}

FWORD_DO(set)  {       *forth_get_var_address(f) = POP; }
FWORD_DO(get)  {  PUSH(*forth_get_var_address(f)); }
FWORD_DO(seti) {       *forth_get_array_address(f) = POP; }
FWORD_DO(geti) {  PUSH(*forth_get_array_address(f)); }

/**********************************************************
 *
 * Branch Words
 *
 **********************************************************/

FWORD_DO(branch)
{
    int offset = (IP ++) -> n;
    IP += offset;
}

FWORD_DO(zbranch)
{
    if (POP == 0) {
        fword_do_branch(f, w);
    } else {
        IP ++;  // Skip branch offset
    }
}

/**********************************************************
 *
 * Control Words
 *
 **********************************************************/

static void forth_mark(F f, forth_header_t *branch_word, int state_type)
{
    if (branch_word) {
        forth_compile_word(f, branch_word);
        forth_compile_offset(f, 0); // Unknown at this time
    }

    forth_state_t mark = { state_type, f->code_offset };
    state_push(f, mark);
}

static int forth_resolve(F f, int state_type)
{
    forth_assert(f, forth_state(f) == state_type,
                 FERR_MISMATCHED_CONTROL,
                 "Control words must be matched properly: if [else] then, do loop, : ; etc.");
    forth_state_t mark = state_pop(f);
    assert(mark.state == state_type);  // Logic error in this code if not
    f->code[mark.offset -1].n = f->code_offset - mark.offset;
    return mark.offset;
}

static void forth_back_branch(F f, forth_header_t *branch_word, int target_offset)
{
    forth_compile_word(f, branch_word);
    forth_compile_offset(f, - (f->code_offset + 1 - target_offset));
}

FWORD_IMM(if)
{
    forth_mark(f, &fword_do_zbranch_header, F_STATE_IF);
}

FWORD_IMM(else)
{
    forth_assert(f, forth_state(f) == F_STATE_IF,
                 FERR_MISMATCHED_CONTROL,
                 "else must follow an if");
    forth_state_t if_mark = state_pop(f);
    forth_mark(f, &fword_do_branch_header, F_STATE_IF);
    state_push(f, if_mark);
    forth_resolve(f, F_STATE_IF);
}

FWORD_IMM(then)
{
    forth_resolve(f, F_STATE_IF);
}

FWORD_DO(do)
{
    int start = POP;
    int limit = POP;
    forth_loop_t do_loop = { start, limit };
    lpush(f, do_loop);
}

FWORD_DO(loop)
{
    forth_loop_t do_loop = lpop(f);
    do_loop.index ++;
    if (do_loop.index >= do_loop.limit) {
        IP ++;
    } else {
        lpush(f, do_loop);
        fword_do_branch(f, w);
    }
}

FWORD(i)
{
    forth_loop_t do_loop = lpop(f);
    push(f, do_loop.index);
    lpush(f, do_loop);
}    


FWORD_IMM(do)
{
    forth_compile_word(f, &fword_do_do_header);
    forth_mark(f, NULL, F_STATE_DO);
}

FWORD_IMM(loop)
{
    forth_assert(f, forth_state(f) == F_STATE_DO,
                 FERR_MISMATCHED_CONTROL,
                 "loop must follow a do");
    forth_state_t do_mark = state_pop(f);
    forth_back_branch(f, &fword_do_loop_header, do_mark.offset);
}
    

/**********************************************************
 *
 * Defining Words (e.g., : ;)
 *
 **********************************************************/

/*
 * colon()
 *
 * Continue executing words until an exit pops an IP and the RP
 * finally returns to its value on entry.
 */

FWORD_DO(colon)
{
    int rp_saved = RP;

    NEST;
    IP = w->p.body;
    do {
        forth_header_t *w = IP++ -> word;
        CALL(w);
    } while (RP < rp_saved);
}


FWORD_DO(exit)
{
    UNNEST;
}


FWORD_IMM2(colon, ":")
{
    // Verfiy we're not currently compiling or otherwise encumbered
    forth_assert(f, !forth_state(f),
                 FERR_EMBEDDED_COLON,
                 "Cannot use the colon word inside a colon, do, if, etc.");
    assert(f->colon_header == NULL);

    // Switch the compiler on
    forth_mark(f, &fword_do_branch_header, F_STATE_COLON);

    // Fetch the next token, i.e., the name of the new word
    int token_found = forth_token(f);
    forth_assert(f, token_found, FERR_NEED_MORE_INPUT, "");

    // Allocate memory for the header
    forth_header_t *p = calloc(1, sizeof(forth_header_t));

    // Populate the header
    bcopy(f->token_string, &p->name[0], MAX_HEADER_NAME_SZ);
    p->name[MAX_HEADER_NAME_SZ-1] = '\0'; // Just in case
    p->code = fword_do_colon;
    p->immediate = 0;
    p->prev = f->dictionary_head;

    // Note the header in the forth environment
    f->colon_header = p;
}


FWORD_IMM2(semicolon, ";")
{
    forth_assert(f, forth_state(f) == F_STATE_COLON,
                 FERR_SEMICOLON_WOUT_COLON,
                 "Semicolon (;) can only be used used to terminate a colon definition");

    // Compile an exit word
    forth_compile_word(f, &fword_do_exit_header);

    int start = forth_resolve(f, F_STATE_COLON);
    assert(f->colon_header);

    // Malloc space for the freshly compiled word.
    int code_len = f->code_offset - start;
    forth_body_t *body = calloc(code_len, sizeof(forth_body_t));
    bcopy(&f->code[start], body, code_len * sizeof(forth_body_t));
    f->colon_header->p.body = body;
    f->dictionary_head = f->colon_header;
    f->colon_header = NULL;
}


/**********************************************************
 *
 * String Operators
 *
 **********************************************************/

FWORD_DO(quote)
{
    int len = IP++ -> n;

    char *buf = malloc(len + 1);
    forth_body_t *quote_buf = f->ip - 2 - len;
    for (int i = 0; i < len; i++) {
        buf[i] = quote_buf[i].c;
    }
    buf[len] = '\0';

    forth_string_t str = {len, buf};
    string_push(f, str);
}

FWORD_IMM2(quote, "\"")
{
    int term_qoute_found = forth_parse(f, '\"');

    forth_assert(f, term_qoute_found,
                 FERR_NEED_MORE_INPUT, "");

    forth_mark(f, &fword_do_branch_header, F_STATE_QUOTE);

    // Carry forward the string to the compiled word
    int len = f->token_end - f->token_start;
    char *buf = &f->input[f->token_start];
    for (int i = 0; i < len; i ++) {
        f->code[f->code_offset++].c = buf[i];
    }
    
    (void) forth_resolve(f, F_STATE_QUOTE);

    // Compile the quote handler
    forth_compile_word(f, &fword_do_quote_header);

    // Tag the length
    f->code[f->code_offset++].n = len;
}

FWORD_IMM2(dot_quote, ".\"")
{
    fword_quote(f, w);
    forth_compile_word(f, &fword_type_header);
}

FWORD(charcat)
{
    forth_string_t str = string_pop(f);
    str.buf = realloc(str.buf, str.len + 1);
    str.buf[str.len++] = pop(f);
    str.buf[str.len] = '\0';
    string_push(f, str);
}

FWORD(strcat)
{
    forth_string_t str2 = string_pop(f);
    forth_string_t str1 = string_pop(f);
    str1.len += str2.len;
    str1.buf = realloc(str1.buf, str1.len +1);
    strcat(str1.buf, str2.buf);
    string_push(f, str1);
    string_free(f, str2);
}
    

/**********************************************************
 *
 * Input Processing Routines
 *
 **********************************************************/

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

static int forth_parse_end(F f, int cons_char)
{
    if (cons_char) {
        f->token_end = f->input_offset -1;
    } else {
        f->token_end = f->input_offset;
    }

    /* It turns out cons_char (consume character) is also is also equal to
     * whether or not the delimter was found.
     */

    return cons_char;
}

static int forth_parse(F f, char delim) // delim --
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

static int forth_token(F f)
{
    int i;

    f->token_start = f->token_end = -1;

    if (f->input_offset >= f->input_len) {
        return 0;
    }

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
     * Parse for the next white space delimited token
     */

    (void) forth_parse(f, ' ');

    i = f->token_start;
    int e = f->token_end;
    if (e <= i) return 0;

    if (e - i >= MAX_INPUT_TOKEN_SZ) {
        e = i + MAX_INPUT_TOKEN_SZ -1;
    }

    char *p = f->token_string;
    while (i < e) {
        *p++ = f->input[i++];
    }
    *p = '\0';

    return 1;
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

FWORD_DO(number)
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

    forth_compile_word(f, &fword_do_lit_header);
    forth_compile_cons(f, n);
}

void forth_process_input(F f, char *input, int len)
{
    f->input = input;
    f->input_len = len;

    f->input_offset = 0;
    f->input_line_cnt = 1;
    f->input_line_begin_offset = 0;

    f->code_offset = 0;
    f->colon_header = NULL;

    while (forth_token(f)) {
        forth_header_t *w = forth_lookup_token(f);
        if (!w) w = &fword_do_number_header;
        if (w->immediate)
            w->code(f, w);
        else
            forth_compile_word(f, w);
    }

    forth_compile_word(f, &fword_do_exit_header);

    forth_header_t anon_input_word = { /* name */ "(input-buffer)",
                                       /* code */ fword_do_colon,
                                       /* immediate */ 0,
                                       /* prev ptr */ NULL,
                                       /* n: var */{ 0 },
                                       /* p: body */ { f->code } };
    CALL(&anon_input_word);
}

/**********************************************************
 *
 * Initialization Routines
 *
 **********************************************************/

forth_header_t *dictionary_ptrs[] = {
#include "fwords.inc"
    NULL
};

F forth_new(void)
{
    F f = calloc(1, sizeof(*f));

    forth_header_t *last_ptr = NULL;
    for (int i = 0; dictionary_ptrs[i]; i++) {
        dictionary_ptrs[i]->prev = last_ptr;
        last_ptr = dictionary_ptrs[i];
    }
    assert(last_ptr != NULL);

    f->dictionary_head = last_ptr;
    f->sp = STACK_SIZE;
    f->rp = RSTACK_SIZE;
    f->lp = LOOP_STACK_SIZE;
    f->state_sp = STATE_STACK_SIZE;
    f->string_sp = STRING_STACK_SIZE;

    return f;
}
