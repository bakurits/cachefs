
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

#define CONSISTENCY_KEY "bakurits-xoiquxtt"
#define CONSISTENCY_VALUE 0xfff123

struct memcache_t* memcache_init();
void memcache_create(struct memcache_t*);

bool memcache_is_consistent(struct memcache_t*);

bool memcache_get(struct memcache_t* memcache, const char* key, void* buff);
bool memcache_add(struct memcache_t* memcache, const char* key,
    const void* value, size_t size);

void memcache_clear(struct memcache_t*);
void memcache_close(struct memcache_t*);

#endif