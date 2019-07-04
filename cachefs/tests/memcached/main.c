/*
  FUSE: Filesystem in Userspace
  Copiyright (C) 2001-2007  Miklos Szered <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#define FUSE_USE_VERSION 31

#include "memcache.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
struct memcache_t* memcache;
void test1()
{
    int a = 4;
    char b[20];
    char c[20];
    strcpy(b, "tsutskhashvili");
    for (int i = 1; i <= 10; i++) {
        memcache_add(memcache, "bakuri", b, strlen(b));
        memcache_get(memcache, "bakuri", &c);
        assert(strncmp(b, c, strlen(b)) == 0);
    }

    printf("test 1 passed\n");
}

void test2()
{
    int a = 4;
    for (int i = 1; i <= 10; i++) {
        memcache_add(memcache, "bakuri", &i, sizeof(int));
        memcache_get(memcache, "bakuri", &a);
        assert(a == i);
    }

    printf("test 2 passed\n");
}
int main(int argc, char* argv[])
{
    memcache = memcache_init();
    test1();
    test2();
}
