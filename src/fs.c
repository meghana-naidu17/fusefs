#define _DEFAULT_SOURCE

#include "fs.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint32_t calculate_bitmap_blocks(
    uint32_t block_size,
    uint32_t total_blocks)
{
    uint32_t bits_per_block = block_size * 8;

    return (total_blocks + bits_per_block - 1)
           / bits_per_block;
}

int fs_format(
    const char *image_path,
    uint32_t block_size,
    uint32_t total_blocks)
{
    if (image_path == NULL ||
        block_size == 0 ||
        total_blocks == 0)
    {
        return -1;
    }

    int fd = open(
        image_path,
        O_RDWR | O_CREAT | O_TRUNC,
        0666
    );

    if (fd < 0)
    {
        perror("open");
        return -1;
    }

    uint32_t bitmap_blocks =
        calculate_bitmap_blocks(
            block_size,
            total_blocks
        );

    uint32_t data_start =
        1 + bitmap_blocks;

    if (data_start >= total_blocks)
    {
        close(fd);
        return -1;
    }

    off_t image_size =
        (off_t)block_size * total_blocks;

    if (ftruncate(fd, image_size) < 0)
    {
        perror("ftruncate");
        close(fd);
        return -1;
    }

    SuperBlock sb;

    memset(&sb, 0, sizeof(SuperBlock));

    sb.magic = FS_MAGIC;
    sb.version = FS_VERSION;
    sb.block_size = block_size;
    sb.total_blocks = total_blocks;
    sb.bitmap_start = 1;
    sb.bitmap_blocks = bitmap_blocks;
    sb.data_start = data_start;

    void *buffer = calloc(1, block_size);

    if (buffer == NULL)
    {
        close(fd);
        return -1;
    }

    memcpy(
        buffer,
        &sb,
        sizeof(SuperBlock)
    );

    ssize_t written = pwrite(
        fd,
        buffer,
        block_size,
        0
    );

    if (written != (ssize_t)block_size)
    {
        free(buffer);
        close(fd);
        return -1;
    }

    memset(buffer, 0, block_size);

    for (uint32_t i = 0;
         i < bitmap_blocks;
         i++)
    {
        off_t offset =
            (off_t)(sb.bitmap_start + i)
            * block_size;

        written = pwrite(
            fd,
            buffer,
            block_size,
            offset
        );

        if (written != (ssize_t)block_size)
        {
            free(buffer);
            close(fd);
            return -1;
        }
    }

    free(buffer);

    fsync(fd);
    close(fd);

    return 0;
}

int fs_init(
    FileSystem *fs,
    const char *image_path)
{
    if (fs == NULL ||
        image_path == NULL)
    {
        return -1;
    }

    int fd = open(
        image_path,
        O_RDWR
    );

    if (fd < 0)
    {
        if (fs_format(
                image_path,
                DEFAULT_BLOCK_SIZE,
                DEFAULT_TOTAL_BLOCKS
            ) < 0)
        {
            return -1;
        }

        fd = open(
            image_path,
            O_RDWR
        );

        if (fd < 0)
        {
            return -1;
        }
    }

    fs->fd = fd;

    if (fs_load_superblock(fs) < 0)
    {
        close(fd);
        return -1;
    }

    if (fs->sb.magic != FS_MAGIC)
    {
        close(fd);
        return -1;
    }

    if (fs->sb.version != FS_VERSION)
    {
        close(fd);
        return -1;
    }

    return 0;
}

int fs_load_superblock(
    FileSystem *fs)
{
    if (fs == NULL)
    {
        return -1;
    }

    uint32_t block_size =
        DEFAULT_BLOCK_SIZE;

    void *buffer =
        calloc(1, block_size);

    if (buffer == NULL)
    {
        return -1;
    }

    ssize_t result = pread(
        fs->fd,
        buffer,
        block_size,
        0
    );

    if (result != (ssize_t)block_size)
    {
        free(buffer);
        return -1;
    }

    memcpy(
        &fs->sb,
        buffer,
        sizeof(SuperBlock)
    );

    free(buffer);

    return 0;
}

int fs_save_superblock(
    FileSystem *fs)
{
    if (fs == NULL)
    {
        return -1;
    }

    void *buffer =
        calloc(1, fs->sb.block_size);

    if (buffer == NULL)
    {
        return -1;
    }

    memcpy(
        buffer,
        &fs->sb,
        sizeof(SuperBlock)
    );

    ssize_t result = pwrite(
        fs->fd,
        buffer,
        fs->sb.block_size,
        0
    );

    free(buffer);

    if (result !=
        (ssize_t)fs->sb.block_size)
    {
        return -1;
    }

    fsync(fs->fd);

    return 0;
}

int fs_shutdown(
    FileSystem *fs)
{
    if (fs == NULL)
    {
        return -1;
    }

    fsync(fs->fd);

    close(fs->fd);

    fs->fd = -1;

    return 0;
}
