#include "memcache.h"
#include "utils.h"
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_BLOCK_SIZE 4196

struct memcache_t {
    int fd;
};

int line_start_index(const char* st, int line)
{

    for (int i = 0;; i++) {
        if (i == 0 || st[i - 1] == '\n') {
            line--;
            if (line == 0)
                return i;
        }
    }
    return -1;
}

struct memcache_t* memcache_init()
{
    struct memcache_t* memcache = malloc(sizeof(struct memcache_t));
    if (memcache == NULL)
        return NULL;

    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    struct in_addr s_addr;
    if (!inet_pton(AF_INET, MEMCACHED_ADDRESS, &s_addr.s_addr)) {
        free(memcache);
        return NULL;
    }

    struct sockaddr_in addr;
    addr.sin_addr = s_addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(MEMCACHED_PORT);
    if (connect(clientfd, (const struct sockaddr*)&addr, sizeof(addr)) == -1) {
        free(memcache);
        return NULL;
    }

    memcache->fd = clientfd;

    return memcache;
}

void memcache_create(struct memcache_t* memcache)
{
    assert(memcache != NULL);
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
    char data[MAX_BLOCK_SIZE];
    int filled = sprintf(data, "get %s\r\n", key);

    if (write(memcache->fd, data, filled) < filled) {
        return false;
    }
    if (read(memcache->fd, data, MAX_BLOCK_SIZE) >= 0) {
        char ret_key[64];
        int ret_exp, ret_size;
        if (sscanf(data, "VALUE %s %d %d\r\n", ret_key, &ret_exp, &ret_size) < 3)
            return false;

        memcpy(buff, data + line_start_index(data, 2), ret_size);
        return true;
    } else {
        return false;
    }
}

bool memcache_add(struct memcache_t* memcache, const char* key,
    const void* value, size_t size)
{
    char data[MAX_BLOCK_SIZE];
    int filled = sprintf(data, "set %s 0 0 %zu\r\n", key, size);
    memcpy(data + filled, value, size);
    filled += size;

    filled += sprintf(data + filled, "\r\n");

    if (write(memcache->fd, data, filled) < filled) {
        return false;
    }
    char result[64];
    bool res = (read(memcache->fd, result, 64) >= 0 && strncmp(result, "STORED", 6) == 0);

    return res;
}

bool memcache_delete(struct memcache_t* memcache, const char* key)
{
    char data[MAX_BLOCK_SIZE];
    int filled = sprintf(data, "delete %s\r\n", key);

    if (write(memcache->fd, data, filled) < filled) {
        return false;
    }

    char result[64];
    return (read(memcache->fd, result, 64) >= 0 && strncmp(result, "DELETED", 7) == 0);
}

void memcache_close(struct memcache_t* memcache)
{
    if (memcache == NULL)
        return;
    close(memcache->fd);
    free(memcache);
}

bool memcache_clear(struct memcache_t* memcache)
{
    char data[MAX_BLOCK_SIZE];
    int filled = sprintf(data, "flush_all\r\n");

    if (write(memcache->fd, data, filled) < filled) {
        return false;
    }

    char result[64];
    return (read(memcache->fd, result, 64) >= 0 && strncmp(result, "OK", 2) == 0);
}