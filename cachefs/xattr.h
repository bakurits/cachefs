#ifndef XATTRS_H
#define XATTRS_H

#include "inode.h"
#include <stdbool.h>
#include <stddef.h>

#define KEY_MAX 20
#define VALUE_MAX 42
#define ROOT_INODE_ID 0

struct attr_entry {
    char key[KEY_MAX];
    char value[VALUE_MAX];
    bool is_deleted;
    char value_size;
};

bool xattr_add(struct inode* inode, const char* key, const char* value, size_t length);
int xattr_get(struct inode* inode, const char* key, char* value, size_t length);
int xattr_remove(struct inode* inode, const char* key);
int xattr_list(struct inode* inode, char* buffer, size_t size);
bool xattr_remove_all(struct inode* inode);

#endif
