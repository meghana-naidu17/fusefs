#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>
#include "fs.h"

int bitmap_set(
    FileSystem *fs,
    uint32_t block_no
);

int bitmap_clear(
    FileSystem *fs,
    uint32_t block_no
);

int bitmap_test(
    FileSystem *fs,
    uint32_t block_no
);

int bitmap_find_free(
    FileSystem *fs,
    uint32_t *block_no
);

#endif
