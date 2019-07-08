#include "freemap.h"
#include "memcache.h"
#include <assert.h>
#include <string.h>

#define FREEMAP_BLOCK_SIZE 1024

struct memcache* memcache;
int current_block;
int number_of_blocks;

static void
get_key(char* key, int ind)
{
    sprintf(key, "FREEMAP#%d", ind);
}

bool init_freemap(struct memcache_t* mem, int inode_cnt)
{
    assert(inode_cnt % FREEMAP_BLOCK_SIZE == 0);
    number_of_blocks = inode_cnt / FREEMAP_BLOCK_SIZE;
    bool success = true;
    memcache = mem;
    current_block = 0;
    for (int i = 0; i < number_of_blocks; i++) {
        char key[30];
        get_key(key, i);
        char value[FREEMAP_BLOCK_SIZE];
        memset(value, 0, FREEMAP_BLOCK_SIZE);
        if (!memcache_add(memcache, key, value, FREEMAP_BLOCK_SIZE))
            success = false;
    }
    if (success)
        return true;

    for (int i = 0; i < number_of_blocks; i++) {
        char key[30];
        get_key(key, i);
        if (!memcache_delete(memcache, key))
            break;
    }
    return false;
}

int get_free_inode()
{
    int start = current_block;
    while (true) {
        char key[30];
        get_key(key, current_block);
        char value[FREEMAP_BLOCK_SIZE];
        if (!memcache_get(memcache, key, value))
            return -1;

        for (int i = 0; i < FREEMAP_BLOCK_SIZE; i++) {
            if (value[i] == 0) {
                value[i] = 1;
                if (!memcache_add(memcache, key, value, FREEMAP_BLOCK_SIZE))
                    return -1;
                return current_block * FREEMAP_BLOCK_SIZE + i;
            }
        }

        current_block = (current_block + 1) % number_of_blocks;
        if (current_block == start)
            return -1;
    }
}

bool free_inode(int inode)
{
    char key[30];
    get_key(key, inode / FREEMAP_BLOCK_SIZE);
    char value[FREEMAP_BLOCK_SIZE];

    if (!memcache_get(memcache, key, value))
        return false;

    value[inode % FREEMAP_BLOCK_SIZE] = 0;

    return memcache_add(memcache, key, value, FREEMAP_BLOCK_SIZE);
}
