/*
 * Copyright (c) 2022 Institute of Parallel And Distributed Systems (IPADS)
 * ChCore-Lab is licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v1. You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v1 for more details.
 */

#include <stdio.h>
#include <string.h>
#include <chcore/types.h>
#include <chcore/fsm.h>
#include <chcore/tmpfs.h>
#include <chcore/ipc.h>
#include <chcore/internal/raw_syscall.h>
#include <chcore/internal/server_caps.h>
#include <chcore/procm.h>
#include <chcore/fs/defs.h>

typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v)      __builtin_va_end(v)
#define va_arg(v, l)   __builtin_va_arg(v, l)
#define va_copy(d, s)  __builtin_va_copy(d, s)

extern struct ipc_struct *fs_ipc_struct;

#define BUF_SIZE 1024
/* You could add new functions or include headers here.*/
/* LAB 5 TODO BEGIN */
int allocfd()
{
        static int fd = 2;
        fd++;
        return fd;
}

int openfile(const char *filename, FILE *f)
{
        // printf("[chcore][fs]in openfile\n");
        int ret;
        struct ipc_msg *ipc_msg =
                ipc_create_msg(fs_ipc_struct, sizeof(struct fs_request), 0);
        chcore_assert(ipc_msg);
        struct fs_request *fr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        fr->req = FS_REQ_OPEN;
        strcpy(fr->open.pathname, filename);
        fr->open.mode = f->mode;
        fr->open.new_fd = f->fd;
        ret = ipc_call(fs_ipc_struct, ipc_msg);
        ipc_destroy_msg(fs_ipc_struct, ipc_msg);
        return ret;
}

int createfile(const char *filename, FILE *f)
{
        int ret;
        struct ipc_msg *ipc_msg =
                ipc_create_msg(fs_ipc_struct, sizeof(struct fs_request), 0);
        chcore_assert(ipc_msg);
        struct fs_request *fr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        fr->req = FS_REQ_CREAT;
        fr->creat.mode = f->mode;
        strcpy(fr->creat.pathname, filename);
        ret = ipc_call(fs_ipc_struct, ipc_msg);
        ipc_destroy_msg(fs_ipc_struct, ipc_msg);
        return ret;
}
/* LAB 5 TODO END */

FILE *fopen(const char *filename, const char *mode)
{
        // simpilidied mode "r/w"
        /* LAB 5 TODO BEGIN */
        FILE *fp = NULL;
        if (mode[0] == 'r' || mode[0] == 'w') {
                fp = malloc(sizeof(FILE));
                fp->mode = (mode[0] == 'r') ? O_RDONLY : O_WRONLY;
                fp->fd = allocfd();
        } else {
                return fp;
        }
        int ret;
        ret = openfile(filename, fp);
        if (ret < 0) {
                if (fp->mode != O_RDONLY) {
                        // create fp
                        ret = createfile(filename, fp);
                        if (ret < 0)
                                goto fail;
                        ret = openfile(filename, fp);
                        if (ret < 0)
                                goto fail;
                } else {
                        goto fail;
                }
        }
/* LAB 5 TODO END */
        return fp;
fail:
        chcore_bug("failed to open file");
        return NULL;
}

size_t fwrite(const void *src, size_t size, size_t nmemb, FILE *f)
{
        /* LAB 5 TODO BEGIN */
        if (f->mode != O_WRONLY && f->mode != O_RDWR) {
                return -1;
        }
        int ret;
        int len = size * nmemb / sizeof(char);
        struct ipc_msg *ipc_msg =
                ipc_create_msg(fs_ipc_struct, sizeof(struct fs_request) + len, 0);
        chcore_assert(ipc_msg);
        struct fs_request *fr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        memcpy((void *)fr + sizeof(struct fs_request), src, len);
        fr->req = FS_REQ_WRITE;
        fr->write.fd = f->fd;
        fr->write.count = len;
        ret = ipc_call(fs_ipc_struct, ipc_msg);
        if (ret < 0) {
                chcore_bug("ipc write error\n");
                goto destroy_msg;
        }
        // printf("[chcore][fwrite] ret = %d\n", ret);
destroy_msg:
        ipc_destroy_msg(fs_ipc_struct, ipc_msg);
        /* LAB 5 TODO END */
        return ret;
}

size_t fread(void *destv, size_t size, size_t nmemb, FILE *f)
{
        /* LAB 5 TODO BEGIN */
        if (f->mode != O_RDONLY &&f->mode != O_RDWR) {
                return -1;
        }
        int ret;
        int len = size * nmemb / sizeof(char);
        struct ipc_msg *ipc_msg =
                ipc_create_msg(fs_ipc_struct, sizeof(struct fs_request) + len, 0);
        chcore_assert(ipc_msg);
        struct fs_request *fr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        fr->req = FS_REQ_READ;
        fr->read.fd = f->fd;
        fr->read.count = len;
        ret = ipc_call(fs_ipc_struct, ipc_msg);
        if (ret < 0) {
                goto destroy_msg;
        }

        memcpy(destv, ipc_get_msg_data(ipc_msg), ret);
destroy_msg:
        ipc_destroy_msg(fs_ipc_struct, ipc_msg);
        /* LAB 5 TODO END */
        return ret;
}

int fclose(FILE *f)
{
        /* LAB 5 TODO BEGIN */
        int ret;
        struct ipc_msg *ipc_msg =
                ipc_create_msg(fs_ipc_struct, sizeof(struct fs_request), 0);
        chcore_assert(ipc_msg);
        struct fs_request *fr = (struct fs_request *)ipc_get_msg_data(ipc_msg);
        fr->req = FS_REQ_CLOSE;
        fr->close.fd = f->fd;
        ret = ipc_call(fs_ipc_struct, ipc_msg);
        ipc_destroy_msg(fs_ipc_struct, ipc_msg);

        if (ret < 0) {
                return -1;
        }
        /* LAB 5 TODO END */
        return 0;
}

/* Need to support %s and %d. */
int fscanf(FILE *f, const char *fmt, ...)
{
        //fscanf(pFile, "%s %s %d", rbuf, rbuf2, &outdata);
        /* LAB 5 TODO BEGIN */
        va_list va;
        va_start(va, fmt);
        char buf[BUF_SIZE] = {'\0'};
        int buf_ptr = 0;
        int file_sz = fread(buf, sizeof(char), BUF_SIZE, f);
        for (; *fmt != 0 && buf_ptr < file_sz; ++fmt) {
                if (*fmt == '%') {
                        ++fmt;
                        if (*fmt == '\0')
                                break;
                        if (*fmt == '%') {
                                goto out;
                        }
                        // %s
                        if (*fmt == 's') {
                                char *data = va_arg(va, char *);
                                int start = buf_ptr;
                                while (buf[buf_ptr] != ' '
                                       && buf_ptr < file_sz) {
                                        buf_ptr++;
                                }
                                memcpy(data, buf + start, buf_ptr - start);
                                data[buf_ptr - start] = '\0';
                        }
                        // %d
                        if (*fmt == 'd') {
                                int *data = va_arg(va, int *);
                                int num = 0;
                                while (buf[buf_ptr] >= '0'
                                       && buf[buf_ptr] <= '9'
                                       && buf_ptr < file_sz) {
                                        num = num * 10 + buf[buf_ptr] - '0';
                                        buf_ptr++;
                                }
                                *data = num;
                        }
                } else {
                out:
                        buf_ptr++;
                        continue;
                }
        }
        /* LAB 5 TODO END */
        return 0;
}

/* Need to support %s and %d. */
int fprintf(FILE *f, const char *fmt, ...)
{
        /* LAB 5 TODO BEGIN */
        char buf[BUF_SIZE];
        char *ptr = buf;
        int ret;
        va_list va;
        va_start(va, fmt);
        ret = simple_vsprintf(&ptr, fmt, va);
        va_end(va);
        if (ret < 0) {
                return -1;
        }
        ret = fwrite(buf, sizeof(char), ret, f);
        /* LAB 5 TODO END */
        return ret;
}
