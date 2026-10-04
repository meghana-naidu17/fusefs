#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include "fs.h"

int block_read(
    FileSystem *fs,
    uint32_t block_no,
    void *buffer
);

int block_write(
    FileSystem *fs,
    uint32_t block_no,
    const void *buffer
);

int block_allocate(
    FileSystem *fs,
    uint32_t *block_no
);

int block_free(
    FileSystem *fs,
    uint32_t block_no
);

#endif
