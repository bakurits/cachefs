#include "memcache.h"

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

bool memcache_get(struct memcache_t* memcache, const char* key, char* buff) {}
bool memcache_add(struct memcache_t* memcache, const char* key,
    const void* value, size_t size) {}

void memcache_free(struct memcache_t* memcache)
{
    if (memcache == NULL)
        return;
    close(memcache->fd);
    free(memcache);
}