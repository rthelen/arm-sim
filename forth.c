#include "sim.h"
#include "arm.h"

typedef reg	cell;

/*
 * Layout of the FORTH kernel image
 */

static cell sp0, rp0;

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
    cell            sync_caches_cb; // Write
} forth_params_t;


cell forth_readline(char *buffer, cell len);

char *forth_lookup_word_name(reg cfa)
{
    if (cfa == dovar_addr) {
        return strdup("dovar");
    }

    if (cfa == docolon_addr) {
        return strdup("docolon");
    }

    if (cfa == docons_addr) {
        return strdup("docons");
    }

    if (cfa == dodoes_addr) {
        return strdup("dodoes");
    }

    if (!mem_addr_is_valid(cfa)) {
        return NULL;
    }

    if (!mem_range_is_valid(cfa - 8, 12)) {
        return NULL;
    }

    reg link = mem_load(cfa, -4);
    if (link && !mem_range_is_valid(link, 4)) {
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

file_t *forth_init(char *filename, reg base, reg size)
{
    file_t *forth_file;
    cell *fimage;
    int fsize;
    cell kernel_ncells;  // Cells
    cell reloc_ncells;  // Cells
    cell *kernel_image;
    cell *reloc_bitmap;

    forth_file = file_load(filename);
    if (!forth_file) {
        error("Couldn't load image %s", filename);
    }

    if (forth_file->image_size < sizeof(cell) * 4) {
        error("Forth image is smaller than the Forth image header, %d bytes", sizeof(cell) * 4);
    }

    forth_file->base = base;
    fimage = (cell *) forth_file->image;
    kernel_ncells =  fimage[0];
    reloc_ncells =  fimage[1];
    kernel_image = &fimage[2];
    reloc_bitmap = &fimage[2 + kernel_ncells];

    fsize = kernel_ncells * 4;
    forth_file->size = fsize;

    if (size < fsize) {
        error("The Forth image is larger than the region alotted for it.");
    }

    forth_params_t *fp = (forth_params_t *) kernel_image;

    if (fp->version != 1) {
        error("The Forth image isn't compatible with this version of the simulator");
    }

    int offset = 0;
    cell *p = kernel_image;
    for (int i = 0; i < reloc_ncells; i++) {
        cell bits = reloc_bitmap[i];
        for (int j = 0; j < 32; j++) {
            if (p == &kernel_image[kernel_ncells]) {
                // NOTE that reloc_ncells/32 is possibly larger than kernel_ncells.
                // This early exit prevents us from writing garbage beyond the end
                // of the kernel.
                break;
            }

            mem_store(base, offset, *p + ((bits & 1) ? base : 0));
            p ++;
            offset += 4;
            bits >>= 1;
        }
    }

//  fp->sp0 = (void *) ((uintptr_t) base + size - (uintptr_t) fp->rp0 - 32);
//  fp->rp0 = (void *) ((uintptr_t) base + size - 32);

    cell rp_size = mem_load(base, offsetof(forth_params_t, rp0));
    sp0 = base + size - rp_size - 32;
    rp0 = base + size - 32;
    mem_store(base, offsetof(forth_params_t, sp0), sp0);
    mem_store(base, offsetof(forth_params_t, rp0), rp0);
    mem_store(base, offsetof(forth_params_t, exit_context), 0);
    mem_store(base, offsetof(forth_params_t, exit_func), 1);
    mem_store(base, offsetof(forth_params_t, type_cb), 2);
    mem_store(base, offsetof(forth_params_t, readline_cb), 3);
    mem_store(base, offsetof(forth_params_t, getfile_cb), 4);
    mem_store(base, offsetof(forth_params_t, sync_caches_cb), 5);

    return forth_file;
}

reg forth_entry(file_t *file)
{
    reg pc = mem_load(file->base, offsetof(forth_params_t, entry));

    return pc;
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
            cfa += 4;
            return (cfa - arm_addr) / 4;
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
    char *machine_name = machine_name = forth_is_machinery(addr);
    if (machine_name) {
        printf("\n        : %s\n", machine_name);
        return 0;
    }
        
    reg word = mem_load(addr, 0);
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

    if (!mem_range_is_valid(word -8, 12)) return 0;

    /*
     * Check to see if the word before the CFA is a link field
     * NOTE: Not all code has link fields.  Anonymous code words
     * fall into this category.  :-(
     */

    char *name = forth_lookup_word_name(word);
    if (!name) return 0;

    printf("%8.8x: %s", addr, name);
    reg count = 1;
#define MATCH(str)	(!strcmp(name, str))
    if (MATCH("(do)") || 
        MATCH("(branch)") ||
        MATCH("(0branch)") ||
        MATCH("(=0branch)") ||
        MATCH("(loop)") ||
        MATCH("(next)") ||
        MATCH("(?for)") ||
        MATCH("(+loop)") ||
        MATCH("(;code@)")) {
        printf("  %8.8x", mem_load(addr, 4));
        count ++;
    } else if (MATCH("lit")) {
        printf("  #%8.8x", mem_load(addr, 4));
        count ++;
    }

    printf("\n");
    free(name);

    return count;
}

reg forth_is_string(reg addr)
{
    reg strlen = mem_load(addr, 0);
    if (strlen > 256) return 0;
    if (strlen == 0) return 0;

    for (int i = 0; i < strlen; i++) {
        byte c = mem_loadb(addr, 4+i);
        if (!isprint(c) && !isspace(c)) return 0;
    }

    printf("%8.8x: \" ", addr);
    for (int i = 0; i < strlen; i++) {
        byte c = mem_loadb(addr, 4+i);
        if (isprint(c)) printf("%c", c);
        else if (c == ' ') printf(" ");
        else if (c == '\n') printf("\\n");
        else if (c == '\r') printf("\\r");
        else if (c == '\t') printf("\\t");
        else printf("\\%d%d%d", BITS(c, 6,2), BITS(c, 3,3), BITS(c, 0,3));
    }
    printf("\"\n");

    return (4 + strlen + 3) >> 2;
}

void forth_word(reg ip)
{
    char *word_name = NULL;

    if (mem_addr_is_valid(ip)) {
        word_name = forth_lookup_word_name(ip);
        reg t = ip;
        while (!word_name && mem_addr_is_valid(t)) {
            t -= 4;
            word_name = forth_lookup_word_name(t);
        }
        if (word_name) {
            printf("%s  ", word_name);
            return;
        }
    }

    printf("%8.8x  ", ip);
}

void forth_backtrace(void)
{
    cell rp = arm_get_reg(RP);

    if (!mem_range_is_valid(rp, rp0 - rp)) return;

    char *word_name = forth_lookup_word_name(arm_get_reg(PC));
    if (word_name) {
        if (strcmp(word_name, "^") == 0) {
            return;
        }
        printf("Back trace: ");
        printf("%s  ", word_name);
        free(word_name);
    } else {
        return;
    }

    forth_word(arm_get_reg(IP));

    while (rp < rp0) {
        cell ip = mem_load(rp, 0);
        forth_word(ip);
        rp += 4;
    }
    printf("\n");
}

void forth_show_stack(void)
{
    cell sp = arm_get_reg(SP);  // Skip DECAFBAD that's been pushed

    if (!mem_range_is_valid(sp, sp0 - sp)) return;

    cell top = arm_get_reg(TOP);
    if (top == 0xDECAFBAD) {
        printf("Stack: (empty)\n");
        return;
    }

    printf("Stack: %8.8x  ", top);
    while (sp < sp0 - 4) {
        cell n = mem_load(sp, 0);
        printf("%8.8x  ", n);
        sp += 4;
    }
    printf("\n");
}
