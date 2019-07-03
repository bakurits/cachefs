#include "memcache.h"
#include <assert.h>
#include <string.h>

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
    if (connect(clientfd, (const struct sockaddr*)&addr, sizeof(addr)) == -1)
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

bool memcache_get(struct memcache_t* memcache, const char* key, void* buff)
{
    char data[2048];
    int header_length = sprintf(data, "get %s\n", key);

    if (write(memcache->fd, data, header_length) < header_length) {
        return false;
    }
    if (read(memcache->fd, data, 2048)) {
        char ret_key[64];
        int ret_exp, ret_size;
        int hd = sscanf(data, "VALUE %s %d %d\n", ret_key, &ret_exp, &ret_size);
        memcpy(buff, data + hd, ret_size);
        return true;
    }
}

bool memcache_add(struct memcache_t* memcache, const char* key,
    const void* value, size_t size)
{
    char data[2048];
    int header_length = sprintf(data, "set %s 0 0 %zu\n", key, size);
    memcpy(data + header_length, value, size);

    int a = write(memcache->fd, data, header_length + size);

    if (a < header_length + size) {

        return false;
    }
    char result[64];
    return 1;
    (read(memcache->fd, result, 64) && strcmp(result, "STORED") == 0);
}

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