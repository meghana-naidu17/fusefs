#ifndef INODE_H
#define INODE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stddef.h>
#include <limits.h>
#include <stdint.h>

#define MAX_INODES 1024
#define MAX_NAME_LEN 255
#define MAX_DIRECT_BLOCKS 12

typedef enum
{
    INODE_FILE = 1,
    INODE_DIRECTORY = 2
} InodeType;

typedef struct Inode Inode;

/* Directory entry */
typedef struct
{
    char name[MAX_NAME_LEN + 1];
    ino_t inode_number;
} DirectoryEntry;

/* Inode structure */
struct Inode
{
    ino_t inode_number;

    InodeType type;

    mode_t mode;
    uid_t uid;
    gid_t gid;

    off_t size;

    struct timespec atime;
    struct timespec mtime;
    struct timespec ctime;

    nlink_t link_count;

    ino_t parent_inode;

    /* Direct data block references */
    uint32_t direct_blocks[MAX_DIRECT_BLOCKS];
    size_t block_count;

    /* Directory information */
    DirectoryEntry *entries;
    size_t entry_count;
    size_t entry_capacity;

    int used;
};

/* Inode management */
int inode_table_init(void);

void inode_table_destroy(void);

Inode *inode_create(
    InodeType type,
    mode_t mode,
    uid_t uid,
    gid_t gid,
    ino_t parent_inode
);

int inode_delete(ino_t inode_number);

/* Inode lookup */
Inode *inode_lookup(ino_t inode_number);

Inode *inode_lookup_path(const char *path);

/* Metadata */
int inode_update_size(
    ino_t inode_number,
    off_t size
);

int inode_update_times(
    ino_t inode_number,
    int update_access,
    int update_modify
);

/* Directory entry management */
int inode_add_entry(
    ino_t parent_inode,
    const char *name,
    ino_t child_inode
);

int inode_remove_entry(
    ino_t parent_inode,
    const char *name
);

Inode *inode_find_entry(
    ino_t parent_inode,
    const char *name
);

#endif
