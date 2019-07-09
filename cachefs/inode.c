#include "inode.h"
#include "list.h"
#include "memcache.h"
#include <pthread.h>
#include <string.h>

#define INODE_BLOCK_SIZE 512

static void
get_key(char* key, int ind)
{
    sprintf(key, "INODE#%d", ind);
}

static void
get_metadata(char* key)
{
    sprintf(key, "INODE#METADATA");
}

struct memcache_t* memcache;

struct inode_disk_metadata {
    size_t length;
    bool is_dir;
    size_t link_cnt;
};

struct inode {
    int id;
    int open_cnt;
    struct list_elem elem;
    pthread_mutex_t lock;
    struct inode_disk_metadata metadata;
};

static struct list open_inodes;

void init_inodes(struct memcache_t* mem)
{
    memcache = mem;
    list_init(&open_inodes);
}

struct inode* inode_open(int id)
{
    struct list_elem* e;
    struct inode* inode;

    for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
         e = list_next(e)) {
        inode = list_entry(e, struct inode, elem);
        if (inode->id == id) {
            inode_reopen(inode);
            return inode;
        }
    }

    inode = malloc(sizeof(struct inode));
    if (inode == NULL)
        return NULL;

    list_push_back(&open_inodes, &inode->elem);
    inode->id = id;
    inode->open_cnt = 1;
    char key[30];
    get_metadata(key);
    memcache_get(memcache, key, &inode->metadata);
    return inode;
}

struct inode* inode_reopen(struct inode* inode)
{
    if (inode != NULL) {
        inode->open_cnt++;
    }
    return inode;
}

size_t inode_read_at(struct inode* inode, void* buff, size_t size,
    size_t offset)
{
    char* buffer = buff;
    size_t current = offset;
    size_t end = offset + size;
    size_t read = 0;
    while (current < end) {
        size_t current_block = current / INODE_BLOCK_SIZE;
        size_t next_offset = (current_block + 1) * INODE_BLOCK_SIZE;
        char key[30];
        get_key(key, current_block);
        char value[INODE_BLOCK_SIZE];
        if (!memcache_get(memcache, key, value))
            goto inode_read_at_end;

        size_t in_block_end
            = next_offset > end ? next_offset - 1 : end - 1;
        in_block_end %= INODE_BLOCK_SIZE;
        size_t in_block_start = current;
        in_block_start %= INODE_BLOCK_SIZE;
        memcpy(buffer + read, value + in_block_start, in_block_end - in_block_start + 1);
        if (!memcache_add(memcache, key, value, INODE_BLOCK_SIZE))
            goto inode_read_at_end;
        read += in_block_end - in_block_start + 1;
        current = next_offset;
    }

inode_read_at_end:

    return read;
}

size_t inode_write_at(struct inode* inode, const void* buff, size_t size,
    size_t offset)
{
    const char* buffer = buff;
    size_t current = offset;
    size_t end = offset + size;
    size_t written = 0;
    while (current < end) {
        size_t current_block = current / INODE_BLOCK_SIZE;
        size_t next_offset = (current_block + 1) * INODE_BLOCK_SIZE;
        char key[30];
        get_key(key, current_block);
        char value[INODE_BLOCK_SIZE];
        if (!memcache_get(memcache, key, value))
            goto inode_write_at_end;

        size_t in_block_end
            = next_offset > end ? next_offset - 1 : end - 1;
        in_block_end %= INODE_BLOCK_SIZE;
        size_t in_block_start = current;
        in_block_start %= INODE_BLOCK_SIZE;
        memcpy(value + in_block_start, buffer + written, in_block_end - in_block_start + 1);
        if (!memcache_add(memcache, key, value, INODE_BLOCK_SIZE))
            goto inode_write_at_end;
        written += in_block_end - in_block_start + 1;
        current = next_offset;
    }

inode_write_at_end:
    if (inode->metadata.length < offset + written) {
        inode->metadata.length = offset + written;
        char key[30];
        get_metadata(key);
        memcache_add(memcache, key, &inode->metadata, sizeof(inode->metadata));
    }

    return written;
}