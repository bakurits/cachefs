#ifndef UTILS_H
#define UTILS_H

#include "directory.h"

int get_next_part(char part[NAME_MAX + 1], const char** srcp);

int split_file_path(const char* whole_path, char* dir, char* file);

void DumpHex(const void* data, size_t size);

#endif