#define _GNU_SOURCE

#include "file.h"

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

    /*
     * Storage integration will use the block layer.
     * For now, return EOF for files without allocated
     * data blocks.
     */
    if (offset >= inode->size)
        return 0;

    size_t available =
        (size_t)(inode->size - offset);

    if (size > available)
        size = available;

    memset(buffer, 0, size);

    inode_update_times(
        inode->inode_number,
        1,
        0
    );

    return (ssize_t)size;
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

    /*
     * The actual block allocation and data writing
     * will be connected to Member 2's storage layer.
     */

    off_t new_size = offset + (off_t)size;

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

    return (ssize_t)size;
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

    return inode_update_size(
        inode->inode_number,
        size
    );
}
