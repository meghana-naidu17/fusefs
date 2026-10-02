#include "fs.h"
#include "block.h"
#include "bitmap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char *image = "fusefs.img";

    printf("================================\n");
    printf("FuseFS Storage Layer Test\n");
    printf("================================\n");

    printf("[1] Formatting filesystem image...\n");

    if (fs_format(
            image,
            DEFAULT_BLOCK_SIZE,
            DEFAULT_TOTAL_BLOCKS) < 0)
    {
        printf("FAIL: Filesystem format failed\n");
        return 1;
    }

    printf("PASS: Filesystem image formatted\n");

    FileSystem fs;

    printf("[2] Initializing filesystem...\n");

    if (fs_init(&fs, image) < 0)
    {
        printf("FAIL: Filesystem initialization failed\n");
        return 1;
    }

    printf("PASS: Filesystem initialized\n");

    printf("[3] Checking superblock...\n");

    printf("Magic        : 0x%X\n", fs.sb.magic);
    printf("Version      : %u\n", fs.sb.version);
    printf("Block size   : %u\n", fs.sb.block_size);
    printf("Total blocks : %u\n", fs.sb.total_blocks);
    printf("Bitmap start : %u\n", fs.sb.bitmap_start);
    printf("Bitmap blocks: %u\n", fs.sb.bitmap_blocks);
    printf("Data start   : %u\n", fs.sb.data_start);

    if (fs.sb.magic != FS_MAGIC)
    {
        printf("FAIL: Invalid filesystem magic\n");
        fs_shutdown(&fs);
        return 1;
    }

    printf("PASS: Superblock is valid\n");

    printf("[4] Allocating a block...\n");

    uint32_t block_no;

    if (block_allocate(&fs, &block_no) < 0)
    {
        printf("FAIL: Block allocation failed\n");
        fs_shutdown(&fs);
        return 1;
    }

    printf("PASS: Block allocated: %u\n", block_no);

    printf("[5] Checking bitmap...\n");

    int used = bitmap_test(&fs, block_no);

    if (used != 1)
    {
        printf("FAIL: Bitmap did not mark block as used\n");
        fs_shutdown(&fs);
        return 1;
    }

    printf("PASS: Bitmap marks block as used\n");

    printf("[6] Testing block write...\n");

    char *write_buffer = malloc(fs.sb.block_size);

    if (write_buffer == NULL)
    {
        printf("FAIL: Memory allocation failed\n");
        fs_shutdown(&fs);
        return 1;
    }

    memset(
        write_buffer,
        0,
        fs.sb.block_size
    );

    strcpy(
        write_buffer,
        "Hello from FuseFS storage layer!"
    );

    if (block_write(
            &fs,
            block_no,
            write_buffer) < 0)
    {
        printf("FAIL: Block write failed\n");
        free(write_buffer);
        fs_shutdown(&fs);
        return 1;
    }

    printf("PASS: Block write successful\n");

    printf("[7] Testing block read...\n");

    char *read_buffer = calloc(
        1,
        fs.sb.block_size
    );

    if (read_buffer == NULL)
    {
        printf("FAIL: Memory allocation failed\n");
        free(write_buffer);
        fs_shutdown(&fs);
        return 1;
    }

    if (block_read(
            &fs,
            block_no,
            read_buffer) < 0)
    {
        printf("FAIL: Block read failed\n");
        free(write_buffer);
        free(read_buffer);
        fs_shutdown(&fs);
        return 1;
    }

    if (strcmp(
            write_buffer,
            read_buffer) != 0)
    {
        printf("FAIL: Read data does not match written data\n");
        free(write_buffer);
        free(read_buffer);
        fs_shutdown(&fs);
        return 1;
    }

    printf("PASS: Block read successful\n");
    printf("Data read: %s\n", read_buffer);

    printf("[8] Freeing block...\n");

    if (block_free(&fs, block_no) < 0)
    {
        printf("FAIL: Block free failed\n");
        free(write_buffer);
        free(read_buffer);
        fs_shutdown(&fs);
        return 1;
    }

    printf("PASS: Block freed\n");

    printf("[9] Checking bitmap after free...\n");

    used = bitmap_test(&fs, block_no);

    if (used != 0)
    {
        printf("FAIL: Bitmap did not clear block\n");
        free(write_buffer);
        free(read_buffer);
        fs_shutdown(&fs);
        return 1;
    }

    printf("PASS: Bitmap marks block as free\n");

    free(write_buffer);
    free(read_buffer);

    printf("[10] Shutting down filesystem...\n");

    if (fs_shutdown(&fs) < 0)
    {
        printf("FAIL: Filesystem shutdown failed\n");
        return 1;
    }

    printf("PASS: Filesystem shutdown successful\n");

    printf("================================\n");
    printf("All storage tests passed!\n");
    printf("================================\n");

    return 0;
}
