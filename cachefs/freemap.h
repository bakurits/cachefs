#ifndef FREEMAP_H
#define FREEMAP_H

#include "memcache.h"
#include <stdbool.h>
#include <stdio.h>

/**
 * Function : init_freemap
 * ----------------------------------------
 * Initializes freemap 
 * This function being called when file system is newly formated 
 * 
 * mem      : memcache data object 
 * inode_cnt: maximum number of inodes possible
 * 
 * Returns  : true if successfully created
 */
bool init_freemap(struct memcache_t* mem, size_t inode_cnt);

/**
 * Function : get_free_inode
 * ----------------------------------------
 * 
 * Finds free inode id
 * 
 * Returns  : id of free inode or -1 if there isn't any
 */
int get_free_inode();

/**
 * Function : free_inode
 * ----------------------------------------
 * 
 * Frees inode and makes it usable 
 * 
 * inode    : id of inode
 * 
 * Returns  : true if inode is freed successfully or false otherwise
 */
bool free_inode(int inode);

#endif