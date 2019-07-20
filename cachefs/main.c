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
#include "xattr.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

    if (memcache != NULL)
        return NULL;

    memcache = memcache_init();
    if (memcache == NULL)
        return NULL;

    init_inodes(memcache);

    if (!memcache_is_consistent(memcache)) {
        assert(memcache_clear(memcache));
        memcache_create(memcache);
        assert(init_freemap(memcache, 1024));
        int root_inode = get_free_inode();

        assert(root_inode == 0);
        assert(dir_create(root_inode, 0, 0, 0755 | S_IFDIR));
        struct dir* root = dir_open_root();
        assert(dir_add(root, ".", root_inode));
        assert(dir_add(root, "..", root_inode));
        inode_path_register("/", root_inode);
    }

    return NULL;
}

static int cachefs_getattr(const char* path, struct stat* stbuf,
    struct fuse_file_info* fi)
{
    printf("begin getattr\n path: %s\n", path);
    (void)fi;
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));

    struct inode* inode = inode_get_from_path(path);

    if (inode == NULL) {
        return -ENOENT;
    }

    stbuf->st_mode = inode->metadata.mode;
    stbuf->st_nlink = inode->metadata.link_cnt;
    stbuf->st_size = inode_length(inode);
    stbuf->st_uid = inode->metadata.uid;
    stbuf->st_gid = inode->metadata.gid;

    inode_close(inode);
    printf("end getattr\n");
    return res;
}

static int cachefs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info* fi,
    enum fuse_readdir_flags flags)
{
    (void)offset;
    (void)fi;
    (void)flags;
    printf("begin readdir\n");

    struct inode* inode = (struct inode*)fi->fh;

    if (inode == NULL || !is_inode(inode)) {
        inode = inode_get_from_path(path);
    }
    if (inode == NULL) {
        return -ENOENT;
    }
    fi->fh = (uint64_t)inode;
    struct dir* dir = dir_open(inode);
    char name[30];
    while (dir_readdir(dir, name)) {
        filler(buf, name, NULL, 0, 0);
    }
    dir_close(dir);
    printf("end readdir\n");
    return 0;
}

static int cachefs_open(const char* path, struct fuse_file_info* fi)
{
    printf("open\n");

    struct inode* inode = inode_get_from_path(path);
    if (inode == NULL) {
        return -ENOENT;
    }
    if (inode_is_dir(inode)) {
        inode_close(inode);
        return -EISDIR;
    }
    printf("end open\n");
    fi->fh = (uint64_t)inode;
    return 0;
}

static int cachefs_read(const char* path, char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi)
{
    size_t len;
    (void)fi;
    printf("read\n");

    struct inode* inode = (struct inode*)fi->fh;

    if (inode == NULL || !is_inode(inode)) {
        inode = inode_get_from_path(path);
    }
    if (inode == NULL) {
        return -ENOENT;
    }
    if (inode_is_dir(inode)) {
        inode_close(inode);
        return -EISDIR;
    }

    return inode_read_at(inode, buf, size, offset, false);
}

static int cachefs_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    struct inode* inode = (struct inode*)fi->fh;

    if (inode == NULL || !is_inode(inode)) {
        inode = inode_get_from_path(path);
    }
    if (inode == NULL) {
        return -ENOENT;
    }
    if (inode_is_dir(inode)) {
        inode_close(inode);
        return -EISDIR;
    }

    return inode_write_at(inode, buf, size, offset, false);
}

static int cachefs_statfs(const char* path, struct statvfs* buff)
{
    printf("statfs\n");
    return 0;
}

static void cachefs_destroy(void* private_data)
{
    printf("destroy\n");
    memcache_close(memcache);
}

static int cachefs_mkdir(const char* path, mode_t mode)
{
    printf("begin mkdir\n");
    char dir_path[strlen(path)];
    char file_name[NAME_MAX + 1];
    if (!split_file_path(path, dir_path, file_name)) {
        return -1;
    }

    struct inode* parent_inode = inode_get_from_path(dir_path);
    if (parent_inode == NULL) {
        return -ENOENT;
    }

    struct inode* current = inode_get_from_path(path);
    if (current != NULL) {
        inode_close(current);
        inode_close(parent_inode);
        return -EEXIST;
    }

    struct fuse_context* fuse_context = fuse_get_context();

    int inode_id = get_free_inode();
    if (inode_id < 0) {
        inode_close(parent_inode);
        return -ENOSPC;
    }

    assert(dir_create(inode_id, fuse_context->gid, fuse_context->uid, mode | S_IFDIR));

    struct dir* parent = dir_open(parent_inode);
    struct dir* child = dir_open(inode_open(inode_id));

    assert(parent != NULL);
    assert(child != NULL);

    assert(dir_add(parent, file_name, inode_id));
    assert(dir_add(child, ".", inode_id));
    assert(dir_add(child, "..", dir_get_inode(parent)->id));
    inode_path_register(path, inode_id);

    dir_close(parent);
    dir_close(child);

    printf("end mkdir\n");
    return 0;
}

static int cachefs_rmdir(const char* path)
{

    printf("begin rmdir %s\n", path);
    char dir_path[strlen(path)];
    char file_name[NAME_MAX + 1];
    if (!split_file_path(path, dir_path, file_name)) {
        return -1;
    }

    if (!strcmp(path, "/"))
        return -EPERM;

    struct inode* inode = inode_get_from_path(path);
    if (inode == NULL) {
        return -ENOENT;
    }

    if (!inode_is_dir(inode)) {
        inode_close(inode);
        return -ENOTDIR;
    }

    struct dir* child = dir_open(inode);

    if (!dir_is_empty(child)) {
        dir_close(child);
        return -ENOENT;
    }

    struct dir* parent = dir_open(inode_get_from_path(dir_path));
    dir_remove(parent, file_name);
    inode_path_delete(path);
    dir_close(parent);
    return 0;
}

static int cachefs_opendir(const char* path, struct fuse_file_info* fi)
{
    printf("begin opendir %s\n", path);
    struct inode* inode = inode_get_from_path(path);
    if (inode == NULL) {
        return -ENOENT;
    }
    if (!inode_is_dir(inode)) {
        return -ENOTDIR;
    }
    struct dir* dir = dir_open(inode);
    fi->fh = (uint64_t)dir_get_inode(dir);
    printf("end opendir\n");
    return 0;
}

static int cachefs_releasedir(const char* path, struct fuse_file_info* fi)
{
    printf("begin releasedir\n");
    struct inode* inode = (struct inode*)fi->fh;
    if (inode == NULL || !is_inode(inode)) {
        return -ENOTDIR;
    }

    inode_close(inode);
    fi->fh = 0;
    printf("end releasedir\n");
    return 0;
}

static int cachefs_fsyncdir(const char* path, int isdatasync, struct fuse_file_info* fi)
{
    printf("fsyncdir\n");
    return 0;
}

static int cachefs_unlink(const char* path)
{
    printf("start unlink\n");
    char dir_path[strlen(path)];
    char file_name[NAME_MAX + 1];
    if (!split_file_path(path, dir_path, file_name)) {
        return -1;
    }

    struct inode* inode = inode_get_from_path(path);
    if (inode == NULL) {
        return -ENOENT;
    }

    struct dir* dir = dir_open(inode_get_from_path(dir_path));
    dir_remove(dir, file_name);
    inode_path_delete(path);
    inode->metadata.link_cnt--;
    inode_flush_metadata(inode);
    inode_remove(inode);
    return 0;
}
static int cachefs_create(const char* path, mode_t mode, struct fuse_file_info* fi)
{
    printf("create\n");
    struct inode* inode = inode_get_from_path(path);

    if (inode != NULL) {
        return -EEXIST;
    }

    char dir_path[strlen(path)];
    char file_name[NAME_MAX + 1];
    if (!split_file_path(path, dir_path, file_name)) {
        return -1;
    }

    struct inode* parent_inode = inode_get_from_path(dir_path);
    if (parent_inode == NULL) {
        return -ENOENT;
    }

    struct inode* current = inode_get_from_path(path);
    if (current != NULL) {
        inode_close(current);
        inode_close(parent_inode);
        return -EEXIST;
    }

    struct fuse_context* fuse_context = fuse_get_context();

    int inode_id = get_free_inode();
    if (inode_id < 0) {
        return -ENOSPC;
    }

    assert(inode_create(inode_id, false, fuse_context->gid, fuse_context->uid, mode | S_IFREG));

    struct dir* parent = dir_open(parent_inode);
    struct inode* child = inode_open(inode_id);

    assert(parent != NULL);
    assert(child != NULL);

    assert(dir_add(parent, file_name, inode_id));
    inode_path_register(path, inode_id);
    printf("end create\n");
    return 0;
}

static int cachefs_release(const char* path, struct fuse_file_info* fi)
{
    printf("start release\n");
    struct inode* inode = (struct inode*)fi->fh;

    if (inode == NULL || !is_inode(inode)) {
        return 0;
    }
    if (inode_is_dir(inode)) {
        return -EISDIR;
    }
    inode_close(inode);
    fi->fh = 0;
    printf("end release\n");
    return 0;
}

static int cachefs_flush(const char* path, struct fuse_file_info* fi)
{
    printf("start flush\n");
    return 0;
}
static int cachefs_fsync(const char* path, int isdatasync, struct fuse_file_info* fi)
{
    printf("start fsync\n");
    return 0;
}

static int cachefs_access(const char* path, int mask)
{
    printf("access\n");
    return 0;
}

static int cachefs_setxattr(const char* path, const char* name, const char* buff, size_t size, int flags)
{
    printf("start setxattr\n");
    struct inode* inode = inode_get_from_path(path);
    if (inode == NULL)
        return -ENOENT;
    if (xattr_add(inode, name, buff, size))
        return 0;
    else
        return -1;
}

static int cachefs_getxattr(const char* path, const char* name, char* buff, size_t size)
{
    printf("start getxattr\n");
    struct inode* inode = inode_get_from_path(path);
    if (inode == NULL)
        return -1;
    return xattr_get(inode, name, buff, size);
}

static int cachefs_listxattr(const char* path, char* buff, size_t size)
{
    printf("start listxattr %zu\n", size);
    struct inode* inode = inode_get_from_path(path);
    if (inode == NULL)
        return -1;
    return xattr_list(inode, buff, size);
}

static int cachefs_removexattr(const char* path, const char* name)
{
    printf("start removexattr\n");
    struct inode* inode = inode_get_from_path(path);
    if (inode == NULL)
        return -1;
    if (xattr_remove(inode, name)) {
        return 0;
    } else {
        return -1;
    }
}

static int cachefs_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info* fi)
{
    return 0;
}

static int cachefs_chmod(const char* path, mode_t mode, struct fuse_file_info* fi)
{
    struct inode* inode = inode_get_from_path(path);
    if (inode == NULL)
        return -ENOENT;
    inode->metadata.mode = mode;
    inode_flush_metadata(inode);
    inode_close(inode);
    return 0;
}

static int cachefs_chown(const char* path, uid_t uid, gid_t gid, struct fuse_file_info* fi)
{
    struct inode* inode = inode_get_from_path(path);
    if (inode == NULL)
        return -ENOENT;
    inode->metadata.gid = gid;
    inode->metadata.uid = uid;
    inode_flush_metadata(inode);
    inode_close(inode);
    return 0;
}

static int cachefs_truncate(const char* path, off_t size, struct fuse_file_info* fi)
{
}

static int cachefs_link(const char* from, const char* to)
{
    printf("begin link %s %s\n", from, to);
    char dir_path[strlen(to)];
    char file_name[NAME_MAX + 1];
    if (!split_file_path(to, dir_path, file_name)) {
        return -1;
    }

    struct dir* to_dir = dir_open(inode_get_from_path(dir_path));
    if (to_dir == NULL) {
        printf("cant open from dir : %s\n", dir_path);
        return -ENOENT;
    }

    struct inode* from_inode = inode_get_from_path(from);
    if (from_inode == NULL) {
        printf("cant open from file : %s\n", from);
        dir_close(to_dir);
        return -ENOENT;
    }

    if (inode_is_dir(from_inode)) {
        dir_close(to_dir);
        inode_close(from_inode);
        return -EPERM;
    }

    if (inode_get_from_path(to)) {
        dir_close(to_dir);
        inode_close(from_inode);
        return -EEXIST;
    }

    assert(dir_add(to_dir, file_name, from_inode->id));
    inode_path_register(to, from_inode->id);
    from_inode->metadata.link_cnt++;
    inode_flush_metadata(from_inode);
    inode_close(from_inode);
    dir_close(to_dir);
    printf("end link\n");
    return 0;
}
static int cachefs_symlink(const char* to, const char* from)
{
    printf("\n\nto : %s from :%s\n\n", to, from);

    printf("create\n");

    struct inode* inode = inode_get_from_path(from);

    if (inode != NULL) {
        return -EEXIST;
    }

    char dir_path[strlen(from)];
    char file_name[NAME_MAX + 1];
    if (!split_file_path(from, dir_path, file_name)) {
        return -1;
    }

    struct fuse_context* fuse_context = fuse_get_context();

    int inode_id = get_free_inode();
    if (inode_id < 0) {
        return -ENOSPC;
    }

    assert(inode_create(inode_id, false, fuse_context->gid, fuse_context->uid, 0666 | S_IFLNK));

    struct dir* parent = dir_open(inode_get_from_path(dir_path));
    struct inode* child = inode_open(inode_id);

    assert(parent != NULL);
    assert(child != NULL);

    assert(dir_add(parent, file_name, inode_id));
    inode_path_register(from, inode_id);
    inode_write_at(inode, to, sizeof(to) + 1, 0, false);
    printf("end symlink\n");

    return 0;
}
static int cachefs_readlink(const char* path, char* buf, size_t size)
{
    struct inode* inode = inode_get_from_path(path);
    if (inode == NULL)
        return -ENOENT;

    return inode_read_at(inode, buf, size, 0, false);
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
    .destroy = cachefs_destroy,
    .unlink = cachefs_unlink,
    .create = cachefs_create,
    .write = cachefs_write,
    .release = cachefs_release,
    .flush = cachefs_flush,
    .fsync = cachefs_fsync,
    .access = cachefs_access,
    .setxattr = cachefs_setxattr,
    .getxattr = cachefs_getxattr,
    .listxattr = cachefs_listxattr,
    .removexattr = cachefs_removexattr,
    .utimens = cachefs_utimens,
    .chmod = cachefs_chmod,
    .chown = cachefs_chown,
    .truncate = cachefs_truncate,
    .link = cachefs_link,
    .symlink = cachefs_symlink,
    .readlink = cachefs_readlink

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