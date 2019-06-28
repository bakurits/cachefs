#include "memcache.h"
#include <assert.h>
struct memcache_t {
    int fd;
};

struct memcache_t* memcache_init()
{
    struct memcache_t* memcache = malloc(sizeof(struct memcache_t));
    if (memcache == NULL)
        return NULL;

    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    struct in_addr s_addr;
    if (!inet_pton(AF_INET, MEMCACHED_ADDRESS, &s_addr.s_addr))
        return NULL;

    struct sockaddr_in addr;
    addr.sin_addr = s_addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(MEMCACHED_PORT);
    if (!connect(clientfd, (const struct sockaddr*)&addr, sizeof(addr)))
        return NULL;

    memcache->fd = clientfd;

    return memcache;
}

void memcache_create(struct memcache_t* memcache)
{
    assert(memcache == NULL);
    int value = CONSISTENCY_VALUE;
    memcache_add(memcache, CONSISTENCY_KEY, (void*)&value, sizeof(int));
}

bool memcache_is_consistent(struct memcache_t* memcache)
{
    assert(memcache != NULL);
    int value;
    return (memcache_get(memcache, CONSISTENCY_KEY, (void*)&value) && value == CONSISTENCY_VALUE);
}

bool memcache_get(struct memcache_t* memcache, const char* key, void* buff) {}
bool memcache_add(struct memcache_t* memcache, const char* key,
    const void* value, size_t size) {}

void memcache_close(struct memcache_t* memcache)
{
    if (memcache == NULL)
        return;
    close(memcache->fd);
    free(memcache);
}

void memcache_clear(struct memcache_t* memcache)
{
}