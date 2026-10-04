#include "fs.h"
#include "file.h"
#include "inode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FileSystem fusefs_fs;

int main(void)
{
    const char *image = "test_inode.img";

    printf("========================================\n");
    printf("FuseFS Inode/File/Directory Test\n");
    printf("========================================\n");

    printf("[1] Formatting filesystem image...\n");

    if (fs_format(image, DEFAULT_BLOCK_SIZE, DEFAULT_TOTAL_BLOCKS) < 0)
    {
        printf("FAIL: Filesystem format failed\n");
        return 1;
    }

    printf("PASS: Filesystem formatted\n");

    printf("[2] Initializing filesystem...\n");

    if (fs_init(&fusefs_fs, image) < 0)
    {
        printf("FAIL: Filesystem initialization failed\n");
        return 1;
    }

    if (inode_table_init() < 0)
    {
        printf("FAIL: Inode table initialization failed\n");
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    printf("PASS: Filesystem and inode table initialized\n");

    printf("[3] Creating directory /documents...\n");

    if (directory_create("/documents", 0755) < 0)
    {
        printf("FAIL: Directory creation failed\n");
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    printf("PASS: Directory created\n");

    printf("[4] Creating file /documents/report.txt...\n");

    if (file_create("/documents/report.txt", 0644) < 0)
    {
        printf("FAIL: File creation failed\n");
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    printf("PASS: File created\n");

    printf("[5] Checking path lookup...\n");

    Inode *inode = file_lookup("/documents/report.txt");

    if (inode == NULL || inode->type != INODE_FILE)
    {
        printf("FAIL: File path lookup failed\n");
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    printf("PASS: File path lookup successful\n");

    printf("[6] Writing file data...\n");

    const char *message = "Hello from FuseFS Member 3!";

    ssize_t written = file_write(
        "/documents/report.txt",
        message,
        strlen(message),
        0
    );

    if (written != (ssize_t)strlen(message))
    {
        printf("FAIL: File write failed\n");
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    printf("PASS: File write successful\n");

    printf("[7] Reading file data...\n");

    char buffer[128];
    memset(buffer, 0, sizeof(buffer));

    ssize_t bytes_read = file_read(
        "/documents/report.txt",
        buffer,
        strlen(message),
        0
    );

    if (bytes_read != (ssize_t)strlen(message))
    {
        printf("FAIL: File read failed\n");
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    if (strcmp(buffer, message) != 0)
    {
        printf("FAIL: Read data does not match written data\n");
        printf("Expected: %s\n", message);
        printf("Actual  : %s\n", buffer);
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    printf("PASS: File read successful\n");
    printf("Data read: %s\n", buffer);

    printf("[8] Checking file metadata...\n");

    struct stat st;

    if (file_getattr("/documents/report.txt", &st) < 0)
    {
        printf("FAIL: File metadata lookup failed\n");
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    if (st.st_size != (off_t)strlen(message))
    {
        printf("FAIL: File size is incorrect\n");
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    printf("PASS: File metadata is correct\n");

    printf("[9] Testing multi-block file...\n");

    size_t large_size = 9000;
    char *large_data = malloc(large_size);

    if (large_data == NULL)
    {
        printf("FAIL: Memory allocation failed\n");
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    for (size_t i = 0; i < large_size; i++)
        large_data[i] = (char)('A' + (i % 26));

    if (file_write(
            "/documents/report.txt",
            large_data,
            large_size,
            0) != (ssize_t)large_size)
    {
        printf("FAIL: Multi-block write failed\n");
        free(large_data);
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    char *large_read = calloc(1, large_size);

    if (large_read == NULL)
    {
        printf("FAIL: Memory allocation failed\n");
        free(large_data);
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    if (file_read(
            "/documents/report.txt",
            large_read,
            large_size,
            0) != (ssize_t)large_size)
    {
        printf("FAIL: Multi-block read failed\n");
        free(large_data);
        free(large_read);
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    if (memcmp(large_data, large_read, large_size) != 0)
    {
        printf("FAIL: Multi-block data mismatch\n");
        free(large_data);
        free(large_read);
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    printf("PASS: Multi-block read/write successful\n");

    free(large_data);
    free(large_read);

    printf("[10] Deleting file...\n");

    if (file_delete("/documents/report.txt") < 0)
    {
        printf("FAIL: File deletion failed\n");
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    if (file_lookup("/documents/report.txt") != NULL)
    {
        printf("FAIL: Deleted file still exists\n");
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    printf("PASS: File deleted\n");

    printf("[11] Deleting directory...\n");

    if (directory_delete("/documents") < 0)
    {
        printf("FAIL: Directory deletion failed\n");
        inode_table_destroy();
        fs_shutdown(&fusefs_fs);
        return 1;
    }

    printf("PASS: Directory deleted\n");

    inode_table_destroy();

    if (fs_shutdown(&fusefs_fs) < 0)
    {
        printf("FAIL: Filesystem shutdown failed\n");
        return 1;
    }

    remove(image);

    printf("========================================\n");
    printf("All Member 3 tests passed!\n");
    printf("========================================\n");

    return 0;
}
