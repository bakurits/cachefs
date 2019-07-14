
#ifndef INODE_H
#define INODE_H

#define FUSE_USE_VERSION 31

#include "list.h"
#include "memcache.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

#define DIR_MAGIC 123130234

/**
 * Stores inode metada on disk
 */
struct inode_disk_metadata {
    size_t length; // Stores length of inode in bytes
    bool is_dir; // Stores if inode is directory */
    __mode_t mode;
    __uid_t uid;
    __gid_t gid;
    size_t link_cnt;
    size_t xattrs_length;
};

/**
 * Inode stored in memory 
 */
struct inode {
    int id;
    int open_cnt;
    bool is_deleted;
    struct list_elem elem;
    int magic;
    pthread_mutex_t lock;
    struct inode_disk_metadata metadata;
};

/**
 * Function : init_inodes
 * ----------------------------------------
 * Initializes inodes variables
 * 
 * mem      : memcache data object 
 *
 */
void init_inodes(struct memcache_t* mem);

/**
 * Function : inode_create
 * ----------------------------------------
 * Initializes inodes variables
 * 
 * inode_id : memcache data object 
 * is_dir   : memcache data object 
 *
 * Returns  :
 */
bool inode_create(int inode_id, bool is_dir, __gid_t gid, __uid_t uid, __mode_t mode);

/**
 * Function : inode_open
 * ----------------------------------------
 * Initializes inode for later use
 * 
 * id       : id of inode
 * 
 * Returns  : inode structure for later operations
 */
struct inode* inode_open(int id);

/**
 * Function : inode_reopen
 * ----------------------------------------
 * Repens inode
 * 
 * inode    : inode to reopen
 *
 * Returns  : inode structure for later operations
 */
struct inode* inode_reopen(struct inode* inode);

/**
 * Function : inode_read_at
 * ----------------------------------------
 * Reads data from inode
 * 
 * inode    : inode to read
 * buff     : buffer where read data should be copied 
 * size     : size of data to read
 * offset   : offset in inode from where read starts 
 * 
 * Returns  : number of read bytes
 */
size_t inode_read_at(struct inode* inode, void* buff, size_t size,
    size_t offset, bool xattrs);

/**
 * Function : inode_write_at
 * ----------------------------------------
 * Writes data in inode 
 * 
 * inode    : inode to write
 * buff     : buffer from where data should be copied to inode
 * size     : size of data to write
 * offset   : offset in inode from where write starts 
 * 
 * Returns  : number of written bytes
 */
size_t inode_write_at(struct inode* inode, const void* buff, size_t size,
    size_t offset, bool xattrs);

/**
 * Function : inode_close
 * ----------------------------------------
 * Writes data in inode 
 * 
 * inode    : inode to write
 * 
 */
void inode_close(struct inode* inode);

/**
 * Function : inode_remove
 * ----------------------------------------
 * Writes data in inode 
 * 
 * inode    : inode to write
 * 
 */
void inode_remove(struct inode* inode);

/**
 * Function : inode_length
 * ----------------------------------------
 * Finds length of inode
 * 
 * inode    : inode 
 * 
 * Returns  : length of inode
 */
size_t inode_length(struct inode* inode);

/**
 * Function : inode_is_dir
 * ----------------------------------------
 * Checks if inode is directory 
 * 
 * inode    : inode 
 * 
 * Returns  : true if inode is directory
 */
bool inode_is_dir(struct inode* inode);

/**
 * Function : inode_path_register
 * ----------------------------------------
 * Checks if inode is directory 
 * 
 * inode    : inode 
 * 
 * Returns  : true if inode is directory
 */
bool inode_path_register(const char* path, int inode_id);

/**
 * Function : inode_get_from_path
 * ----------------------------------------
 * Checks if inode is directory 
 * 
 * inode    : inode 
 * 
 * Returns  : true if inode is directory
 */
struct inode* inode_get_from_path(const char* path);

/**
 * Function : inode_path_delete
 * ----------------------------------------
 * Checks if inode is directory 
 * 
 * inode    : inode 
 * 
 * Returns  : true if inode is directory
 */
bool inode_path_delete(const char* path);

bool is_inode(struct inode* inode);

size_t inode_xattrs_length(struct inode* inode);

#endif