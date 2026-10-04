#define _DEFAULT_SOURCE

#include "fusefs.h"
#include "inode.h"
#include "file.h"
#include "fs.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

FileSystem fusefs_fs;

int fusefs_getattr(
    const char *path,
    struct stat *stbuf,
    struct fuse_file_info *fi)
{
    (void)fi;

    return file_getattr(path, stbuf);
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

    Inode *directory = inode_lookup_path(path);

    if (directory == NULL)
        return -ENOENT;

    if (directory->type != INODE_DIRECTORY)
        return -ENOTDIR;

    filler(buffer, ".", NULL, 0, 0);
    filler(buffer, "..", NULL, 0, 0);

    for (size_t i = 0; i < directory->entry_count; i++)
    {
        DirectoryEntry *entry =
            &directory->entries[i];

        Inode *child =
            inode_lookup(entry->inode_number);

        if (child == NULL)
            continue;

        struct stat st;
        memset(&st, 0, sizeof(st));

        st.st_ino = child->inode_number;
        st.st_mode = child->mode;
        st.st_nlink = child->link_count;
        st.st_uid = child->uid;
        st.st_gid = child->gid;
        st.st_size = child->size;
        st.st_atim = child->atime;
        st.st_mtim = child->mtime;
        st.st_ctim = child->ctime;

        filler(
            buffer,
            entry->name,
            &st,
            0,
            0
        );
    }

    return 0;
}

int fusefs_open(
    const char *path,
    struct fuse_file_info *fi)
{
    (void)fi;

    Inode *inode = file_lookup(path);

    if (inode == NULL)
        return -ENOENT;

    if (inode->type != INODE_FILE)
        return -EISDIR;

    return 0;
}

int fusefs_read(
    const char *path,
    char *buffer,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi)
{
    (void)fi;

    return (int)file_read(
        path,
        buffer,
        size,
        offset
    );
}

int fusefs_write(
    const char *path,
    const char *buffer,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi)
{
    (void)fi;

    return (int)file_write(
        path,
        buffer,
        size,
        offset
    );
}

int fusefs_create(
    const char *path,
    mode_t mode,
    struct fuse_file_info *fi)
{
    (void)fi;

    return file_create(path, mode);
}

int fusefs_unlink(
    const char *path)
{
    return file_delete(path);
}

int fusefs_truncate(
    const char *path,
    off_t size,
    struct fuse_file_info *fi)
{
    (void)fi;

    return file_truncate(path, size);
}

int fusefs_mkdir(
    const char *path,
    mode_t mode)
{
    return directory_create(path, mode);
}

int fusefs_rmdir(
    const char *path)
{
    return directory_delete(path);
}

int fusefs_init_filesystem(void)
{
    if (fs_init(&fusefs_fs, "fusefs.img") != 0)
        return -1;

    if (inode_table_init() != 0)
    {
        fs_shutdown(&fusefs_fs);
        return -1;
    }

    return 0;
}

void fusefs_destroy_filesystem(void)
{
    inode_table_destroy();
    fs_shutdown(&fusefs_fs);
}
