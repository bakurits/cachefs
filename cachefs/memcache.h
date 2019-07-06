
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

/**
 * Initializes memcache_t object for use
 */
struct memcache_t* memcache_init();

/**
 * Creates Memcached's metadata in memory
 */
void memcache_create(struct memcache_t*);

/**
 * Chechs if Memcached has valid kay for filesistem
 */
bool memcache_is_consistent(struct memcache_t*);

/**
 * Gets Memcached's records
 */
bool memcache_get(struct memcache_t*, const char*, void*);

/**
 * Adds data in Memcached
 */
bool memcache_add(struct memcache_t*, const char*,
    const void*, size_t);

/**
 * Deletes data with given key from Memcached
 */
bool memcache_delete(struct memcache_t*, const char*);

/**
 * clears all data in Memcached
 */
bool memcache_clear(struct memcache_t*);

/**
 * removes memcache_t object 
 */
void memcache_close(struct memcache_t*);

#endif