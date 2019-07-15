#include "xattr.h"
#include "utils.h"
#include <assert.h>
#include <string.h>

static bool lookup(struct inode* inode, const char* name,
    struct attr_entry* entry_pointer, size_t* offset_pointer)
{
    struct attr_entry entry;
    size_t ofs;
    assert(inode != NULL);
    assert(name != NULL);
    for (ofs = 0; inode_read_at(inode, &entry, sizeof(struct attr_entry), ofs, true) == sizeof(struct attr_entry);
         ofs += sizeof(struct attr_entry)) {
        if (entry.is_deleted)
            continue;
        if (!strcmp(name, entry.key)) {
            if (entry_pointer != NULL)
                *entry_pointer = entry;
            if (offset_pointer != NULL)
                *offset_pointer = ofs;
            return true;
        }
    }
    return false;
}

bool xattr_add(struct inode* inode, const char* key, const char* value, size_t length)
{
    struct attr_entry e;
    size_t ofs;
    bool success = false;

    assert(key != NULL);
    assert(inode != NULL);
    if (*key == '\0' || strlen(key) > KEY_MAX || length > VALUE_MAX) {
        return false;
    }

    memcpy(e.key, key, strlen(key) + 1);
    memcpy(e.value, value, length);
    e.is_deleted = false;
    e.value_size = length;
    size_t offset = inode_xattrs_length(inode);
    lookup(inode, key, NULL, &offset);
    return inode_write_at(inode, &e, sizeof e, offset, true) == sizeof e;
}
int xattr_get(struct inode* inode, const char* key, char* value, size_t size)
{
    struct attr_entry e;

    assert(inode != NULL);
    assert(key != NULL);

    if (lookup(inode, key, &e, NULL)) {
        if (e.value_size + 1 <= size) {
            strncpy(value, e.value, e.value_size);
        }
        return e.value_size;
    } else {
        return -1;
    }
}
int xattr_remove(struct inode* inode, const char* key)
{
    struct attr_entry e;
    bool success = false;
    size_t ofs;

    assert(inode != NULL);
    assert(key != NULL);

    if (!lookup(inode, key, &e, &ofs))
        return -1;

    e.is_deleted = true;
    return (inode_write_at(inode, &e, sizeof e, ofs, true) == sizeof e);
}
int xattr_list(struct inode* inode, char* buffer, size_t size)
{
    struct attr_entry entry;
    size_t pos = 0;
    size_t buf_pos = 0;
    bzero(buffer, size);
    while (inode_read_at(inode, &entry, sizeof entry, pos, true) == sizeof entry) {
        pos += sizeof entry;
        if (!entry.is_deleted) {
            size_t new_pos = buf_pos + strlen(entry.key) + entry.value_size + 2;
            if (new_pos < size) {
                strncpy(buffer + buf_pos, entry.key, strlen(entry.key));
                strncpy(buffer + buf_pos + strlen(entry.key) + 1, entry.value, entry.value_size);
                DumpHex(buffer, size);
            }
            buf_pos = new_pos;
        }
    }
    return buf_pos;
}

bool xattr_remove_all(struct inode* inode)
{
}
