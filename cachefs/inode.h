#ifndef INODE_H
#define INODE_H

#include <stdbool.h>
#include <stdio.h>

void init_inodes(struct memcache_t* mem);

struct inode* inode_open(int id);

struct inode* inode_reopen(struct inode* inode);

size_t inode_read_at(struct inode* inode, void* buffer_, size_t size,
    size_t offset);

size_t inode_write_at(struct inode* inode, const void* buffer, size_t size,
    size_t offset);

#endif