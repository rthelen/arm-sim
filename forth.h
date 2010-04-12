/*
 * This file is part of arm-sim: http://madscientistroom.org/arm-sim
 *
 * Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
 * reversed. (See the file COPYRIGHT for details.)
 */

#include <setjmp.h>

typedef  reg  cell;
typedef sreg scell;

/*
 * Word Header
 */

#define MAX_HEADER_NAME_SZ			32

typedef struct forth_environment_s forth_environment_t;
typedef forth_environment_t *F;
typedef struct forth_header_s forth_header_t;
typedef void (*forth_code_word_t)(F f, forth_header_t *word_header);
typedef union forth_body_u {
    forth_header_t	*word;
    int				 br_offset; // offset relative to current body cell for a branch
    cell			 cons;      // constant for literal
} forth_body_t;

struct forth_header_s {
    char				 name[MAX_HEADER_NAME_SZ];
    forth_code_word_t	 code;
    int					 immediate;
    forth_header_t		*prev;
    union {
        cell			 var;    // Var
        cell			 cons;   // Constant
        int				 dim;    // Dimension of array
    } n;
    union {
        forth_body_t	*body;   // Points to the contents of the word body
        cell			*array;    // Points to the contents of an array
    } p;
};

typedef struct forth_state_s {
    int		state;
    int		offset;
} forth_state_t;

typedef struct forth_loop_s {
    int		index;
    int		limit;
} forth_loop_t;

#define MAX_BREAK_POINTS		32
#define MAX_INPUT_CODE_SZ		512
#define MAX_INPUT_TOKEN_SZ		MAX_HEADER_NAME_SZ

#define STACK_SIZE           64
#define RSTACK_SIZE          64
#define LOOP_STACK_SIZE      16
#define STATE_STACK_SIZE     16

struct forth_environment_s {
    forth_header_t *dictionary_head;
    forth_header_t *breakpoints[MAX_BREAK_POINTS];

    int err_num;
    char *err_str;
    jmp_buf forth_jmpbuf;

    int sp;            /* parameter stack pointer */
    int rp;            /* return stack pointer */
    int lp;            /* loop stack pointer */
    int state_sp;      /* state stack pointer */
    forth_body_t *ip;     /* instruction pointer */

    cell           stack[STACK_SIZE];
    forth_body_t *rstack[RSTACK_SIZE];
    forth_loop_t  lstack[LOOP_STACK_SIZE];
    forth_state_t state_stack[STATE_STACK_SIZE];

    char *input;
    int input_len;
    int input_offset;
    int input_line_cnt; // Line number of input {used with files}
    int input_line_begin_offset; // Char number of input {relative to last cr}

    // Start and end of token in the input stream
    char token_string[66];
    int token_start, token_end;
    // Where in input stream did this token occur?
    int token_line_num, token_char_cnt;

    /*
     * Code generation
     */
    int code_offset;
    forth_body_t code[MAX_INPUT_CODE_SZ];

    /*
     * Compiler variables
     */
    forth_header_t *colon_header;
};

#define SP		(f->sp)
#define RP		(f->rp)
#define LP		(f->lp)
#define IP		(f->ip)

#define RPUSH(n)  rpush(f, n)
#define RPOP      rpop(f)

#define LPUSH(n)  lpush(f, n)
#define LPOP      lpop(f)

#define CALL(w)	  ((w)->code(f, w))
#define NEST      RPUSH(IP)
#define UNNEST    (IP = RPOP)

#define MIN(a,b)    (((a) < (b)) ? (a) : (b))

#define PUSH(v)	push(f, v)
#define POP     pop(f)
#define SPOP    spop(f)

#define DUP     fword_dup(f, w)
#define DROP    fword_drop(f, w)
#define SWAP    fword_swap(f, w)
#define NIP     fword_nip(f, w)

/*
 * Stack identifiers
 */

enum {
    F_DATA_STACK = 1,
    F_RETURN_STACK,
    F_LOOP_STACK,
    F_STATE_STACK,
};

/*
 * Forth states
 */
enum {
    F_STATE_COLON = 1,
    F_STATE_DO,
    F_STATE_IF,
};

/*
 * Forth Error Numbers
 */

enum {
    FERR_NEED_MORE_INPUT = -1,
    F_NOERR = 0,
    FERR_STACK_UNDERRUN,
    FERR_STACK_OVERFLOW,
    FERR_RETURN_STACK_UNDERRUN,
    FERR_RETURN_STACK_OVERFLOW,
    FERR_LOOP_STACK_UNDERRUN,
    FERR_LOOP_STACK_OVERFLOW,
    FERR_STATE_STACK_UNDERRUN,
    FERR_STATE_STACK_OVERFLOW,
    FERR_INVALID_TOKEN,
    FERR_EMBEDDED_COLON,
    FERR_SEMICOLON_WOUT_COLON,
    FERR_MISMATCHED_CONTROL,
};

F forth_new(void);
void forth_process_input(F f, char *input, int len);


#define FWORD_HEADER(_name, _str, _imm)                       \
    void _name(F f, forth_header_t *w);                       \
    forth_header_t _name ## _header = { _str, _name, _imm };  \
    void _name(F f, forth_header_t *w)

#define FWORD2(_name, _str)                    \
    FWORD_HEADER(fword_ ## _name, _str, 0)

#define FWORD(_name)                           \
    FWORD2(_name, # _name)

#define FWORD_IMM2(_name, _str)                \
    FWORD_HEADER(fword_ ## _name, _str, 1)

#define FWORD_IMM(_name)                       \
    FWORD_IMM2(_name, # _name)

#define FWORD_DO2(_name, _str)                 \
    FWORD_HEADER(fword_do_ ## _name, _str, 1)

#define FWORD_DO(_name)                        \
    FWORD_DO2(_name, "(" # _name ")")

