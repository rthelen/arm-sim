#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>

typedef uint32_t reg;
typedef uint8_t  byte;

#define MB(x)	((x) << 20)
#define KB(x)	((x) << 10)
#define BAD_MEMVAL	0xBADCE11;

#define offsetof(_struct, _field)   ((byte *)&(((_struct *)0) -> _field) - (byte *)0)

#define ASSERT(x)	assert(x)

extern byte *memory;

extern reg addr_base;
extern reg addr_size;

extern byte *image;
extern int image_size;

void warn(const char *fmt, ...);

void init_memory(reg base, reg size);
void *memory_range(reg arm_addr, reg arm_size);
void mem_store(reg arm_addr, reg arm_offset, reg val);
reg mem_load(reg arm_addr, reg arm_offset);
    void mem_storeb(reg arm_addr, reg arm_offset, byte val);
byte mem_loadb(reg arm_addr, reg arm_offset);
void mem_dump(reg arm_addr, reg arm_numwords);

int image_load(char *fname);

int forth_parse_image(void);
int forth_relocate_image(reg base);
reg forth_init(reg base);

void io_write(reg str, reg len);
reg io_readline(reg buffer, reg len);

void undo_record_reg(int reg_num);
void undo_record_flags(void);
void undo_record_memory(reg address);
int undo(int num_steps);
int redo(int num_steps);
int undo_size(void);
int redo_size(void);
