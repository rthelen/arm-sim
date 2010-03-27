/*
 * This file is part of arm-sim: http://madscientistroom.org/arm-sim
 *
 * Copyright (c) 2010 Randy Thelen. All rights reserved, and all wrongs
 * reversed. (See the file COPYRIGHT for details.)
 */

#include "sim.h"

char *forth_path;

file_t *file_load(char *file_name)
{
    char *fname;
    file_t *file;

    if (file_name[0] == '/') {
        asprintf(&fname, "%s", file_name);
    } else if (forth_path[strlen(forth_path) -1] == '/') {
        asprintf(&fname, "%s%s", forth_path, file_name);
    } else {
        asprintf(&fname, "%s/%s", forth_path, file_name);
    }

    file = malloc(sizeof(file_t));
    if (!file) return NULL;

    file->name = fname;
    file->fp = NULL;
    file->image = NULL;
    file->image_size = 0;
    file->base = 0;
    file->size = 0;

    file->fp = fopen(file->name, "r");
    if (!file->fp) {
        goto err;
    }

    if (fseek(file->fp, 0, SEEK_END)) {
        goto err;
    }

    file->image_size = ftell(file->fp);

    file->image = malloc(file->image_size);
    ASSERT(file->image);

    if (fseek(file->fp, 0, SEEK_SET)) {
        goto err;
    }

    if (!fread(file->image, 1, file->image_size, file->fp)) {
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
    file->base = base;

    for (int i = 0; i < file->image_size; i++) {
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
