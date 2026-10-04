#ifndef FILE_H
#define FILE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>

#include "inode.h"

/* File operations */

int file_create(
    const char *path,
    mode_t mode
);

int file_delete(
    const char *path
);

ssize_t file_read(
    const char *path,
    void *buffer,
    size_t size,
    off_t offset
);

ssize_t file_write(
    const char *path,
    const void *buffer,
    size_t size,
    off_t offset
);

/* Directory operations */

int directory_create(
    const char *path,
    mode_t mode
);

int directory_delete(
    const char *path
);

int directory_add_entry(
    const char *directory_path,
    const char *name,
    ino_t inode_number
);

int directory_remove_entry(
    const char *directory_path,
    const char *name
);

/* Path and lookup operations */

Inode *file_lookup(
    const char *path
);

int file_getattr(
    const char *path,
    struct stat *stbuf
);

int file_truncate(
    const char *path,
    off_t size
);

#endif
