
#include "inode.h"
#include "freemap.h"
#include "utils.h"
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define INODE_BLOCK_SIZE 4096
#define INODE_MAGIC 2341785

static void
get_key(char* key, int inode_id, int ind)
{
    sprintf(key, "%d#%d", inode_id, ind);
}

static void
get_metadata(char* key, int inode_id)
{
    sprintf(key, "%d#METADATA", inode_id);
}
static void
get_xattrs(char* key, int inode_id, int ind)
{
    sprintf(key, "%d#XATTRS%d", inode_id, ind);
}

static struct memcache_t* memcache;
static struct list open_inodes;
static pthread_mutex_t inodes_lock;
__gid_t root_g_id;
__uid_t root_u_id;

void init_inodes(struct memcache_t* mem, __gid_t gid, __uid_t uid)
{
    memcache = mem;
    list_init(&open_inodes);
    root_g_id = gid;
    root_u_id = uid;
    pthread_mutex_init(&inodes_lock, NULL);
}

bool inode_create(int inode_id, bool is_dir, __gid_t gid, __uid_t uid, __mode_t mode)
{
    struct inode_disk_metadata disk_inode;

    disk_inode.is_dir = is_dir;
    disk_inode.length = 0;
    disk_inode.link_cnt = 1;
    disk_inode.gid = gid;
    disk_inode.uid = uid;
    disk_inode.mode = mode;
    disk_inode.xattrs_length = 0;

    char key[30];
    get_metadata(key, inode_id);

    bool res = memcache_add(memcache, key, &disk_inode, sizeof(struct inode_disk_metadata));

    return res;
}

struct inode* inode_open(int id)
{

    struct list_elem* e;
    struct inode* inode;

    pthread_mutex_lock(&inodes_lock);
    for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
         e = list_next(e)) {
        inode = list_entry(e, struct inode, elem);
        if (inode->id == id) {
            inode_reopen(inode);
            pthread_mutex_unlock(&inodes_lock);
            return inode;
        }
    }

    inode = malloc(sizeof(struct inode));
    if (inode == NULL) {
        pthread_mutex_unlock(&inodes_lock);
        return NULL;
    }

    list_push_back(&open_inodes, &inode->elem);
    inode->id = id;
    inode->open_cnt = 1;
    inode->is_deleted = false;
    inode->magic = INODE_MAGIC;
    pthread_mutex_init(&inode->lock, NULL);
    char key[30];
    get_metadata(key, inode->id);
    pthread_mutex_unlock(&inodes_lock);
    if (memcache_get(memcache, key, &inode->metadata))
        return inode;
    else
        return NULL;
}

struct inode* inode_reopen(struct inode* inode)
{
    if (inode == NULL)
        return NULL;
    pthread_mutex_lock(&inode->lock);
    inode->open_cnt++;
    pthread_mutex_unlock(&inode->lock);
    return inode;
}

size_t inode_read_at(struct inode* inode, void* buff, size_t size,
    size_t offset, bool xattrs)
{
    pthread_mutex_lock(&inode->lock);
    char* buffer = buff;
    size_t current = offset;
    size_t end = offset + size;
    size_t read = 0;

    size_t length = xattrs ? inode->metadata.xattrs_length : inode->metadata.length;

    if (end > length) {
        end = length;
    }

    while (current < end) {
        size_t current_block = current / INODE_BLOCK_SIZE;
        size_t next_offset = (current_block + 1) * INODE_BLOCK_SIZE;

        size_t in_block_end
            = next_offset <= end ? next_offset - 1 : end - 1;
        in_block_end %= INODE_BLOCK_SIZE;
        size_t in_block_start = current;
        in_block_start %= INODE_BLOCK_SIZE;
        char key[30];
        if (!xattrs) {
            get_key(key, inode->id, current_block);
        } else {
            get_xattrs(key, inode->id, current_block);
        }

        char value[INODE_BLOCK_SIZE];
        if (!memcache_get(memcache, key, value))
            goto inode_read_at_end;

        memcpy(buffer + read, value + in_block_start, in_block_end - in_block_start + 1);
        read += in_block_end - in_block_start + 1;
        current = next_offset;
    }

inode_read_at_end:
    pthread_mutex_unlock(&inode->lock);
    return read;
}

size_t inode_write_at(struct inode* inode, const void* buff, size_t size,
    size_t offset, bool xattrs)
{
    pthread_mutex_lock(&inode->lock);
    bool metadate_modified = false;
    const char* buffer = buff;
    size_t current = offset;
    size_t end = offset + size;
    size_t written = 0;
    size_t length = xattrs ? inode->metadata.xattrs_length : inode->metadata.length;

    size_t block_cnt = length / INODE_BLOCK_SIZE;
    if (block_cnt * INODE_BLOCK_SIZE < length)
        block_cnt++;

    /* printf("\n\nstarted writing in %d offset : %zu\n\n", inode->id, offset); */

    while (current < end) {
        size_t current_block = current / INODE_BLOCK_SIZE;
        size_t current_start = current * INODE_BLOCK_SIZE;
        size_t next_offset = (current_block + 1) * INODE_BLOCK_SIZE;

        size_t in_block_end
            = next_offset <= end ? next_offset - 1 : end - 1;
        in_block_end %= INODE_BLOCK_SIZE;
        size_t in_block_start = current;
        in_block_start %= INODE_BLOCK_SIZE;

        /* printf("copping in block %zu\n", current_block);
        printf("from : %zu to : %zu \n\n", in_block_start, in_block_end); */
        char key[30];
        if (!xattrs) {
            get_key(key, inode->id, current_block);
        } else {
            get_xattrs(key, inode->id, current_block);
        }
        char value[INODE_BLOCK_SIZE];
        if (current_block < block_cnt && (in_block_end != INODE_BLOCK_SIZE - 1 || in_block_start != 0)) {
            memcache_get(memcache, key, value);
        }

        memcpy(value + in_block_start, buffer + written, in_block_end - in_block_start + 1);
        if (!memcache_add(memcache, key, value, INODE_BLOCK_SIZE))
            goto inode_write_at_end;
        written += in_block_end - in_block_start + 1;
        current = next_offset;
    }

inode_write_at_end:
    if (written > 0 && length < offset + written) {
        length = offset + written;
        if (xattrs)
            inode->metadata.xattrs_length = length;
        else
            inode->metadata.length = length;
        char key[30];
        get_metadata(key, inode->id);
        memcache_add(memcache, key, &inode->metadata, sizeof(inode->metadata));
    }
    pthread_mutex_unlock(&inode->lock);

    return written;
}

void inode_close(struct inode* inode)
{
    if (inode == NULL)
        return;
    pthread_mutex_lock(&inode->lock);
    inode->open_cnt--;
    if (inode->open_cnt == 0) {
        list_remove(&inode->elem);
        if (inode->is_deleted) {
            char key[30];
            for (int i = 0; i <= inode->metadata.length / INODE_BLOCK_SIZE; i++) {
                get_key(key, inode->id, i);
                memcache_delete(memcache, key);
            }
            for (int i = 0; i <= inode->metadata.xattrs_length / INODE_BLOCK_SIZE; i++) {
                get_xattrs(key, inode->id, i);
                memcache_delete(memcache, key);
            }

            get_metadata(key, inode->id);
            memcache_delete(memcache, key);
            free_inode(inode->id);
        }

        free(inode);
    }
    pthread_mutex_unlock(&inode->lock);
}

void inode_remove(struct inode* inode)
{
    if (inode == NULL)
        return;
    pthread_mutex_lock(&inode->lock);
    inode->is_deleted = true;
    pthread_mutex_unlock(&inode->lock);
}

size_t inode_length(struct inode* inode)
{
    if (inode == NULL)
        return 0;
    return inode->metadata.length;
}
bool inode_is_dir(struct inode* inode)
{
    if (inode == NULL)
        return false;
    return inode->metadata.is_dir;
}

bool inode_path_register(const char* path, int inode_id)
{
    char key[256];
    sprintf(key, "ipth#%s", path);
    return memcache_add(memcache, key, &inode_id, sizeof(int));
}

struct inode* inode_get_from_path(const char* path)
{
    /* printf("getting key : %s\n\n", path); */
    char key[256];
    sprintf(key, "ipth#%s", path);
    int res = -1;

    if (memcache_get(memcache, key, &res)) {
        /*         printf("\n\n\n resssss :          %d\n", res); */
        return inode_open(res);
    }
    return NULL;
}

bool inode_path_delete(const char* path)
{
    char key[256];
    sprintf(key, "ipth#%s", path);
    return memcache_delete(memcache, key);
}

bool is_inode(struct inode* inode)
{
    return inode != NULL && inode->magic == INODE_MAGIC;
}

size_t inode_xattrs_length(struct inode* inode)
{
    return inode->metadata.xattrs_length;
}

bool inode_flush_metadata(struct inode* inode)
{
    if (inode == NULL)
        return false;
    char key[30];
    get_metadata(key, inode->id);
    return memcache_add(memcache, key, &inode->metadata, sizeof(inode->metadata));
}

bool inode_check_permission(struct inode* inode, permission_t permission)
{
    /*  printf("\n\n\n permissions\n");
    printf("ids %d %d\n", (int)getuid(), (int)inode->metadata.uid);
    printf("gids %d %d\n", (int)getgid(), (int)inode->metadata.gid);
    printf("mode %d\n\n\n", (int)inode->metadata.mode);
    printf("root %d %d\n\n\n", (int)root_g_id, (int)root_u_id); */
    if (getuid() == root_u_id || getgid() == root_g_id) {
        return true;
    }
    if (getuid() == inode->metadata.uid) {
        switch (permission) {
        case READ:
            return (inode->metadata.mode & S_IRUSR) != 0;
        case WRITE:
            return (inode->metadata.mode & S_IWUSR) != 0;
        default:
            return (inode->metadata.mode & S_IXUSR) != 0;
        }
    } else if (getgid() == inode->metadata.gid) {
        switch (permission) {
        case READ:
            return (inode->metadata.mode & S_IRGRP) != 0;
        case WRITE:
            return (inode->metadata.mode & S_IWGRP) != 0;
        case EXECUTE:
            return (inode->metadata.mode & S_IXGRP) != 0;
        }
    } else {
        switch (permission) {
        case READ:
            return (inode->metadata.mode & S_IROTH) != 0;
        case WRITE:
            return (inode->metadata.mode & S_IWOTH) != 0;
        case EXECUTE:
            return (inode->metadata.mode & S_IXOTH) != 0;
        }
    }
    return false;
}