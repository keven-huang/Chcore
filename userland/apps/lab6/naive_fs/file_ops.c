#include <string.h>
#include <stdio.h>
#include <chcore/memory.h>

#include "file_ops.h"
#include "block_layer.h"

#define FILEBLK_SIZE 100

int atoi(char *str)
{
        int ret = 0;
        while (*str) {
                ret = ret * 10 + *str - '0';
                str++;
        }
        return ret;
}

char *itos(int num)
{
        char *ret = (char *)malloc(10);
        int i = 0;
        while (num) {
                ret[i++] = num % 10 + '0';
                num /= 10;
        }
        int start = 0;
        int end = i - 1;
        while (start < end) {
                char temp = ret[start];
                ret[start] = ret[end];
                ret[end] = temp;
                start++;
                end--;
        }
        ret[i] = '\0';
        return ret;
}
// bitformat
static int BLOCK_NUM = 3;
static int first_init = 1;
// helper function
int init_bitmap()
{
        char bitmap[BLOCK_SIZE];
        memset(bitmap, 0, BLOCK_SIZE);
        bitmap[0] = 0x7;
        sd_bwrite(0, bitmap);
        return 0;
}

int init()
{
        if (first_init) {
                init_bitmap();
                first_init = 0;
        }
}

int alloc_blk()
{
        do {
                BLOCK_NUM++;
        } while (is_blk_used(BLOCK_NUM));
        set_blk(BLOCK_NUM, 1);
        return BLOCK_NUM;
}

bool is_blk_used(int blknum)
{
        // check if the block is used
        char bitmap[BLOCK_SIZE];
        sd_bread(0, bitmap);
        int byte = blknum / 8;
        int bit = blknum % 8;
        return (bitmap[byte] >> bit) & 1;
}

void set_blk(int blknum, bool used)
{
        char bitmap[BLOCK_SIZE];
        sd_bread(0, bitmap);
        int byte = blknum / 8;
        int bit = blknum % 8;
        if (used) {
                bitmap[byte] |= (1 << bit);
        } else {
                bitmap[byte] &= ~(1 << bit);
        }
        sd_bwrite(0, bitmap);
}

// dentry format --  name:bcknum1,bcknum2,;name:bcknum1,bcknum2,;
int get_file2blk(const char *name, int blks[], int *num)
{
        int ret;
        int offset;
        int blkno;
        char buffer[BLOCK_SIZE * 2];
        ret = sd_bread(1, buffer);
        ret = sd_bread(2, buffer + BLOCK_SIZE);
        if (ret < 0)
                return ret;
        char *file_start = buffer;
findfile:;
        char *ptr = strstr(file_start, name);
        if (ptr) {
                if (ptr != buffer) {
                        if (*(ptr - 1) != ';') {
                                file_start = ptr + strlen(name);
                                goto findfile;
                        }
                }
                char *ptr2 = strstr(ptr, ":");
                ptr2++;
                offset = 0;
                blkno = 0;
                char blknum[10];
                while (*ptr2 != ';') {
                        while (*ptr2 != ',') {
                                blknum[offset++] = *ptr2;
                                ptr2++;
                        }
                        blknum[offset] = '\0';
                        blks[blkno++] = atoi(blknum);
                        offset = 0;
                        ptr2++;
                }
                *num = blkno;
                return 0;
        } else {
                return -1;
        }
}

int write_file2blk(const char *name, int blks[], int num)
{
        int ret;
        char buffer[BLOCK_SIZE * 2];
        ret = sd_bread(1, buffer);
        ret = sd_bread(2, buffer + BLOCK_SIZE);
        if (ret < 0)
                return ret;
        char fileblk[FILEBLK_SIZE];
        memset(fileblk, 0, FILEBLK_SIZE);
        strcat(fileblk, name);
        strcat(fileblk, ":");
        for (int i = 0; i < num; ++i) {
                char *in = itos(blks[i]);
                strcat(fileblk, in);
                free(in);
                strcat(fileblk, ",");
        }
        strcat(fileblk, ";");
        strcat(buffer, fileblk);
        ret = sd_bwrite(1, buffer);
        ret = sd_bwrite(2, buffer + BLOCK_SIZE);
        // printf("[debug]blk2file buffer = %s\n", buffer);
        if (ret < 0)
                return ret;
        return 0;
}

int del_file2blk(const char *name)
{
        int ret;
        int offset;
        int blkno;
        char buffer[BLOCK_SIZE * 2];
        ret = sd_bread(1, buffer);
        ret = sd_bread(2, buffer + BLOCK_SIZE);
        if (ret < 0)
                return ret;
        char *file_start = buffer;
findfile:;
        char *ptr = strstr(file_start, name);
        if (ptr) {
                if (ptr != buffer) {
                        if (*(ptr - 1) != ';') {
                                file_start = ptr + strlen(name);
                                goto findfile;
                        }
                }
                char *ptr2 = strstr(ptr, ";");
                ptr2++;
                strcpy(ptr, ptr2);
        } else {
                return -1;
        }
        ret = sd_bwrite(1, buffer);
        ret = sd_bwrite(2, buffer + BLOCK_SIZE);
        if (ret < 0)
                return ret;
        return 0;
}

int naive_fs_access(const char *name)
{
        /* LAB 6 TODO BEGIN */
        /* BLANK BEGIN */
        init();
        int ret;
        int blks[10];
        int num;
        ret = get_file2blk(name, blks, &num);
        if (ret < 0) {
                return ret;
        }
        /* BLANK END */
        /* LAB 6 TODO END */
        return 0;
}

int naive_fs_creat(const char *name)
{
        /* LAB 6 TODO BEGIN */
        /* BLANK BEGIN */
        init();
        int ret;
        int blks[10];
        int num;
        ret = get_file2blk(name, blks, &num);
        if (ret >= 0)
                return -1;
        int blkno = alloc_blk();
        blks[0] = blkno;
        num = 1;
        ret = write_file2blk(name, blks, num);
        /* BLANK END */
        /* LAB 6 TODO END */
        return 0;
}

int naive_fs_pread(const char *name, int offset, int size, char *buffer)
{
        /* LAB 6 TODO BEGIN */
        /* BLANK BEGIN */
        int ret;
        int blks[10];
        int num;
        ret = get_file2blk(name, blks, &num);
        if (ret < 0)
                return ret;
        int blkno, blkoff;
        int cur_off = offset;
        size_t to_read;
        while (size > 0) {
                blkno = cur_off / BLOCK_SIZE;
                blkoff = cur_off % BLOCK_SIZE;
                to_read = (BLOCK_SIZE - blkoff > size) ? size :
                                                         BLOCK_SIZE - blkoff;
                char blk[BLOCK_SIZE];
                if (blkno >= num)
                        break;
                ret = sd_bread(blks[blkno], blk);
                if (ret < 0)
                        return ret;
                memcpy(buffer, blk + blkoff, to_read);
                buffer += to_read;
                size -= to_read;
                cur_off += to_read;
        }
        /* BLANK END */
        /* LAB 6 TODO END */
        return cur_off - offset;
}

int naive_fs_pwrite(const char *name, int offset, int size, const char *buffer)
{
        /* LAB 6 TODO BEGIN */
        /* BLANK BEGIN */
        int ret;
        int blks[10];
        int num;
        ret = get_file2blk(name, blks, &num);
        if (ret < 0)
                return ret;
        int blkno, blkoff;
        int cur_off = offset;
        size_t to_write;
        while (size > 0) {
                blkno = cur_off / BLOCK_SIZE;
                blkoff = cur_off % BLOCK_SIZE;
                to_write = (BLOCK_SIZE - blkoff > size) ? size :
                                                          BLOCK_SIZE - blkoff;
                char blk[BLOCK_SIZE];
                if (blkno >= num) {
                        int newblk = alloc_blk();
                        blks[num++] = newblk;
                }
                ret = sd_bread(blks[blkno], blk);
                if (ret < 0)
                        return ret;
                memcpy(blk + blkoff, buffer, to_write);
                ret = sd_bwrite(blks[blkno], blk);
                if (ret < 0)
                        return ret;
                buffer += to_write;
                size -= to_write;
                cur_off += to_write;
        }
        ret = del_file2blk(name);
        if (ret < 0) {
                return ret;
        }
        ret = write_file2blk(name, blks, num);
        if (ret < 0) {
                return ret;
        }
        /* BLANK END */
        /* LAB 6 TODO END */
        return cur_off - offset;
}

int naive_fs_unlink(const char *name)
{
        /* LAB 6 TODO BEGIN */
        /* BLANK BEGIN */
        int ret;
        ret = del_file2blk(name);
        /* BLANK END */
        /* LAB 6 TODO END */
        return ret;
}
