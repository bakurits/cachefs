/*
  FUSE: Filesystem in Userspace
  Copiyright (C) 2001-2007  Miklos Szered <miklos@szeredi.hu>
  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#define FUSE_USE_VERSION 31

#include "memcache.h"
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
    if (memcache != NULL && !memcache_is_consistent(memcache)) {
        memcache_clear(memcache);
        memcache_create(memcache);
    }

    printf("FileSistem Initialized\n\n\n");

    return NULL;
}

static int cachefs_getattr(const char* path, struct stat* stbuf,
    struct fuse_file_info* fi)
{
    (void)fi;
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (strcmp(path + 1, options.filename) == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(options.contents);
    } else
        res = -ENOENT;

    return res;
}

static int cachefs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info* fi,
    enum fuse_readdir_flags flags)
{
    (void)offset;
    (void)fi;
    (void)flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, options.filename, NULL, 0, 0);

    return 0;
}

static int cachefs_open(const char* path, struct fuse_file_info* fi)
{
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
}
static void cachefs_destroy(void* private_data)
{
    memcache_close(memcache);
}

static struct fuse_operations cachefs_oper = {
    .init = cachefs_init,
    .getattr = cachefs_getattr,
    .readdir = cachefs_readdir,
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

    struct memcache_t* memcache = memcache_init();
    int a = 4;
    char b[20];
    char c[20];
    strcpy(b, "tsutskhashvili");
    for (int i = 1; i <= 10; i++) {
        memcache_add(memcache, "bakuri", b, strlen(b));
        memcache_get(memcache, "bakuri", &c);
        printf("%s\n", c);
        assert(strcmp(b, c) == 0);
    }

    return 0;
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