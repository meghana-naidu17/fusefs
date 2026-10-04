#define _GNU_SOURCE

#include "file.h"
#include "block.h"
#include "fs.h"

extern FileSystem fusefs_fs;

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/* Split a path into parent path and final name */
static int split_path(
    const char *path,
    char *parent_path,
    size_t parent_size,
    char *name,
    size_t name_size
)
{
    if (path == NULL || path[0] != '/')
        return -EINVAL;

    if (strcmp(path, "/") == 0)
        return -EINVAL;

    char temp[4096];

    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *last_slash = strrchr(temp, '/');

    if (last_slash == NULL)
        return -EINVAL;

    if (last_slash == temp)
    {
        strncpy(parent_path, "/", parent_size - 1);
        parent_path[parent_size - 1] = '\0';
    }
    else
    {
        *last_slash = '\0';

        strncpy(parent_path, temp, parent_size - 1);
        parent_path[parent_size - 1] = '\0';
    }

    strncpy(name, last_slash + 1, name_size - 1);
    name[name_size - 1] = '\0';

    if (name[0] == '\0')
        return -EINVAL;

    return 0;
}

/* Look up a file or directory by path */
Inode *file_lookup(const char *path)
{
    return inode_lookup_path(path);
}

/* Create a regular file */
int file_create(
    const char *path,
    mode_t mode
)
{
    char parent_path[4096];
    char name[MAX_NAME_LEN + 1];

    int result = split_path(
        path,
        parent_path,
        sizeof(parent_path),
        name,
        sizeof(name)
    );

    if (result != 0)
        return result;

    Inode *parent = inode_lookup_path(parent_path);

    if (parent == NULL)
        return -ENOENT;

    if (parent->type != INODE_DIRECTORY)
        return -ENOTDIR;

    if (inode_find_entry(parent->inode_number, name) != NULL)
        return -EEXIST;

    Inode *inode = inode_create(
        INODE_FILE,
        mode,
        getuid(),
        getgid(),
        parent->inode_number
    );

    if (inode == NULL)
        return -ENOSPC;

    result = inode_add_entry(
        parent->inode_number,
        name,
        inode->inode_number
    );

    if (result != 0)
    {
        inode_delete(inode->inode_number);
        return result;
    }

    return 0;
}

/* Delete a regular file */
int file_delete(const char *path)
{
    char parent_path[4096];
    char name[MAX_NAME_LEN + 1];

    int result = split_path(
        path,
        parent_path,
        sizeof(parent_path),
        name,
        sizeof(name)
    );

    if (result != 0)
        return result;

    Inode *parent = inode_lookup_path(parent_path);

    if (parent == NULL)
        return -ENOENT;

    Inode *inode =
        inode_find_entry(parent->inode_number, name);

    if (inode == NULL)
        return -ENOENT;

    if (inode->type != INODE_FILE)
        return -EISDIR;

    /*
     * Release all storage blocks belonging to the file.
     */
    for (size_t i = 0; i < inode->block_count; i++)
    {
        if (block_free(
                &fusefs_fs,
                inode->direct_blocks[i]) < 0)
        {
            return -EIO;
        }
    }

    result = inode_remove_entry(
        parent->inode_number,
        name
    );

    if (result != 0)
        return result;

    return inode_delete(inode->inode_number);
}

/* Read file data */

ssize_t file_read(
    const char *path,
    void *buffer,
    size_t size,
    off_t offset
)
{
    if (buffer == NULL)
        return -EINVAL;

    if (offset < 0)
        return -EINVAL;

    Inode *inode = inode_lookup_path(path);

    if (inode == NULL)
        return -ENOENT;

    if (inode->type != INODE_FILE)
        return -EISDIR;

    if (offset >= inode->size || size == 0)
        return 0;

    size_t available =
        (size_t)(inode->size - offset);

    if (size > available)
        size = available;

    size_t block_size = fusefs_fs.sb.block_size;
    size_t bytes_read = 0;

    while (bytes_read < size)
    {
        off_t current_offset =
            offset + (off_t)bytes_read;

        size_t block_index =
            (size_t)(current_offset / (off_t)block_size);

        size_t block_offset =
            (size_t)(current_offset % (off_t)block_size);

        if (block_index >= MAX_DIRECT_BLOCKS)
            return -EFBIG;

        size_t remaining = size - bytes_read;
        size_t bytes_in_block =
            block_size - block_offset;

        if (remaining > bytes_in_block)
            remaining = bytes_in_block;

        if (block_index >= inode->block_count)
        {
            memset(
                (char *)buffer + bytes_read,
                0,
                remaining
            );
        }
        else
        {
            uint8_t *block_buffer =
                malloc(block_size);

            if (block_buffer == NULL)
                return -ENOMEM;

            if (block_read(
                    &fusefs_fs,
                    inode->direct_blocks[block_index],
                    block_buffer) < 0)
            {
                free(block_buffer);
                return -EIO;
            }

            memcpy(
                (char *)buffer + bytes_read,
                block_buffer + block_offset,
                remaining
            );

            free(block_buffer);
        }

        bytes_read += remaining;
    }

    inode_update_times(
        inode->inode_number,
        1,
        0
    );

    return (ssize_t)bytes_read;
}

/* Write file data */
ssize_t file_write(
    const char *path,
    const void *buffer,
    size_t size,
    off_t offset
)
{
    if (buffer == NULL)
        return -EINVAL;

    if (offset < 0)
        return -EINVAL;

    Inode *inode = inode_lookup_path(path);

    if (inode == NULL)
        return -ENOENT;

    if (inode->type != INODE_FILE)
        return -EISDIR;

    if (size == 0)
        return 0;

    size_t block_size = fusefs_fs.sb.block_size;

    if (size > (size_t)MAX_DIRECT_BLOCKS * block_size)
        return -EFBIG;

    if ((uint64_t)offset + (uint64_t)size >
        (uint64_t)MAX_DIRECT_BLOCKS * block_size)
        return -EFBIG;

    size_t bytes_written = 0;

    while (bytes_written < size)
    {
        off_t current_offset =
            offset + (off_t)bytes_written;

        size_t block_index =
            (size_t)(current_offset / (off_t)block_size);

        size_t block_offset =
            (size_t)(current_offset % (off_t)block_size);

        if (block_index >= MAX_DIRECT_BLOCKS)
            return -EFBIG;

        size_t remaining = size - bytes_written;
        size_t bytes_in_block =
            block_size - block_offset;

        if (remaining > bytes_in_block)
            remaining = bytes_in_block;

        /*
         * Allocate a new storage block when this
         * file block does not exist yet.
         */
        if (block_index >= inode->block_count)
        {
            uint32_t new_block;

            if (block_allocate(
                    &fusefs_fs,
                    &new_block) < 0)
            {
                return bytes_written > 0
                    ? (ssize_t)bytes_written
                    : -ENOSPC;
            }

            inode->direct_blocks[block_index] = new_block;
            inode->block_count++;
        }

        uint8_t *block_buffer =
            malloc(block_size);

        if (block_buffer == NULL)
            return bytes_written > 0
                ? (ssize_t)bytes_written
                : -ENOMEM;

        /*
         * Read the existing block first so that a
         * partial write does not destroy other data.
         */
        if (block_read(
                &fusefs_fs,
                inode->direct_blocks[block_index],
                block_buffer) < 0)
        {
            free(block_buffer);

            return bytes_written > 0
                ? (ssize_t)bytes_written
                : -EIO;
        }

        memcpy(
            block_buffer + block_offset,
            (const char *)buffer + bytes_written,
            remaining
        );

        if (block_write(
                &fusefs_fs,
                inode->direct_blocks[block_index],
                block_buffer) < 0)
        {
            free(block_buffer);

            return bytes_written > 0
                ? (ssize_t)bytes_written
                : -EIO;
        }

        free(block_buffer);

        bytes_written += remaining;
    }

    off_t new_size =
        offset + (off_t)bytes_written;

    if (new_size > inode->size)
    {
        int result =
            inode_update_size(
                inode->inode_number,
                new_size
            );

        if (result != 0)
            return result;
    }

    inode_update_times(
        inode->inode_number,
        0,
        1
    );

    return (ssize_t)bytes_written;
}

/* Create a directory */
int directory_create(
    const char *path,
    mode_t mode
)
{
    char parent_path[4096];
    char name[MAX_NAME_LEN + 1];

    int result = split_path(
        path,
        parent_path,
        sizeof(parent_path),
        name,
        sizeof(name)
    );

    if (result != 0)
        return result;

    Inode *parent = inode_lookup_path(parent_path);

    if (parent == NULL)
        return -ENOENT;

    if (parent->type != INODE_DIRECTORY)
        return -ENOTDIR;

    if (inode_find_entry(parent->inode_number, name) != NULL)
        return -EEXIST;

    Inode *inode = inode_create(
        INODE_DIRECTORY,
        mode,
        getuid(),
        getgid(),
        parent->inode_number
    );

    if (inode == NULL)
        return -ENOSPC;

    result = inode_add_entry(
        parent->inode_number,
        name,
        inode->inode_number
    );

    if (result != 0)
    {
        inode_delete(inode->inode_number);
        return result;
    }

    parent->link_count++;

    return 0;
}

/* Delete an empty directory */
int directory_delete(const char *path)
{
    if (path == NULL || strcmp(path, "/") == 0)
        return -EBUSY;

    char parent_path[4096];
    char name[MAX_NAME_LEN + 1];

    int result = split_path(
        path,
        parent_path,
        sizeof(parent_path),
        name,
        sizeof(name)
    );

    if (result != 0)
        return result;

    Inode *parent = inode_lookup_path(parent_path);

    if (parent == NULL)
        return -ENOENT;

    Inode *inode =
        inode_find_entry(parent->inode_number, name);

    if (inode == NULL)
        return -ENOENT;

    if (inode->type != INODE_DIRECTORY)
        return -ENOTDIR;

    if (inode->entry_count != 0)
        return -ENOTEMPTY;

    result = inode_remove_entry(
        parent->inode_number,
        name
    );

    if (result != 0)
        return result;

    if (parent->link_count > 0)
        parent->link_count--;

    return inode_delete(inode->inode_number);
}

/* Add an entry to a directory */
int directory_add_entry(
    const char *directory_path,
    const char *name,
    ino_t inode_number
)
{
    if (directory_path == NULL || name == NULL)
        return -EINVAL;

    Inode *directory =
        inode_lookup_path(directory_path);

    if (directory == NULL)
        return -ENOENT;

    if (directory->type != INODE_DIRECTORY)
        return -ENOTDIR;

    return inode_add_entry(
        directory->inode_number,
        name,
        inode_number
    );
}

/* Remove an entry from a directory */
int directory_remove_entry(
    const char *directory_path,
    const char *name
)
{
    if (directory_path == NULL || name == NULL)
        return -EINVAL;

    Inode *directory =
        inode_lookup_path(directory_path);

    if (directory == NULL)
        return -ENOENT;

    if (directory->type != INODE_DIRECTORY)
        return -ENOTDIR;

    return inode_remove_entry(
        directory->inode_number,
        name
    );
}

/* Return metadata for a file or directory */
int file_getattr(
    const char *path,
    struct stat *stbuf
)
{
    if (path == NULL || stbuf == NULL)
        return -EINVAL;

    Inode *inode = inode_lookup_path(path);

    if (inode == NULL)
        return -ENOENT;

    memset(stbuf, 0, sizeof(struct stat));

    stbuf->st_ino = inode->inode_number;
    stbuf->st_mode = inode->mode;
    stbuf->st_nlink = inode->link_count;
    stbuf->st_uid = inode->uid;
    stbuf->st_gid = inode->gid;
    stbuf->st_size = inode->size;
    stbuf->st_blksize = 4096;
    stbuf->st_blocks =
        (inode->size + 511) / 512;

    stbuf->st_atim = inode->atime;
    stbuf->st_mtim = inode->mtime;
    stbuf->st_ctim = inode->ctime;

    return 0;
}

/* Change file size */
int file_truncate(
    const char *path,
    off_t size
)
{
    if (size < 0)
        return -EINVAL;

    Inode *inode = inode_lookup_path(path);

    if (inode == NULL)
        return -ENOENT;

    if (inode->type != INODE_FILE)
        return -EISDIR;

    size_t block_size = fusefs_fs.sb.block_size;

    if ((uint64_t)size >
        (uint64_t)MAX_DIRECT_BLOCKS * block_size)
        return -EFBIG;

    /*
     * Shrinking the file:
     * free blocks that are completely beyond the new size.
     */
    if (size < inode->size)
    {
        size_t required_blocks = 0;

        if (size > 0)
        {
            required_blocks =
                ((size_t)size + block_size - 1) /
                block_size;
        }

        while (inode->block_count > required_blocks)
        {
            size_t index = inode->block_count - 1;

            if (block_free(
                    &fusefs_fs,
                    inode->direct_blocks[index]) < 0)
            {
                return -EIO;
            }

            inode->block_count--;
        }

        /*
         * If the new size ends in the middle of a block,
         * clear the bytes after EOF so old data is not
         * exposed if the file grows again later.
         */
        if (size > 0 &&
            (size % block_size) != 0 &&
            inode->block_count > 0)
        {
            size_t last_index =
                inode->block_count - 1;

            uint8_t *block_buffer =
                malloc(block_size);

            if (block_buffer == NULL)
                return -ENOMEM;

            if (block_read(
                    &fusefs_fs,
                    inode->direct_blocks[last_index],
                    block_buffer) < 0)
            {
                free(block_buffer);
                return -EIO;
            }

            size_t valid_bytes =
                (size_t)size % block_size;

            memset(
                block_buffer + valid_bytes,
                0,
                block_size - valid_bytes
            );

            if (block_write(
                    &fusefs_fs,
                    inode->direct_blocks[last_index],
                    block_buffer) < 0)
            {
                free(block_buffer);
                return -EIO;
            }

            free(block_buffer);
        }
    }

    int result =
        inode_update_size(
            inode->inode_number,
            size
        );

    if (result != 0)
        return result;

    inode_update_times(
        inode->inode_number,
        0,
        1
    );

    return 0;
}

