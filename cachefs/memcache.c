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
#define CONNECTION_COUNT 1

struct memcache_t {
    int fds[CONNECTION_COUNT];
    bool in_use[CONNECTION_COUNT];
    pthread_mutex_t lock;
};

static int get_fd(struct memcache_t* memcache)
{
    int res = -1;
    pthread_mutex_lock(&memcache->lock);
    for (int i = 0; i < CONNECTION_COUNT; i++) {
        if (!memcache->in_use[i]) {
            memcache->in_use[i] = true;
            res = memcache->fds[i];
            break;
        }
    }
    pthread_mutex_unlock(&memcache->lock);
    printf("getting fd #%d\n", res);
    return res;
}

static void release_fd(struct memcache_t* memcache, int fd)
{
    printf("releasing fd #%d\n", fd);
    if (fd < 0)
        return;

    pthread_mutex_lock(&memcache->lock);
    for (int i = 0; i < CONNECTION_COUNT; i++) {
        if (memcache->in_use[i] && memcache->fds[i] == fd) {
            memcache->in_use[i] = false;
            break;
        }
    }
    pthread_mutex_unlock(&memcache->lock);
}

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
    for (int i = 0; i < CONNECTION_COUNT; i++) {
        int clientfd = socket(AF_INET, SOCK_STREAM, 0);
        struct in_addr s_addr;
        if (!inet_pton(AF_INET, MEMCACHED_ADDRESS, &s_addr.s_addr)) {
            for (int j = 0; j < i; j++)
                close(memcache->fds[j]);
            free(memcache);
            return NULL;
        }

        struct sockaddr_in addr;
        addr.sin_addr = s_addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(MEMCACHED_PORT);
        if (connect(clientfd, (const struct sockaddr*)&addr, sizeof(addr)) == -1) {
            for (int j = 0; j < i; j++)
                close(memcache->fds[j]);
            free(memcache);
            return NULL;
        }

        memcache->fds[i] = clientfd;
        memcache->in_use[i] = false;
    }
    pthread_mutex_init(&memcache->lock, NULL);
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
    int fd = get_fd(memcache);
    if (fd < 0)
        return false;
    char data[MAX_BLOCK_SIZE];
    int filled = sprintf(data, "get %s\r\n", key);

    if (write(fd, data, filled) < filled) {
        release_fd(memcache, fd);
        return false;
    }
    if (read(fd, data, MAX_BLOCK_SIZE) >= 0) {
        char ret_key[64];
        int ret_exp, ret_size;
        if (sscanf(data, "VALUE %s %d %d\r\n", ret_key, &ret_exp, &ret_size) < 3) {
            release_fd(memcache, fd);
            return false;
        }

        memcpy(buff, data + line_start_index(data, 2), ret_size);
        release_fd(memcache, fd);
        return true;
    } else {
        release_fd(memcache, fd);
        return false;
    }
}

bool memcache_add(struct memcache_t* memcache, const char* key,
    const void* value, size_t size)
{

    int fd = get_fd(memcache);
    if (fd < 0)
        return false;

    char data[MAX_BLOCK_SIZE];
    int filled = sprintf(data, "set %s 0 0 %zu\r\n", key, size);
    memcpy(data + filled, value, size);
    filled += size;

    filled += sprintf(data + filled, "\r\n");

    if (write(fd, data, filled) < filled) {
        release_fd(memcache, fd);
        return false;
    }
    char result[64];
    bool res = (read(fd, result, 64) >= 0 && strncmp(result, "STORED", 6) == 0);
    release_fd(memcache, fd);
    return res;
}

bool memcache_delete(struct memcache_t* memcache, const char* key)
{
    int fd = get_fd(memcache);
    if (fd < 0)
        return false;

    char data[MAX_BLOCK_SIZE];
    int filled = sprintf(data, "delete %s\r\n", key);

    if (write(fd, data, filled) < filled) {
        release_fd(memcache, fd);
        return false;
    }

    char result[64];
    bool res = (read(fd, result, 64) >= 0 && strncmp(result, "DELETED", 7) == 0);
    release_fd(memcache, fd);
    return res;
}

void memcache_close(struct memcache_t* memcache)
{
    if (memcache == NULL)
        return;
    pthread_mutex_lock(&memcache->lock);
    for (int j = 0; j < CONNECTION_COUNT; j++)
        close(memcache->fds[j]);
    pthread_mutex_unlock(&memcache->lock);
    pthread_mutex_destroy(&memcache->lock);
    free(memcache);
}

bool memcache_clear(struct memcache_t* memcache)
{
    int fd = get_fd(memcache);
    if (fd < 0)
        return false;
    char data[MAX_BLOCK_SIZE];
    int filled = sprintf(data, "flush_all\r\n");

    if (write(fd, data, filled) < filled) {
        release_fd(memcache, fd);
        return false;
    }

    char result[64];
    bool res = (read(fd, result, 64) >= 0 && strncmp(result, "OK", 2) == 0);
    release_fd(memcache, fd);
    return res;
}