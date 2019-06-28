
#ifndef MEMCACHE_H
#define MEMCACHE_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MEMCACHED_PORT 11211
#define MEMCACHED_ADDRESS "127.0.0.1"

struct memcache_t* memcache_init();
bool memcache_get(struct memcache_t* memcache, const char* key, char* buff);
bool memcache_add(struct memcache_t* memcache, const char* key,
    const void* value, size_t size);

void memcache_free(struct memcache_t*);

#endif