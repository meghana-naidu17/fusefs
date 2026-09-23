#ifndef FUSEFS_H
#define FUSEFS_H

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>

#define FUSEFS_ROOT_INODE 0

int fusefs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);

int fusefs_readdir(
    const char *path,
    void *buffer,
    fuse_fill_dir_t filler,
    off_t offset,
    struct fuse_file_info *fi,
    enum fuse_readdir_flags flags
);

int fusefs_open(
    const char *path,
    struct fuse_file_info *fi
);

int fusefs_read(
    const char *path,
    char *buffer,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi
);

int fusefs_write(
    const char *path,
    const char *buffer,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi
);

int fusefs_create(
    const char *path,
    mode_t mode,
    struct fuse_file_info *fi
);

int fusefs_unlink(
    const char *path
);

int fusefs_truncate(
    const char *path,
    off_t size,
    struct fuse_file_info *fi
);

int fusefs_mkdir(
    const char *path,
    mode_t mode
);

int fusefs_rmdir(
    const char *path
);

int fusefs_init_filesystem(void);

void fusefs_destroy_filesystem(void);

#endif
