#include "sim.h"
#include "arm.h"

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

reg dovar_addr;
reg docolon_addr;
reg docons_addr;
reg dodoes_addr;

/*
 * Note that the comments denote whether -this- program reads, ignores, or
 * writes values into particular fields.
 */

typedef struct forth_params_s {
    cell            version;        // Read
    cell            entry;          // Read
    cell            ip0;            // Ignore
    cell            rp0;            // Read/Write
    cell            h0;             // Ignore
    cell            sp0;            // Write
    cell            exit_context ;  // Write
    cell            exit_func;      // Write
    cell            type_cb;        // Write
    cell            qkey_cb;        // Write
    cell            key_cb;         // Write
    cell            readline_cb;    // Write
    cell            getfile_cb;     // Write
} forth_params_t;


cell forth_readline(char *buffer, cell len);

static char *forth_lookup_word_name(reg cfa)
{
    if (cfa < addr_base ||
        cfa >= addr_base + addr_size) {
        return NULL;
    }

    reg link = mem_load(cfa, -4);
    if (link &&
        (link <  addr_base &&
         link >= addr_base + addr_size)) {
        return NULL;
    }

    byte len = mem_loadb(cfa, -5);
    if (len > 128) {
        return NULL;
    }

    reg strp = cfa-6;
    while (len-- > 0) {
        char c = mem_loadb(strp--, 0);
        if (!isprint(c)) {
            return NULL;
        }
    }

    // Cache the names?

    strp = cfa - 5;
    len = mem_loadb(strp--, 0);
    char *str = malloc(len + 1);
    str[len] = '\0';
    while (len-- > 0) {
        str[len] = mem_loadb(strp--, 0);
    }
    return str;
}

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
    int fsize;

    fsize = image_ncells * 4;

    if (addr_size < (fsize + base)) {
        printf("ERROR: The Forth image is larger than the region alotted for it.\n");
        return -1;
    }

    base = base + addr_base;

    forth_params_t *fp = (forth_params_t *) kernel_image;

    if (fp->version != 1) {
        return -1;
    }

    int offset = 0;
    cell *p = kernel_image;
    for (int i = 0; i < reloc_ncells; i++) {
        cell bits = reloc_bitmap[i];
        for (int j = 0; j < 32; j++) {
            cell adj;
            if (bits & 1) {
                adj = addr_base;
            } else {
                adj = 0;
            }

            mem_store(base, offset, *p++ + adj);

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
    base += addr_base;
    mem_store(base, offsetof(forth_params_t, exit_context), 0);
    mem_store(base, offsetof(forth_params_t, exit_func), 1);
    mem_store(base, offsetof(forth_params_t, type_cb), 2);
    mem_store(base, offsetof(forth_params_t, readline_cb), 3);
#endif

    return mem_load(base, offsetof(forth_params_t, entry));
}

reg forth_is_header(reg arm_addr)
{
    int len, pad;

    /*
     * Skip padding
     */
    for (pad = 0; pad < 4; pad++) {
        if (mem_loadb(arm_addr, pad) != 0) {
            break;
        }
    }

    if (pad == 4) {
        return 0;  // Not a forth header
    }

    /*
     * Consume ascii characters up to length byte
     */
    for (len = 0; len < 128; len++) {
        byte c = mem_loadb(arm_addr, pad + len);
        if (c == len) {
            ASSERT(c > 0);
            break;
        }
        if (!isprint(c) || c == ' ') {
            return 0; // A non-printable character (or space) in a Forth names?  Nah.
        }
    }

    if ((arm_addr + pad + len + 1) & 3) {
        return 0; // The byte past the length byte should be word aligned
    }

    reg lfa = arm_addr + pad + len + 1;
    reg cfa = lfa + 4;
    reg link = mem_load(lfa, 0);
    if (link && link < arm_addr - MB(1)) {
        return 0;  // Too far away; not a valid link
    }
    if (link > arm_addr) {
        return 0;  // Links never go forward
    }

    printf("\n        : ");
    for (int i = 0; i < len; i++) {
        printf("%c", mem_loadb(arm_addr, pad + i));
    }
    printf("\n");

    reg bl = mem_load(cfa, 0);
    if (arm_decode_instr(bl) == ARM_INSTR_B) {
        reg dest = decode_dest_addr(cfa, bl & 0x00ffffff, 24, 0);
        if (dest == docolon_addr) {
            while (1) {
                cfa += 4;
                reg word_cfa = mem_load(cfa, 0);
                char *word_name = forth_lookup_word_name(word_cfa);
                if (word_name) {
                    printf("%8.8x: %s\n", cfa, word_name);
                    free(word_name);  // Cache the names?
                } else {
                    /*
                     * Handle lit and strings before exiting
                     */
                    return (cfa - arm_addr) / 4;
                }
            }                
        }
    }

    return (pad + len + 1 + 4) / 4;
}

static int check_one_machine(reg addr, const reg *machine)
{
    int i = 0;

    while (machine[i]) {
        reg val = mem_load(addr, i*4);
        if (val != machine[i]) return 0;
        i++;
    }

    return 1;
}

static char *forth_is_machinery(reg addr)
{
    const reg dovar[] = { 0xe52d6004, /* str     top, [sp, -4]! */
                          0xe1a0600e, /* mov     top, lr        */
                          0xe494f004, /* next                   */
                          0
    };

    const reg docons[] = { 0xe52d6004, /* str     top, [sp, -4]! */
                           0xe59e6000, /* ldr     top, [lr]      */
                           0xe494f004, /* next                   */
                           0
    };

    const reg docolon[] = { 0xe5254004, /* str     ip, [rp, -4]! */
                            0xe1a0400e, /* mov     ip, lr        */
                            0xe494f004, /* next                  */
                            0
    };

    const reg dodoes[] = { 0xe5254004, /* str     ip, [rp, -4]!  */
                           0xe1a0400e, /* mov     ip, lr         */
                           0xe52d6004, /* str     top, [sp, -4]! */
                           0xe1a06000, /* mov     top, r0        */
                           0xe494f004, /* next                   */
                           0
    };

#define CHECK(name)							\
    if (check_one_machine(addr, name)) {	\
        name ## _addr = addr;				\
        return #name;						\
    }

    CHECK(dovar)
    CHECK(docons)
    CHECK(docolon)
    CHECK(dodoes)

    return NULL;
}


reg forth_is_word(reg addr)
{
    reg word = mem_load(addr, 0);
    char *machine_name;

    machine_name = forth_is_machinery(addr);
    if (machine_name) {
        printf("\n        : %s\n", machine_name);
        return 0;
    }
        
    if (word == 0xe494f004) {
        /*
         * Next
         */
        printf("%8.8x: e494f004 next\n", addr);
        return 1;
    }

    /*
     * Check to see if the value at addr can be a CFA.
     */

    if (word < addr_base+8) return 0;
    if (word >= addr_base + addr_size) return 0;

    /*
     * Check to see if the word before the CFA is a link field
     * NOTE: Not all code has link fields.  Anonymous code words
     * fall into this category.  :-(
     */

    reg link = mem_load(word -4, 0);
    if (link) { // A link of 0 is valid
        if (link < addr_base) return 0;
        if (link > word) return 0;
    }

    return 0;
}
