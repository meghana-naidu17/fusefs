#define _DEFAULT_SOURCE

#include "fusefs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int fusefs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    (void)fi;

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    return -ENOENT;
}

int fusefs_readdir(
    const char *path,
    void *buffer,
    fuse_fill_dir_t filler,
    off_t offset,
    struct fuse_file_info *fi,
    enum fuse_readdir_flags flags)
{
    (void)offset;
    (void)fi;
    (void)flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buffer, ".", NULL, 0, 0);
    filler(buffer, "..", NULL, 0, 0);

    return 0;
}

int fusefs_open(
    const char *path,
    struct fuse_file_info *fi)
{
    (void)path;
    (void)fi;

    return -ENOENT;
}

int fusefs_read(
    const char *path,
    char *buffer,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi)
{
    (void)path;
    (void)buffer;
    (void)size;
    (void)offset;
    (void)fi;

    return -ENOENT;
}

int fusefs_write(
    const char *path,
    const char *buffer,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi)
{
    (void)path;
    (void)buffer;
    (void)size;
    (void)offset;
    (void)fi;

    return -EROFS;
}

int fusefs_create(
    const char *path,
    mode_t mode,
    struct fuse_file_info *fi)
{
    (void)path;
    (void)mode;
    (void)fi;

    return -EROFS;
}

int fusefs_unlink(
    const char *path)
{
    (void)path;

    return -EROFS;
}

int fusefs_truncate(
    const char *path,
    off_t size,
    struct fuse_file_info *fi)
{
    (void)path;
    (void)size;
    (void)fi;

    return -EROFS;
}

int fusefs_mkdir(
    const char *path,
    mode_t mode)
{
    (void)path;
    (void)mode;

    return -EROFS;
}

int fusefs_rmdir(
    const char *path)
{
    (void)path;

    return -EROFS;
}

int fusefs_init_filesystem(void)
{
    return 0;
}

void fusefs_destroy_filesystem(void)
{
}
