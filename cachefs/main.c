/*
  FUSE: Filesystem in Userspace
  Copiyright (C) 2001-2007  Miklos Szered <miklos@szeredi.hu>
  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#define FUSE_USE_VERSION 31

#include "directory.h"
#include "freemap.h"
#include "inode.h"
#include "memcache.h"
#include "utils.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/*
 * Command line options
 *
 * We can't set default values for the char* fields here because
 * fuse_opt_parse would attempt to free() them when the user specifies
 * different values on the command line.
 */
static struct options {
    const char* filename;
    const char* contents;
    int show_help;
} options;

struct memcache_t* memcache = NULL;

#define OPTION(t, p)                      \
    {                                     \
        t, offsetof(struct options, p), 1 \
    }

static const struct fuse_opt option_spec[] = {
    OPTION("--name=%s", filename), OPTION("--contents=%s", contents),
    OPTION("-h", show_help), OPTION("--help", show_help), FUSE_OPT_END
};

static void* cachefs_init(struct fuse_conn_info* conn,
    struct fuse_config* cfg)
{
    (void)conn;
    cfg->kernel_cache = 1;

    memcache = memcache_init();
    if (memcache == NULL)
        return NULL;

    init_inodes(memcache);

    if (!memcache_is_consistent(memcache)) {
        assert(memcache_clear(memcache));
        memcache_create(memcache);
        assert(init_freemap(memcache, 1024));
        int root_inode = get_free_inode();
        printf("%d\n", root_inode);
        assert(root_inode == 0);
        assert(dir_create(root_inode, 0, 0, 0));
        struct dir* root = dir_open_root();
        dir_add(root, ".", 1);
        dir_add(root, "..", 1);
    }

    printf("FileSistem Initialized\n\n\n");

    return NULL;
}

static int cachefs_getattr(const char* path, struct stat* stbuf,
    struct fuse_file_info* fi)
{
    printf("getattr\n path: %s\n", path);
    (void)fi;
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));

    struct inode* inode = inode_from_path(path);
    if (inode == NULL)
        return -ENOENT;

    stbuf->st_mode = inode->metadata.mode;
    stbuf->st_nlink = inode->metadata.link_cnt;
    stbuf->st_size = inode_length(inode);
    stbuf->st_uid = inode->metadata.uid;
    stbuf->st_gid = inode->metadata.gid;
    inode_close(inode);
    return res;
}

static int cachefs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info* fi,
    enum fuse_readdir_flags flags)
{
    (void)offset;
    (void)fi;
    (void)flags;
    printf("readdir\n");

    struct inode* inode = inode_from_path(path);
    if (inode == NULL || !inode_is_dir(inode))
        return -ENOENT;

    struct dir* dir = dir_open(inode);
    char name[15];
    while (dir_readdir(dir, name)) {
        filler(buf, name, NULL, 0, 0);
    }

    return 0;
}

static int cachefs_open(const char* path, struct fuse_file_info* fi)
{
    printf("open\n");
    if (strcmp(path + 1, options.filename) != 0)
        return -ENOENT;

    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EACCES;

    return 0;
}

static int cachefs_read(const char* path, char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi)
{
    size_t len;
    (void)fi;
    printf("read\n");
    if (strcmp(path + 1, options.filename) != 0)
        return -ENOENT;

    len = strlen(options.contents);
    if (offset < len) {
        if (offset + size > len)
            size = len - offset;
        memcpy(buf, options.contents + offset, size);
    } else
        size = 0;
    return size;
}

static int cachefs_statfs(const char* path, struct statvfs* buff)
{
    printf("statfs\n");
}
static void cachefs_destroy(void* private_data)
{
    printf("destroy\n");
    memcache_close(memcache);
}
static int cachefs_mkdir(const char* path, mode_t mode)
{
    printf("mkdir\n");
    char dir_path[strlen(path)];
    char file_name[NAME_MAX + 1];
    if (!split_file_path(path, dir_path, file_name))
        return -1;

    struct fuse_context* fuse_context = fuse_get_context();

    int inode_id = get_free_inode();
    if (inode_id < 0)
        return -1;
    if (!dir_create(inode_id, fuse_context->gid, fuse_context->uid, mode)) {
        free_inode(inode_id);
        return -1;
    }

    struct dir* parent = dir_open_path(NULL, dir_path);
    struct dir* child = dir_open(inode_open(inode_id));
    dir_add(parent, file_name, inode_id);
    dir_add(child, ".", inode_id);
    dir_add(parent, "..", dir_get_inode(parent)->id);
    return 0;
}

static int cachefs_rmdir(const char* path)
{
}

static int cachefs_opendir(const char* path, struct fuse_file_info* fi)
{
    printf("opendir\n");
}

static int cachefs_releasedir(const char* path, struct fuse_file_info* fi)
{
    printf("releasedir\n");
}

static int cachefs_fsyncdir(const char* path, int isdatasync, struct fuse_file_info* fi)
{
    printf("fsyncdir\n");
}

static struct fuse_operations cachefs_oper = {
    .init = cachefs_init,
    .getattr = cachefs_getattr,
    .mkdir = cachefs_mkdir,
    .rmdir = cachefs_rmdir,
    .opendir = cachefs_opendir,
    .readdir = cachefs_readdir,
    .releasedir = cachefs_releasedir,
    .fsyncdir = cachefs_fsyncdir,
    .open = cachefs_open,
    .read = cachefs_read,
    .statfs = cachefs_statfs,
    .destroy = cachefs_destroy
};

static void show_help(const char* progname)
{
    printf("usage: %s [options] <mountpoint>\n\n", progname);
    printf("File-system specific options:\n"
           "    --name=<s>          Name of the \"hello\" file\n"
           "                        (default: \"hello\")\n"
           "    --contents=<s>      Contents \"hello\" file\n"
           "                        (default \"Hello, World!\\n\")\n"
           "\n");
}

int main(int argc, char* argv[])
{
    int ret;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    /* Set defaults -- we have to use strdup so that
   fuse_opt_parse can free the defaults if other
   values are specified */
    options.filename = strdup("hello");
    options.contents = strdup("Hello World!\n");

    /* Parse options */
    if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
        return 1;

    /* When --help is specified, first print our own file-system
   specific help text, then signal fuse_main to show
   additional help (by adding `--help` to the options again)
   without usage: line (by setting argv[0] to the empty
   string) */
    if (options.show_help) {
        show_help(argv[0]);
        assert(fuse_opt_add_arg(&args, "--help") == 0);
        args.argv[0][0] = '\0';
    }

    ret = fuse_main(args.argc, args.argv, &cachefs_oper, NULL);
    fuse_opt_free_args(&args);
    return ret;
}