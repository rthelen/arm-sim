#include "sim.h"

byte *image;
int image_size;

int image_load(char *file_name)
{
    FILE *f;

    image = NULL;
    image_size = 0;

    f = fopen(file_name, "r");
    if (!f) {
        return -1;
    }

    if (fseek(f, 0, SEEK_END)) {
        goto err;
    }

    image_size = ftell(f);

    image = malloc(image_size);
    ASSERT(image);

    if (fseek(f, 0, SEEK_SET)) {
        goto err;
    }

    if (!fread(image, 1, image_size, f)) {
        goto err;
    }

    fclose(f);
    return 0;

err:
    fclose(f);
    if (image) {
        free(image);
    }
    image = NULL;
    image_size = 0;
    return -1;
}

