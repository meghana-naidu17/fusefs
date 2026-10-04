#define _DEFAULT_SOURCE

#include "bitmap.h"
#include "block.h"

#include <stdlib.h>

static int get_bitmap_position(
    FileSystem *fs,
    uint32_t block_no,
    uint32_t *bitmap_block,
    uint32_t *byte_index,
    uint8_t *mask)
{
    if (fs == NULL)
        return -1;

    if (block_no >= fs->sb.total_blocks)
        return -1;

    uint32_t bits_per_block =
        fs->sb.block_size * 8;

    uint32_t bitmap_index =
        block_no / bits_per_block;

    uint32_t bit_index =
        block_no % bits_per_block;

    *bitmap_block =
        fs->sb.bitmap_start + bitmap_index;

    *byte_index =
        bit_index / 8;

    *mask =
        (uint8_t)(1U << (bit_index % 8));

    return 0;
}

int bitmap_set(
    FileSystem *fs,
    uint32_t block_no)
{
    uint32_t bitmap_block;
    uint32_t byte_index;
    uint8_t mask;

    if (get_bitmap_position(
            fs,
            block_no,
            &bitmap_block,
            &byte_index,
            &mask) < 0)
    {
        return -1;
    }

    uint8_t *buffer =
        calloc(1, fs->sb.block_size);

    if (buffer == NULL)
        return -1;

    if (block_read(
            fs,
            bitmap_block,
            buffer) < 0)
    {
        free(buffer);
        return -1;
    }

    buffer[byte_index] |= mask;

    if (block_write(
            fs,
            bitmap_block,
            buffer) < 0)
    {
        free(buffer);
        return -1;
    }

    free(buffer);

    return 0;
}

int bitmap_clear(
    FileSystem *fs,
    uint32_t block_no)
{
    uint32_t bitmap_block;
    uint32_t byte_index;
    uint8_t mask;

    if (get_bitmap_position(
            fs,
            block_no,
            &bitmap_block,
            &byte_index,
            &mask) < 0)
    {
        return -1;
    }

    uint8_t *buffer =
        calloc(1, fs->sb.block_size);

    if (buffer == NULL)
        return -1;

    if (block_read(
            fs,
            bitmap_block,
            buffer) < 0)
    {
        free(buffer);
        return -1;
    }

    buffer[byte_index] &= (uint8_t)~mask;

    if (block_write(
            fs,
            bitmap_block,
            buffer) < 0)
    {
        free(buffer);
        return -1;
    }

    free(buffer);

    return 0;
}

int bitmap_test(
    FileSystem *fs,
    uint32_t block_no)
{
    uint32_t bitmap_block;
    uint32_t byte_index;
    uint8_t mask;

    if (get_bitmap_position(
            fs,
            block_no,
            &bitmap_block,
            &byte_index,
            &mask) < 0)
    {
        return -1;
    }

    uint8_t *buffer =
        calloc(1, fs->sb.block_size);

    if (buffer == NULL)
        return -1;

    if (block_read(
            fs,
            bitmap_block,
            buffer) < 0)
    {
        free(buffer);
        return -1;
    }

    int result =
        (buffer[byte_index] & mask) != 0;

    free(buffer);

    return result;
}

int bitmap_find_free(
    FileSystem *fs,
    uint32_t *block_no)
{
    if (fs == NULL || block_no == NULL)
        return -1;

    for (uint32_t i = fs->sb.data_start;
         i < fs->sb.total_blocks;
         i++)
    {
        int used =
            bitmap_test(fs, i);

        if (used == 0)
        {
            *block_no = i;
            return 0;
        }
    }

    return -1;
}
