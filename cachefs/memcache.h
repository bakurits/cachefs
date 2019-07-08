
#ifndef MEMCACHE_H
#define MEMCACHE_H

#include <stdbool.h>

#define MEMCACHED_PORT 11211
#define MEMCACHED_ADDRESS "127.0.0.1"

#define CONSISTENCY_KEY "bakurits-xoiquxtt"
#define CONSISTENCY_VALUE 0xfff123

/**
 * Function : memcache_init
 * ----------------------------------------
 * 
 * Initializes memcache_t object for use
 * 
 * Returns : pointer to memcache data or NULL if error ocurred
 */
struct memcache_t* memcache_init();

/**
 * Function : memcache_create
 * ----------------------------------------
 * 
 * Creates Memcached's metadata in memory
 * 
 * memcache : memcache object
 */
void memcache_create(struct memcache_t* memcache);

/**
 * Function : memcache_is_consistent
 * ----------------------------------------
 * 
 * Chechs if Memcached has valid kay for filesistem
 * 
 * memcache : memcache object
 * 
 * Returns  : true if memcache is consistent and false otherwise 
 */
bool memcache_is_consistent(struct memcache_t* memcache);

/**
 * Function : memcache_get
 * ----------------------------------------
 *  
 * Gets Memcached's records
 * 
 * memcache : memcache object
 * key      : key of record 
 * buff     : pointer to memory block where data should be copied 
 * 
 * Returns  : true if key exists and false otherwise
 */
bool memcache_get(struct memcache_t* memcache, const char* key, void* buff);

/**
 * Function : memcache_add
 * ----------------------------------------
 * 
 * Adds data in Memcached
 * 
 * memcache : memcache object
 * key      : key of record 
 * buff     : pointer of memory block from where data should be copied  
 * size     : size of data
 * 
 * Returns  : true if data successfully added and false otherwise
 */
bool memcache_add(struct memcache_t* memcache, const char* key,
    const void* buff, size_t size);

/**
 * Function : memcache_delete
 * ----------------------------------------
 *  
 * Deletes data with given key from Memcached
 * 
 * memcache : memcache object
 * key      : key of record
 * 
 * Returns  : true if key exists and false otherwise
 */
bool memcache_delete(struct memcache_t* memcache, const char* key);

/**
 * Function : memcache_clear
 * ----------------------------------------
 *  
 * clears all data in Memcached
 * 
 * memcache : memcache object
 * 
 * Returns  : true if whole data successfully removed or false otherwise
 */
bool memcache_clear(struct memcache_t* memcache);

/**
 * Function : memcache_close
 * ----------------------------------------
 *  
 * Closes memcache object 
 * 
 * memcache : memcache object
 */
void memcache_close(struct memcache_t* memcache);

#endif