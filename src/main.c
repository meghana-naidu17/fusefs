#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>

#include "fusefs.h"

static struct fuse_operations fusefs_operations = {
    .getattr = fusefs_getattr,
    .readdir = fusefs_readdir,
    .open = fusefs_open,
    .read = fusefs_read,
    .write = fusefs_write,
    .create = fusefs_create,
    .unlink = fusefs_unlink,
    .truncate = fusefs_truncate,
    .mkdir = fusefs_mkdir,
    .rmdir = fusefs_rmdir
};

int main(int argc, char *argv[])
{
    if (fusefs_init_filesystem() != 0)
    {
        fprintf(stderr, "Failed to initialize FuseFS\n");
        return 1;
    }

    int result = fuse_main(
        argc,
        argv,
        &fusefs_operations,
        NULL
    );

    fusefs_destroy_filesystem();

    return result;
}
