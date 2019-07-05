#include "memcache.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
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

void test3()
{
    int a[10];
    for (int i = 0; i < 10; i++) {
        a[i] = 12312 - i * 34;
        char key[30];
        sprintf(key, "%d", i);
        memcache_add(memcache, key, &a[i], sizeof(int));
    }

    for (int i = 0; i < 10; i++) {
        char key[30];
        sprintf(key, "%d", i);
        int b;
        memcache_get(memcache, key, &b);
        assert(a[i] == b);
    }

    printf("test 3 passed\n");
}

void test4()
{
    int a[10];
    for (int i = 0; i < 10; i++) {
        a[i] = 12312 - i * 34;
    }
    char key[30];
    sprintf(key, "test%d", 4);
    assert(memcache_add(memcache, key, a, 10 * sizeof(int)));

    int b[10];
    assert(memcache_get(memcache, key, b));

    for (int i = 0; i < 10; i++) {
        assert(a[i] == b[i]);
    }

    printf("test 4 passed\n");
}

int main(int argc, char* argv[])
{
    memcache = memcache_init();
    assert(memcache != NULL);
    assert(memcache_clear(memcache));
    test1();
    test2();
    test3();
    test4();
}