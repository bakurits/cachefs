#ifndef INODE_H
#define INODE_H

#include "memcache.h"
#include <stdbool.h>
#include <stdio.h>

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
int inode_create(int inode_id, bool is_dir);

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
 * Function : inode_create
 * ----------------------------------------
 * Creates inode
 * 
 * inode_id     : newly allocated id for inode 
 * is_dir       : indicates if inode is directory
 * 
 * Returns  : id of newly created inode
 */
int inode_create(int inode_id, bool is_dir);

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
    size_t offset);

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
    size_t offset);

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
 * 
 * 
 * inode    : inode 
 * 
 * Returns  : length of inode
 */
bool inode_is_dir(struct inode* inode);
#endif