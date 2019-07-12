#include "inode.h"
#include "freemap.h"
#include <stdlib.h>
#include <string.h>

#define INODE_BLOCK_SIZE 512

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

static struct memcache_t* memcache;
static struct list open_inodes;
static pthread_mutex_t inodes_lock;

void init_inodes(struct memcache_t* mem)
{
    memcache = mem;
    list_init(&open_inodes);
    pthread_mutex_init(&inodes_lock, NULL);
}

int inode_create(int inode_id, bool is_dir, __gid_t gid, __uid_t uid, __mode_t mode)
{
    struct inode_disk_metadata* disk_inode = NULL;

    disk_inode = malloc(sizeof(struct inode_disk_metadata));
    if (disk_inode == NULL) {
        return false;
    }

    disk_inode->is_dir = is_dir;
    disk_inode->length = 0;
    disk_inode->link_cnt = 1;
    disk_inode->gid = gid;
    disk_inode->uid = uid;
    disk_inode->mode = mode;

    char key[30];
    get_metadata(key, inode_id);
    bool success = memcache_add(memcache, key, &disk_inode, sizeof(disk_inode));
    free(disk_inode);

    return success;
}

struct inode* inode_open(int id)
{
    printf("inode_open\n");
    struct list_elem* e;
    struct inode* inode;

    pthread_mutex_lock(&inodes_lock);
    for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
         e = list_next(e)) {
        inode = list_entry(e, struct inode, elem);
        if (inode->id == id) {
            inode_reopen(inode);
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
    char key[30];
    get_metadata(key, inode->id);
    pthread_mutex_unlock(&inodes_lock);
    // TODO: ლისტიდან ამოღება და წაშლა
    if (memcache_get(memcache, key, &inode->metadata))
        return inode;
    else
        return NULL;
}

struct inode* inode_reopen(struct inode* inode)
{
    pthread_mutex_lock(&inode->lock);

    if (inode != NULL) {
        inode->open_cnt++;
    }
    pthread_mutex_unlock(&inode->lock);
    return inode;
}

size_t inode_read_at(struct inode* inode, void* buff, size_t size,
    size_t offset)
{
    pthread_mutex_lock(&inode->lock);

    char* buffer = buff;
    size_t current = offset;
    size_t end = offset + size;
    size_t read = 0;
    while (current < end) {
        size_t current_block = current / INODE_BLOCK_SIZE;
        size_t next_offset = (current_block + 1) * INODE_BLOCK_SIZE;
        char key[30];
        get_key(key, inode->id, current_block);
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
    pthread_mutex_unlock(&inode->lock);
    return read;
}

size_t inode_write_at(struct inode* inode, const void* buff, size_t size,
    size_t offset)
{
    pthread_mutex_lock(&inode->lock);

    const char* buffer = buff;
    size_t current = offset;
    size_t end = offset + size;
    size_t written = 0;
    while (current < end) {
        size_t current_block = current / INODE_BLOCK_SIZE;
        size_t next_offset = (current_block + 1) * INODE_BLOCK_SIZE;
        char key[30];
        get_key(key, inode->id, current_block);
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
    /* Release resources if this was the last opener. */
    if (--inode->open_cnt == 0) {
        list_remove(&inode->elem);

        if (inode->is_deleted) {
            char key[30];
            for (int i = 0; i < inode->metadata.length / INODE_BLOCK_SIZE; i++) {
                get_key(key, inode->id, i);
                memcache_delete(memcache, key);
            }
            get_metadata(key, inode->id);
            memcache_delete(memcache, key);
        }

        free_inode(inode->id);

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
    return inode->metadata.length;
}
bool inode_is_dir(struct inode* inode)
{
    return inode->metadata.is_dir;
}