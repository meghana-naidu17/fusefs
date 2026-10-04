#define _GNU_SOURCE
#include "inode.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static Inode inode_table[MAX_INODES];
static ino_t next_inode_number = 1;

/* Get current time */
static struct timespec current_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts;
}

/* Initialize inode table */
int inode_table_init(void)
{
    memset(inode_table, 0, sizeof(inode_table));
    next_inode_number = 1;

    /* Create root directory */
    Inode *root = &inode_table[0];

    root->inode_number = 0;
    root->type = INODE_DIRECTORY;
    root->mode = S_IFDIR | 0755;
    root->uid = 0;
    root->gid = 0;
    root->size = 0;
    root->atime = current_time();
    root->mtime = root->atime;
    root->ctime = root->atime;
    root->link_count = 2;
    root->parent_inode = 0;
    root->entries = NULL;
    root->entry_count = 0;
    root->entry_capacity = 0;
    root->block_count = 0;
    root->used = 1;

    return 0;
}

/* Destroy inode table */
void inode_table_destroy(void)
{
    for (size_t i = 0; i < MAX_INODES; i++)
    {
        free(inode_table[i].entries);
        inode_table[i].entries = NULL;
        inode_table[i].entry_count = 0;
        inode_table[i].entry_capacity = 0;
    }

    memset(inode_table, 0, sizeof(inode_table));
}

/* Find a free inode slot */
static Inode *find_free_inode(void)
{
    for (size_t i = 1; i < MAX_INODES; i++)
    {
        if (!inode_table[i].used)
            return &inode_table[i];
    }

    return NULL;
}

/* Create a new inode */
Inode *inode_create(
    InodeType type,
    mode_t mode,
    uid_t uid,
    gid_t gid,
    ino_t parent_inode
)
{
    Inode *inode = find_free_inode();

    if (inode == NULL)
        return NULL;

    memset(inode, 0, sizeof(Inode));

    inode->inode_number = next_inode_number++;
    inode->type = type;

    if (type == INODE_FILE)
        inode->mode = S_IFREG | (mode & 07777);
    else
        inode->mode = S_IFDIR | (mode & 07777);

    inode->uid = uid;
    inode->gid = gid;
    inode->size = 0;

    inode->atime = current_time();
    inode->mtime = inode->atime;
    inode->ctime = inode->atime;

    inode->link_count = (type == INODE_DIRECTORY) ? 2 : 1;
    inode->parent_inode = parent_inode;

    inode->entries = NULL;
    inode->entry_count = 0;
    inode->entry_capacity = 0;

    inode->block_count = 0;
    inode->used = 1;

    return inode;
}

/* Lookup inode by inode number */
Inode *inode_lookup(ino_t inode_number)
{
    if (inode_number >= MAX_INODES)
        return NULL;

    if (!inode_table[inode_number].used)
        return NULL;

    return &inode_table[inode_number];
}

/*
 * Find a directory entry with a given name.
 */
Inode *inode_find_entry(
    ino_t parent_inode,
    const char *name
)
{
    Inode *parent = inode_lookup(parent_inode);

    if (parent == NULL || parent->type != INODE_DIRECTORY)
        return NULL;

    for (size_t i = 0; i < parent->entry_count; i++)
    {
        if (strcmp(parent->entries[i].name, name) == 0)
            return inode_lookup(parent->entries[i].inode_number);
    }

    return NULL;
}

/* Add an entry to a directory */
int inode_add_entry(
    ino_t parent_inode,
    const char *name,
    ino_t child_inode
)
{
    Inode *parent = inode_lookup(parent_inode);

    if (parent == NULL || parent->type != INODE_DIRECTORY)
        return -ENOTDIR;

    if (name == NULL || name[0] == '\0')
        return -EINVAL;

    if (strlen(name) > MAX_NAME_LEN)
        return -ENAMETOOLONG;

    if (inode_lookup(child_inode) == NULL)
        return -ENOENT;

    /* Prevent duplicate names */
    if (inode_find_entry(parent_inode, name) != NULL)
        return -EEXIST;

    /* Increase directory entry capacity */
    if (parent->entry_count >= parent->entry_capacity)
    {
        size_t new_capacity =
            (parent->entry_capacity == 0)
                ? 8
                : parent->entry_capacity * 2;

        DirectoryEntry *new_entries =
            realloc(
                parent->entries,
                new_capacity * sizeof(DirectoryEntry)
            );

        if (new_entries == NULL)
            return -ENOMEM;

        parent->entries = new_entries;
        parent->entry_capacity = new_capacity;
    }

    DirectoryEntry *entry =
        &parent->entries[parent->entry_count];

    strncpy(entry->name, name, MAX_NAME_LEN);
    entry->name[MAX_NAME_LEN] = '\0';
    entry->inode_number = child_inode;

    parent->entry_count++;
    parent->size =
        (off_t)(parent->entry_count * sizeof(DirectoryEntry));

    parent->mtime = current_time();
    parent->ctime = parent->mtime;

    return 0;
}

/* Remove a directory entry */
int inode_remove_entry(
    ino_t parent_inode,
    const char *name
)
{
    Inode *parent = inode_lookup(parent_inode);

    if (parent == NULL || parent->type != INODE_DIRECTORY)
        return -ENOTDIR;

    for (size_t i = 0; i < parent->entry_count; i++)
    {
        if (strcmp(parent->entries[i].name, name) == 0)
        {
            if (i < parent->entry_count - 1)
            {
                memmove(
                    &parent->entries[i],
                    &parent->entries[i + 1],
                    (parent->entry_count - i - 1)
                        * sizeof(DirectoryEntry)
                );
            }

            parent->entry_count--;

            parent->size =
                (off_t)(parent->entry_count *
                        sizeof(DirectoryEntry));

            parent->mtime = current_time();
            parent->ctime = parent->mtime;

            return 0;
        }
    }

    return -ENOENT;
}

/* Delete an inode */
int inode_delete(ino_t inode_number)
{
    Inode *inode = inode_lookup(inode_number);

    if (inode == NULL)
        return -ENOENT;

    if (inode_number == 0)
        return -EBUSY;

    free(inode->entries);
    inode->entries = NULL;

    memset(inode, 0, sizeof(Inode));

    return 0;
}

/* Update inode size */
int inode_update_size(
    ino_t inode_number,
    off_t size
)
{
    Inode *inode = inode_lookup(inode_number);

    if (inode == NULL)
        return -ENOENT;

    if (size < 0)
        return -EINVAL;

    inode->size = size;
    inode->mtime = current_time();
    inode->ctime = inode->mtime;

    return 0;
}

/* Update inode timestamps */
int inode_update_times(
    ino_t inode_number,
    int update_access,
    int update_modify
)
{
    Inode *inode = inode_lookup(inode_number);

    if (inode == NULL)
        return -ENOENT;

    struct timespec now = current_time();

    if (update_access)
        inode->atime = now;

    if (update_modify)
    {
        inode->mtime = now;
        inode->ctime = now;
    }

    return 0;
}

/*
 * Resolve a path such as:
 *
 * /documents/report.txt
 *
 * Starting from the root inode.
 */
Inode *inode_lookup_path(const char *path)
{
    if (path == NULL || path[0] != '/')
        return NULL;

    if (strcmp(path, "/") == 0)
        return inode_lookup(0);

    char path_copy[PATH_MAX];

    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    Inode *current = inode_lookup(0);

    char *saveptr = NULL;
    char *component = strtok_r(path_copy, "/", &saveptr);

    while (component != NULL)
    {
        if (current == NULL ||
            current->type != INODE_DIRECTORY)
        {
            return NULL;
        }

        current = inode_find_entry(
            current->inode_number,
            component
        );

        if (current == NULL)
            return NULL;

        component = strtok_r(NULL, "/", &saveptr);
    }

    return current;
}
