/*
 * This file is part of arm-sim: http://madscientistroom.org/arm-sim
 *
 * Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
 * reversed. (See the file COPYRIGHT for details.)
 */

typedef  reg  cell;
typedef sreg scell;

/*
 * Word Header
 */

#define MAX_HEADER_NAME_SZ			32

typedef struct fth_header_s fth_header_t;
typedef void (*word_t)(fth_header_t *word_header);
typedef union fth_body_u {
    fth_header_t	*word;
    int				 br_offset; // offset relative to current body cell for a branch
    cell			 cons;      // constant for literal
} fth_body_t;

struct fth_header_s {
    char			 name[MAX_HEADER_NAME_SZ];
    fth_header_t	*prev;
    word_t			 func;
    union {
        cell		 var;    // Var
        cell		 cons;   // Constant
        int			 dim;    // Dimension of array
    } n;
    union {
        fth_body_t	*body;   // Points to the contents of the word body
        cell		*val;    // Points to the contents of an array
    } p;
};

#define MAX_BREAK_POINTS		32
#define MAX_INPUT_CODE_SZ		512
#define MAX_INPUT_LINE_SZ		2000
#define MAX_INPUT_TOKEN_SZ		MAX_HEADER_NAME_SZ

#define STACK_SIZE      4096
#define RSTACK_SIZE     64
#define LSTACK_SIZE     16

typedef struct fth_environment_s {
    fth_header_t *dictionary_head;
    fth_header_t *breakpoints[MAX_BREAK_POINTS];

    int err_num;
    char *err_str;
    jmp_buf forth_jmpbuf;

    int sp;            /* parameter stack pointer */
    int rp;            /* return stack pointer */
    int lp;            /* loop stack pointer */
    fth_body_t *ip;     /* instruction pointer */

    cell         stack[STACK_SIZE];
    fth_body_t *rstack[RSTACK_SIZE];
    int         lstack[LSTACK_SIZE];

    char *input;
    int input_len;
    int input_offset;
    int input_line_cnt; // Line number of input {used with files}
    int input_char_cnt; // Char number of input {relative to last cr}

    // Start and end of token in the input stream
    char token_string[
    int token_start, token_end;
    // Where in input stream did this token occur?
    int token_line_num, token_char_cnt;

    /*
     * Compiling variables
     */
    int in_colon;
    fth_body_t code[MAX_INPUT_CODE_SZ];
    int code_offset;
} fth_environment_t;
typedef fth_environment_t *F;

#define SP		(f->sp)
#define RP		(f->rp)
#define LP		(f->lp)
#define IP		(f->ip)

#define RPUSH(n)  fth_rpush(f, n)
#define RPOP      fth_rpop(f)

#define LPUSH(n)  fth_lpush(f, n)
#define LPOP      fth_lpop(f)

#define NEST      RPUSH(IP)
#define UNNEST    (IP = RPOP)

#define MIN(a,b)    (((a) < (b)) ? (a) : (b))

#define PUSH(v)	fth_push(f, v)
#define POP     fth_pop(f)
#define SPOP    fth_spop(f)
#define DUP     fth_dup(f)
#define DROP    fth_drop(f)
#define SWAP    fth_swap(f)
#define NIP     fth_nip(f)

/*
 * Stack identifiers
 */

enum {
    F_DATA_STACK = 1,
    F_RETURN_STACK,
    F_LOOP_STACK
};

/*
 * Forth Error Numbers
 */

enum {
    F_NOERR,
    FERR_STACK_UNDERRUN,
    FERR_STACK_OVERFLOW,
    FERR_RETURN_STACK_UNDERRUN,
    FERR_RETURN_STACK_OVERFLOW,
    FERR_LOOP_STACK_UNDERRUN,
    FERR_LOOP_STACK_OVERFLOW,
};
