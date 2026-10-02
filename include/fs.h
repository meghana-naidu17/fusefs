#ifndef FS_H
#define FS_H

#include <stdint.h>

#define FS_MAGIC 0x46555345
#define FS_VERSION 1

#define DEFAULT_BLOCK_SIZE 4096
#define DEFAULT_TOTAL_BLOCKS 1024

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t bitmap_start;
    uint32_t bitmap_blocks;
    uint32_t data_start;
} SuperBlock;

typedef struct
{
    int fd;
    SuperBlock sb;
} FileSystem;

int fs_format(
    const char *image_path,
    uint32_t block_size,
    uint32_t total_blocks
);

int fs_init(
    FileSystem *fs,
    const char *image_path
);

int fs_load_superblock(
    FileSystem *fs
);

int fs_save_superblock(
    FileSystem *fs
);

int fs_shutdown(
    FileSystem *fs
);

#endif
