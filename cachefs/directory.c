

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
};

/* Extracts a file name part from *SRCP into PART, and updates *SRCP so that the
next call will return the next file name part. Returns 1 if successful, 0 at
end of string, -1 for a too-long file name part. */

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool lookup(const struct dir* dir, const char* name,
    struct dir_entry* entry_pointer, size_t* offset_pointer)
{
    struct dir_entry entry;
    size_t ofs;
    assert(dir != NULL);
    assert(name != NULL);
    for (ofs = 0; inode_read_at(dir->inode, &entry, sizeof(struct dir_entry), ofs) == sizeof(struct dir_entry);
         ofs += sizeof(struct dir_entry)) {
        if (strcmp(name, entry.name)) {
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

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
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

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir* dir_open_root(void)
{
    return dir_open(inode_open(1));
}

/* Opens the directory for given path and returns a directory for it.
   Return true if successful, false on failure. */
struct dir* dir_open_path(struct dir* cwd, char* path)
{
    if (path == NULL)
        return NULL;
    struct dir* cur;
    if (strlen(path) > 0 && path[0] == '/') {
        cur = dir_open_root();
    } else {
        if (cwd != NULL) {
            cur = dir_reopen(cwd);
        } else
            cur = dir_open_root();
    }
    const char* path_left = path;
    while (true) {
        char cur_file[NAME_MAX + 1];
        int res = get_next_part(cur_file, &path_left);
        if (res == -1) {
            dir_close(cur);
            return NULL;
        }
        if (res == 0)
            break;
        struct inode* inode = NULL;
        if (!dir_lookup(cur, cur_file, &inode)) {
            dir_close(cur);
            return NULL;
        }
        struct dir* next = dir_open(inode);
        if (next == NULL) {
            dir_close(cur);
            return NULL;
        }
        dir_close(cur);
        cur = next;
    }
    return cur;
}

struct inode* inode_from_path(const char* path)
{
    char dir_path[strlen(path)];
    char file_name[NAME_MAX + 1];

    if (!split_file_path(path, dir_path, file_name))
        return NULL;

    struct dir* prec = dir_open_path(NULL, dir_path);
    if (prec == NULL)
        return NULL;

    struct inode* inode = NULL;
    if (dir_lookup(prec, file_name, &inode))
        return inode;
    else
        return NULL;
}

/* Destroys DIR and frees associated resources. */
void dir_close(struct dir* dir)
{
    if (dir != NULL) {
        inode_close(dir->inode);
        free(dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode* dir_get_inode(struct dir* dir)
{
    return dir->inode;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
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

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool dir_add(struct dir* dir, const char* name, int inode_id)
{
    struct dir_entry e;
    size_t ofs;
    bool success = false;

    assert(dir != NULL);
    assert(name != NULL);

    /* Check NAME for validity. */
    if (*name == '\0' || strlen(name) > NAME_MAX)
        return false;

    /* Check that NAME is not in use. */
    if (lookup(dir, name, NULL, NULL))
        goto done;

    memcpy(e.name, name, sizeof e.name);
    e.inode_id = inode_id;

    success = inode_write_at(dir->inode, &e, sizeof e, inode_length(dir->inode) == sizeof e);

done:
    return success;
}

bool dir_remove(struct dir* dir, const char* name)
{
    struct dir_entry e;
    struct inode* inode = NULL;
    bool success = false;
    size_t ofs;

    assert(dir != NULL);
    assert(name != NULL);

    /* Find directory entry. */
    if (!lookup(dir, name, &e, &ofs))
        goto done;

    /* Open inode. */
    inode = inode_open(e.inode_id);
    if (inode == NULL)
        goto done;

    inode_cut(dir->inode, ofs, ofs + sizeof e);
    if (inode_write_at(dir->inode, &e, sizeof e, ofs) != sizeof e)
        goto done;

    /* Remove inode. */
    inode_remove(inode);
    success = true;

done:
    inode_close(inode);
    return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool dir_readdir(struct dir* dir, char name[NAME_MAX + 1])
{
    struct dir_entry entry;

    if (dir->pos < 2 * sizeof(struct dir_entry))
        dir->pos = 2 * sizeof(struct dir_entry);

    if (inode_read_at(dir->inode, &entry, sizeof entry, dir->pos) == sizeof entry) {
        memcpy(name, entry.name, NAME_MAX + 1);
        dir->pos += sizeof entry;
        return true;
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