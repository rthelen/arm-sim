#include "sim.h"

file_t *file_load(char *file_name)
{
    file_t *file;

    file = malloc(sizeof(file_t));
    if (!file) return NULL;

    file->fp = NULL;
    file->image = NULL;
    file->size = 0;
    file->name = file_name;

    file->fp = fopen(file->name, "r");
    if (!file->fp) {
        goto err;
    }

    if (fseek(file->fp, 0, SEEK_END)) {
        goto err;
    }

    file->size = ftell(file->fp);

    file->image = malloc(file->size);
    ASSERT(file->image);

    if (fseek(file->fp, 0, SEEK_SET)) {
        goto err;
    }

    if (!fread(file->image, 1, file->size, file->fp)) {
        goto err;
    }

    fclose(file->fp);
    file->fp = NULL;

    return file;

err:
    file_free(file);
    return NULL;
}

void file_put_in_memory(file_t *file, reg base)
{
    for (int i = 0; i < file->size; i++) {
        mem_storeb(base, i, file->image[i]);
    }
}

void file_free(file_t *file)
{
    if (file->fp) {
        fclose(file->fp);
        file->fp = NULL;
    }

    if (file->image) {
        free(file->image);
    }

    free(file);
}
