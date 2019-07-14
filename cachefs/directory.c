

#include "directory.h"
#include "list.h"
#include "utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dir {
    struct inode* inode;
    size_t pos;
};

struct dir_entry {
    int inode_id;
    char name[NAME_MAX + 1];
    bool is_deleted;
};

static bool lookup(const struct dir* dir, const char* name,
    struct dir_entry* entry_pointer, size_t* offset_pointer)
{
    struct dir_entry entry;
    size_t ofs;
    assert(dir != NULL);
    assert(name != NULL);
    for (ofs = 0; inode_read_at(dir->inode, &entry, sizeof(struct dir_entry), ofs, false) == sizeof(struct dir_entry);
         ofs += sizeof(struct dir_entry)) {
        if (entry.is_deleted)
            continue;
        if (!strcmp(name, entry.name)) {
            if (entry_pointer != NULL)
                *entry_pointer = entry;
            if (offset_pointer != NULL)
                *offset_pointer = ofs;
            return true;
        }
    }
    return false;
}

static void inode_cut(struct inode* inode, size_t start, size_t end)
{
}

bool dir_create(int inode_id, __gid_t gid, __uid_t uid, __mode_t mode)
{
    return inode_create(inode_id, true, gid, uid, mode);
}

struct dir* dir_open(struct inode* inode)
{
    struct dir* dir = malloc(sizeof *dir);

    if (inode != NULL && dir != NULL) {
        assert(inode_is_dir(inode));
        dir->pos = 0;
        dir->inode = inode;
        return dir;
    } else {
        inode_close(inode);
        free(dir);
        return NULL;
    }
}
struct dir* dir_reopen(struct dir* dir)
{
    assert(dir != NULL);
    assert(dir->inode != NULL);
    return dir_open(inode_reopen(dir->inode));
}

struct dir* dir_open_root()
{
    return dir_open(inode_open(ROOT_INODE_ID));
}

void dir_close(struct dir* dir)
{
    if (dir != NULL) {
        inode_close(dir->inode);
        free(dir);
    }
}

struct inode* dir_get_inode(struct dir* dir)
{
    return dir->inode;
}

bool dir_lookup(const struct dir* dir, const char* name, struct inode** inode)
{
    struct dir_entry e;

    assert(dir != NULL);
    assert(name != NULL);

    if (lookup(dir, name, &e, NULL))
        *inode = inode_open(e.inode_id);
    else
        *inode = NULL;

    return *inode != NULL;
}

bool dir_add(struct dir* dir, const char* name, int inode_id)
{
    struct dir_entry e;
    size_t ofs;
    bool success = false;

    assert(dir != NULL);
    assert(name != NULL);
    if (*name == '\0' || strlen(name) > NAME_MAX) {
        return false;
    }
    if (lookup(dir, name, NULL, NULL)) {
        return false;
    }

    memcpy(e.name, name, strlen(name) + 1);
    e.inode_id = inode_id;
    e.is_deleted = false;
    return inode_write_at(dir->inode, &e, sizeof e, inode_length(dir->inode), false) == sizeof e;
}

bool dir_remove(struct dir* dir, const char* name)
{
    struct dir_entry e;
    struct inode* inode = NULL;
    bool success = false;
    size_t ofs;

    assert(dir != NULL);
    assert(name != NULL);

    if (!lookup(dir, name, &e, &ofs))
        goto done;

    inode = inode_open(e.inode_id);
    if (inode == NULL)
        goto done;
    e.is_deleted = true;
    if (inode_write_at(dir->inode, &e, sizeof e, ofs, false) != sizeof e)
        goto done;

    inode_remove(inode);
    success = true;

done:
    inode_close(inode);
    return success;
}

bool dir_readdir(struct dir* dir, char name[NAME_MAX + 1])
{
    struct dir_entry entry;

    if (dir->pos < 2 * sizeof(struct dir_entry))
        dir->pos = 2 * sizeof(struct dir_entry);

    while (inode_read_at(dir->inode, &entry, sizeof entry, dir->pos, false) == sizeof entry) {
        dir->pos += sizeof entry;
        if (!entry.is_deleted) {
            memcpy(name, entry.name, NAME_MAX + 1);
            return true;
        }
    }

    return false;
}

bool dir_is_empty(struct dir* dir)
{
    char name[NAME_MAX + 1];
    int cnt = 0;
    while (dir_readdir(dir, name)) {
        cnt++;
    }
    return cnt == 0;
}
