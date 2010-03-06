#include "sim.h"

typedef reg	cell;

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
    cell            version;        // Read
    forth_entry     entry;          // Read
    cell            ip0;            // Ignore
    cell            rp0;            // Read/Write
    cell            h0;             // Ignore
    cell            sp0;            // Write
    cell            exit_context ;  // Write
    cell            exit_func;      // Write
    forth_type_cb   type_cb;        // Write
    forth_key_cb    qkey_cb;        // Write
    forth_key_cb    key_cb;         // Write
    forth_readline_cb readline_cb;  // Write
    forth_getfile_cb getfile_cb;    // Write
} forth_params_t;


cell forth_readline(char *buffer, cell len);

int forth_parse_image(void)
{
    cell *fimage;

    if (!image || image_size < sizeof(cell) * 4) {
        return -1;
    }

    fimage = (cell *) image;
    image_ncells = fimage[0];
    reloc_ncells = fimage[1];

    if (image_size < sizeof(cell) * (2 + image_ncells + reloc_ncells)) {
        return -1;
    }

    kernel_image = &fimage[2];
    reloc_bitmap = &fimage[2 + image_ncells];

    return 0;
}

int forth_relocate_image(reg base)
{
    forth_params_t *fp;
    cell *fimage;
    int i, j;
    int fsize, offset;

    fimage = (cell *) image;
    fsize = image_ncells * 4;

    if (addr_size < (fsize + base)) {
        printf("ERROR: The Forth image is larger than the region alotted for it.\n");
        return -1;
    }

    fp = (forth_params_t *) image;

    if (fp->version != 1) {
        return -1;
    }

    offset = 0;
    for (i = 0; i < reloc_ncells; i++) {
        cell bits = reloc_bitmap[i];
        for (j = 0; j < 32; j++) {
            cell adj;
            if (bits & 1) {
                adj = addr_base;
            } else {
                adj = 0;
            }

            mem_store(base, offset, fimage[i] + adj);

            offset += 4;
            bits >>= 1;
        }
    }

//  fp->sp0 = (void *) ((uintptr_t) base + size - (uintptr_t) fp->rp0 - 32);
//  fp->rp0 = (void *) ((uintptr_t) base + size - 32);
    mem_store(base, offsetof(forth_params_t, sp0), 
              addr_base + addr_size - mem_load(base, offsetof(forth_params_t, rp0)) - 32);
    mem_store(base, offsetof(forth_params_t, rp0), addr_base + addr_size - 32);

    return 0;
}

reg forth_init(reg base)
{
#if 0
    forth_params_t *fp = (void *) image;

    fp->exit_context = jmpbuf;
    fp->exit_func = lib_longjmp;
    fp->type_cb = forth_type;
    fp->readline_cb = forth_readline;
#else
    mem_store(base, offsetof(forth_params_t, exit_context), 0);
    mem_store(base, offsetof(forth_params_t, exit_func), 1);
    mem_store(base, offsetof(forth_params_t, type_cb), 2);
    mem_store(base, offsetof(forth_params_t, readline_cb), 3);
#endif

    return mem_load(base, offsetof(forth_params_t, entry));
}
