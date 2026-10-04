#define _DEFAULT_SOURCE

#include "block.h"
#include "bitmap.h"

#include <stdlib.h>
#include <unistd.h>

int block_read(
    FileSystem *fs,
    uint32_t block_no,
    void *buffer)
{
    if (fs == NULL || buffer == NULL)
        return -1;

    if (block_no >= fs->sb.total_blocks)
        return -1;

    off_t offset =
        (off_t)block_no * fs->sb.block_size;

    ssize_t result = pread(
        fs->fd,
        buffer,
        fs->sb.block_size,
        offset
    );

    if (result != (ssize_t)fs->sb.block_size)
        return -1;

    return 0;
}

int block_write(
    FileSystem *fs,
    uint32_t block_no,
    const void *buffer)
{
    if (fs == NULL || buffer == NULL)
        return -1;

    if (block_no >= fs->sb.total_blocks)
        return -1;

    off_t offset =
        (off_t)block_no * fs->sb.block_size;

    ssize_t result = pwrite(
        fs->fd,
        buffer,
        fs->sb.block_size,
        offset
    );

    if (result != (ssize_t)fs->sb.block_size)
        return -1;

    return 0;
}

int block_allocate(
    FileSystem *fs,
    uint32_t *block_no)
{
    if (fs == NULL || block_no == NULL)
        return -1;

    if (bitmap_find_free(fs, block_no) < 0)
        return -1;

    if (bitmap_set(fs, *block_no) < 0)
        return -1;

    void *buffer =
        calloc(1, fs->sb.block_size);

    if (buffer == NULL)
    {
        bitmap_clear(fs, *block_no);
        return -1;
    }

    if (block_write(
            fs,
            *block_no,
            buffer) < 0)
    {
        bitmap_clear(fs, *block_no);
        free(buffer);
        return -1;
    }

    free(buffer);

    return 0;
}

int block_free(
    FileSystem *fs,
    uint32_t block_no)
{
    if (fs == NULL)
        return -1;

    if (block_no < fs->sb.data_start)
        return -1;

    if (block_no >= fs->sb.total_blocks)
        return -1;

    return bitmap_clear(
        fs,
        block_no
    );
}
