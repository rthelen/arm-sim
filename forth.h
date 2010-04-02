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

typedef struct as_header_s as_header_t;
typedef void (*word_t)(as_header_t *word_header);
typedef union as_body_u {
    as_header_t		*word;
    int				 br_offset; // offset relative to current body cell for a branch
    cell			 cons;      // constant for literal
} as_body_t;

struct as_header_s {
    char			 name[MAX_HEADER_NAME_SZ];
    as_header_t		*prev;
    word_t			 func;
    union {
        cell		 var;    // Var
        cell		 cons;   // Constant
        int			 dim;    // Dimension of array
    } n;
    union {
        as_body_t	*body;   // Points to the contents of the word body
        cell		*val;    // Points to the contents of an array
    } p;
};

#define MAX_BREAK_POINTS		32

#define STACK_SIZE      4096
#define RSTACK_SIZE     64
#define LSTACK_SIZE     16

#define RPUSH(n)  rpush(n)
#define RPOP      rpop()

#define LPUSH(n)  lpush(n)
#define LPOP      lpop()

#define NEST      rpush(IP)
#define UNNEST    (IP = RPOP)

#define MIN(a,b)    (((a) < (b)) ? (a) : (b))

#define PUSH(v)	push(v)
#define POP   as_pop()
#define SPOP  as_spop()
#define DUP   as_dup()
#define DROP  as_drop()
#define SWAP  as_swap()
#define NIP   as_nip()

