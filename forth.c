#include "arm.h"

typedef void (*forth_type_cb)(char *str, cell len);
typedef cell (*forth_key_cb)(void);
typedef cell (*forth_readline_cb)(char *buffer, cell len);
typedef char *(*forth_getfile_cb)(char *str, cell len);
typedef void (*forth_entry)(void *base);

/*
 * Layout of the FORTH kernel image
 */

cell image_ncells;  // Cells
cell reloc_ncells;  // Cells
cell *kernel_image;
cell *reloc_bitmap;

/*
 * Note that the comments denote whether -this- program reads, ignores, or
 * writes values into particular fields.
 */

typedef struct forth_params_s {
    cell          version;        // Read
    forth_entry     entry;          // Read
    void            *ip0;           // Ignore
    void            *rp0;           // Read/Write
    void            *h0;            // Ignore
    void            *sp0;           // Write
    void            *exit_context;  // Write
    void            *exit_func;     // Write
    forth_type_cb   type_cb;        // Write
    forth_key_cb    qkey_cb;        // Write
    forth_key_cb    key_cb;         // Write
    forth_readline_cb readline_cb;  // Write
    forth_getfile_cb getfile_cb;    // Write
} forth_params_t;


cell forth_readline(char *buffer, cell len);

int forth_parse_image(void)
{
    cell *p;

    if (!image || image_size < sizeof(cell) * 4) {
        return -1;
    }

    p = (cell *) image;
    image_ncells = p[0];
    reloc_ncells = p[1];

    if (image_size < sizeof(cell) * (2 + image_ncells + reloc_ncells)) {
        return -1;
    }

    kernel_image = &p[2];
    reloc_bitmap = &p[2 + image_ncells];

    return 0;
}

int forth_relocate_image(void)
{
    forth_params_t *fp;
    cell *p;
    cell *r;
    int i, j;
    int shrinkage;
    int fsize;

    fsize = image_ncells * 4;

    if (addr_size < fsize) {
        xprintf("ERROR: The Forth image is larger than the region alotted for it.\n");
        return -1;
    }

    memcpy(addr_base, kernel_image, fsize);

    p = (cell *)memory;
    r = reloc_bitmap;
    for (i = 0; i < reloc_ncells; i++) {
        cell bits = *r++;
        for (j = 0; j < 32; j++) {
            if (bits & 1) {
                /* Add base to base[i] */
                *p += addr_base;
            }
            bits >>= 1;
            p++;
        }
    }

    fp = (forth_params_t *) memory;

    if (fp->version != 1) {
        return -1;
    }

    fp->sp0 = (void *) ((uintptr_t) base + size - (uintptr_t) fp->rp0 - 32);
    fp->rp0 = (void *) ((uintptr_t) base + size - 32);

    return 0;
}

static void *forth_init(cell *base, jmp_buf jmpbuf)
{
    forth_params_t *fp = (void *) base;

    fp->exit_context = jmpbuf;
    fp->exit_func = lib_longjmp;
    fp->type_cb = forth_type;
    fp->readline_cb = forth_readline;

    return fp->entry;
}


typedef int (*forth_entry_t)(cell *base);
