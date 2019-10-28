/********************************************************

   ksbuffer.c
    Copyright 2019.10.27 konoar

 ********************************************************/

#include "ksCommon.h"
#include "ksBuffer.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define KS_BUFFER_POS_NONE     -1
#define KS_BUFFER_STREAM_MAX    4

typedef struct _ksBufferStream
{

    short   start;
    short   end;
    short   write;
    short   reserved;

    struct
    {

        short   size;
        short   flag;

        char    data[KS_BUFFER_BLOCK_MAX];

    } stream[KS_BUFFER_STREAM_MAX];

    pthread_mutex_t mutex;

} ksBufferStream;

static int ksBufferLock(ksBufferStream *s)
{

    if (0 != pthread_mutex_lock(&s->mutex)) {

        return KS_NG;

    }

    return KS_OK;

}

static int ksBufferUnlock(ksBufferStream *s)
{

    if (0 != pthread_mutex_unlock(&s->mutex)) {

        return KS_NG;

    }

    return KS_OK;

}

int ksBufferInit(ksBuffer *buff)
{

    ksBufferStream *s = NULL;
    pthread_mutexattr_t attr;

    int ret = KS_OK;

    if (!buff || *buff) {

        return KS_NG;

    }

    s = (ksBufferStream*)malloc(sizeof(ksBufferStream));

    do {

        s->start    = 0;
        s->end      = 0;
        s->write    = KS_BUFFER_POS_NONE;
        s->reserved = 0;

        for (int idx = 0; idx < KS_BUFFER_STREAM_MAX; idx++) {

            s->stream[idx].size     = 0;
            s->stream[idx].flag     = 0;

        }

        if (0 != pthread_mutexattr_init(&attr)) {

            ret = KS_NG;
            break;

        }

        if (0 != pthread_mutex_init(&s->mutex, &attr)) {

            ret = KS_NG;
            break;

        }

    } while(0);

    if (KS_OK == ret) {

        *buff = (ksBuffer)s;

    } else {

        free((void*)s);
        s = NULL;

    }

    return ret;

}

int ksBufferUninit(ksBuffer *buff)
{

    ksBufferStream *s = NULL;

    int ret = KS_OK;

    if (!buff || !*buff) {

        return KS_NG;

    }

    s = (ksBufferStream*)*buff;

    if (0 != pthread_mutex_destroy(&s->mutex)) {

        ret = KS_NG;

    }

    free(*buff);
    *buff = NULL;

    return ret;

}

int ksBufferWrite(ksBuffer *buff, const char *data, int *size, short flag)
{

    ksBufferStream *s = NULL;

    int ret = KS_OK;

    if (!buff || !*buff || !size || KS_BUFFER_BLOCK_MAX < *size) {

        return KS_NG;

    }

    s = (ksBufferStream*)buff;

    if (KS_OK != ksBufferLock(s)) {

        return ret;

    }

    do {

        if (KS_BUFFER_POS_NONE != s->write) {

            if (!(KS_BUFFER_FLAG_END & s->stream[s->write].flag) &&
                 (KS_BUFFER_BLOCK_MAX - s->stream[s->write].size) >= *size) {

                break;

            }

        }

        if (s->start == s->end) {

            if (KS_BUFFER_STREAM_MAX == (s->end + 1)) {

                s->write    = s->start;
                s->end      = 0;

            } else {

                s->write    = s->start;
                s->end      = s->start + 1;

            }

        } else if (s->start < s->end) {

            if (KS_BUFFER_STREAM_MAX == (s->end + 1)) {

                if (0 == s->start) {

                    ret = KS_NG;
                    break;

                }

                s->write    = s->end;
                s->end      = 0;

            } else {

                s->write    = s->end;
                s->end      = s->end + 1;

            }

        } else {

            if ((s->end + 1) == s->start) {

                ret = KS_NG;
                break;

            }

            s->write        = s->end;
            s->end          = s->end + 1;

        }

    } while(0);

    if (KS_OK == ret && KS_BUFFER_POS_NONE != s->write) {

        if (0 < *size) {

            memcpy(s->stream[s->write].data + s->stream[s->write].size, data, *size);
            s->stream[s->write].size += *size;

        }

        s->stream[s->write].flag |= flag;

    }

    if (KS_OK != ksBufferUnlock(s)) {

        return KS_NG;

    }

    return ret;

}

int ksBufferRead(ksBuffer *buff, char *data, int *size)
{

    ksBufferStream *s = NULL;

    int ret = KS_OK;

    if (!buff || !*buff || !size) {

        return KS_NG;

    }

    s = (ksBufferStream*)buff;

    if (KS_OK != ksBufferLock(s)) {

        return ret;

    }

    do {

        if (s->start == s->end) {

            ret = KS_NODATA;
            break;

        }

        if (*size < s->stream[s->start].size) {

            ret = KS_BADBUFFER;
            break;

        }

        if (0 < s->stream[s->start].size) {

            memcpy(data, s->stream[s->start].data, s->stream[s->start].size);
            *size = s->stream[s->start].size;

        } else {

            *size = 0;

        }

        if (KS_BUFFER_FLAG_END & s->stream[s->start].flag) {

            ret = KS_END;

        } else {

            ret = KS_OK;

        }

        s->stream[s->start].size = 0;
        s->stream[s->start].flag = 0;

        if (s->write == s->start) {

            s->write = KS_BUFFER_POS_NONE;

        }

        if (s->start == KS_BUFFER_STREAM_MAX - 1) {

            s->start = 0;

        } else {

            s->start++;

        }

    } while(0);

    if (KS_OK != ksBufferUnlock(s)) {

        return KS_NG;

    }

    return ret;

}

