#include "utils.h"
#include <string.h>
int get_next_part(char part[NAME_MAX + 1], const char** srcp)
{
    const char* src = *srcp;
    char* dst = part;
    /* Skip leading slashes. If it’s all slashes, we’re done. */
    while (*src == '/')
        src++;
    if (*src == '\0')
        return 0;

    /* Copy up to NAME_MAX character from SRC to DST. Add null terminator. */
    while (*src != '/' && *src != '\0') {
        if (dst < part + NAME_MAX)
            *dst++ = *src;
        else
            return -1;
        src++;
    }
    *dst = '\0';

    /* Advance source pointer. */
    *srcp = src;
    return 1;
}

int split_file_path(const char* whole_path, char* dir, char* file)
{
    dir[0] = file[0] = '\0';
    int n = strlen(whole_path);
    int i;
    for (i = n - 1; i >= 0; i--) {
        if (whole_path[i] == '/') {
            memcpy(dir, whole_path, i + 1);
            memcpy(file, &whole_path[i + 1], n - i);
            return true;
        }
    }
    if (n > NAME_MAX)
        return false;

    memcpy(file, whole_path, n);
    return true;
}