#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>

typedef uint32_t reg;
typedef uint8_t  byte;

int sim_done;

#define FALSE		(0)
#define TRUE		(!FALSE)

#define GB(x)	((x) << 30)
#define MB(x)	((x) << 20)
#define KB(x)	((x) << 10)

#define BITS(val, bit, nbits)   (((val) >> (bit)) & ((1 << (nbits)) -1))
#define BIT(val, bit)           (((val) >> (bit)) & 1)
#define BPAT1(a)	        	(a)
#define BPAT2(b, a)				(BPAT1(b) << 1 | BPAT1(a))
#define BPAT3(c, b, a)			(BPAT1(c) << 2 | BPAT2(b,a))
#define BPAT4(d, b, c, a)   	(BPAT1(d) << 3 | BPAT3(c,b,a))
#define BPAT5(e, d, b, c, a)    (BPAT1(e) << 4 | BPAT4(d,c,b,a))

/*
 * If a BAD memory access is made, then the following value is returned
 * from the memory subsystem.  Subsequent fetches from this address will
 * result the same value.  Executing code at this address causes an
 * unaligned fault.  This value is decoded as a SWI and those aren't
 * supported in this simulator, so that will cause an illegal instruction
 * fault.  And, last, the low order byte is 0x11 which is a non-printing
 * ASCII value.
 */

#define BAD_MEMVAL	0xEFEDCE11

#define offsetof(_struct, _field)   ((byte *)&(((_struct *)0) -> _field) - (byte *)0)

#define ASSERT(x)	assert(x)

extern byte *memory;

extern reg dovar_addr;
extern reg docolon_addr;
extern reg docons_addr;
extern reg dodoes_addr;

extern int undo_disable;

void brkpoint(void);
void debug_if(int flag);
void warn(const char *fmt, ...);
void error(const char *fmt, ...);
void unpredictable(const char *fmt, ...);

void memory_more(reg base, reg size);
reg mem_ram_base(void);
reg mem_ram_size(void);
void mem_set_image_size(int image_size);
void mem_set_image_base(reg base);
reg  mem_image_base(void);
reg mem_image_size(void);
int mem_addr_is_valid(reg addr);
int mem_range_is_valid(reg arm_addr, reg size);
void *memory_range(reg arm_addr, reg arm_size);
void mem_store(reg arm_addr, reg arm_offset, reg val);
reg mem_load(reg arm_addr, reg arm_offset);
    void mem_storeb(reg arm_addr, reg arm_offset, byte val);
byte mem_loadb(reg arm_addr, reg arm_offset);
void mem_dump(reg arm_addr, reg arm_numwords);

typedef struct file_s {
    FILE *fp;
    char *name;
    byte *image;
    size_t image_size;
    reg base;
    reg size;
} file_t;

file_t *file_load(char *fname);
void file_free(file_t *file);
void file_put_in_memory(file_t *file, reg base);

file_t *forth_init(char *filename, reg base, reg size);
reg forth_entry(file_t *file);

reg forth_is_header(reg arm_addr);
reg forth_is_word(reg addr);
reg forth_is_string(reg addr);

void io_write(reg str, reg len);
reg io_readline(reg buffer, reg len);
reg io_readfile(reg filename, reg len);

void undo_record_reg(int reg_num);
void undo_record_flags(void);
void undo_record_memory(reg address);
void undo_finish_instr(void);
int undo(int num_steps);
int redo(int num_steps);
int undo_size(void);
int redo_size(void);

void disassemble(reg addr, reg instr, char *buff, int sz);
