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

void test5()
{
    int a[10];
    for (int i = 0; i < 10; i++) {
        a[i] = 12312 - i * 34;
        char key[30];
        sprintf(key, "test5%d", i);
        memcache_add(memcache, key, &a[i], sizeof(int));
    }
    for (int i = 0; i < 10; i++) {
        char key[30];
        sprintf(key, "test5%d", i);
        if (i % 2 == 0)
            memcache_delete(memcache, key);
    }

    for (int i = 0; i < 10; i++) {
        char key[30];

        sprintf(key, "test5%d", i);
        int b;
        if (i % 2 == 0) {
            assert(memcache_get(memcache, key, &b) == false);
        } else {
            memcache_get(memcache, key, &b);
            assert(a[i] == b);
        }
    }

    printf("test 5 passed\n");
}

struct inode_disk_metadata {
    size_t length;
    bool is_dir;
    bool si;
    __mode_t mode;
    __uid_t uid;
    __gid_t gid;
    size_t link_cnt;
};

void test6()
{
    struct inode_disk_metadata a[10];
    for (int i = 0; i < 10; i++) {

        a[i].is_dir = 1;
        a[i].mode = 0;
        a[i].link_cnt = 1;
        a[i].length = 0;
        a[i].uid = 0;
        a[i].gid = 0;
        a[i].si = 37;
        char key[30];
        sprintf(key, "test6%d", i);
        memcache_add(memcache, key, &a[i], sizeof(struct inode_disk_metadata));
    }
    for (int i = 0; i < 1; i++) {
        char key[30];

        sprintf(key, "test6%d", i);
        struct inode_disk_metadata b;
        memcache_get(memcache, key, &b);
        assert(a[i].length == b.length);
        assert(a[i].is_dir == b.is_dir);
        assert(a[i].mode == b.mode);
        assert(a[i].uid == b.uid);
        assert(a[i].gid == b.gid);
        assert(a[i].link_cnt == b.link_cnt);
    }

    printf("test 6 passed\n");
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
    test5();
    test6();
}