/********************************************************

   ksHttp.c
    Copyright 2019.10.27 konoar

 ********************************************************/

#include "ksCommon.h"
#include "ksHttp.h"
#include "ksBuffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>

#define KS_HTTP_BUFFER_MAX  1024
#define KS_HTTP_URI_MAX     256
#define KS_HTTP_HOST_MAX    256
#define KS_HTTP_PATH_MAX    1024
#define KS_HTTP_ROOT        "./data"

#define KS_SLEEP(_a_)                           \
    do {                                        \
                                                \
        struct timespec t = {                   \
            0, _a_ * 1000 * 1000                \
        };                                      \
        nanosleep(&t, NULL);                    \
                                                \
    } while(0)

typedef struct _ksSessionStateless
{

    int     acc;
    char    uri     [KS_HTTP_URI_MAX];
    char    host    [KS_HTTP_HOST_MAX];

    enum {

        KS_HTTP_METHOD_UNKNOWN,
        KS_HTTP_METHOD_GET,
        KS_HTTP_METHOD_POST,
        KS_HTTP_METHOD_PUT

    } method;

    enum {

        KS_HTTP_VERSION_UNKNOWN,
        KS_HTTP_VERSION_1_0,
        KS_HTTP_VERSION_1_1,
        KS_HTTP_VERSION_2

    } version;

    enum {

        KS_HTTP_RSTAGE_INIT,
        KS_HTTP_RSTAGE_HEADER,
        KS_HTTP_RSTAGE_BODY,
        KS_HTTP_RSTAGE_FINISH

    } rstage;

    enum {

        KS_HTTP_WSTAGE_INIT,
        KS_HTTP_WSTAGE_HEADER,
        KS_HTTP_WSTAGE_BODY,
        KS_HTTP_WSTAGE_FINISH

    } wstage;

    ksBuffer    bufferin;
    ksBuffer    bufferout;

} ksSessionStateless;

int ksHttpGetWord(const char *data, const int datalen, const int pos, char *buff, const int bufflen)
{

    enum {

        KS_HTTP_GETWORD_START,
        KS_HTTP_GETWORD_WORD,
        KS_HTTP_GETWORD_SPACE

    } state = KS_HTTP_GETWORD_START;

    int cnt = 0, start = 0;

    for (int idx = 0; idx < datalen; idx++) {

        switch (state) {

            case KS_HTTP_GETWORD_START:

                if (data[idx] == 0x0D ||
                    data[idx] == 0x0A) {

                    return KS_NG;

                } else if (data[idx] == 0x20) {

                    state = KS_HTTP_GETWORD_SPACE;

                } else {

                    start = idx;
                    state = KS_HTTP_GETWORD_WORD;

                }
                break;

            case KS_HTTP_GETWORD_WORD:

                if (data[idx] == 0x20 ||
                    data[idx] == 0x0D ||
                    data[idx] == 0x0A) {

                    if (++cnt == pos) {

                        if ((idx -start) >= bufflen) {

                            return KS_NG;

                        }

                        memcpy(buff, data + start, idx - start);
                        buff[idx - start] = 0x00;

                        return KS_OK;

                    }

                    state = KS_HTTP_GETWORD_SPACE;

                }
                break;

            case KS_HTTP_GETWORD_SPACE:

                if (data[idx] == 0x0D ||
                    data[idx] == 0x0A) {

                    return KS_NG;

                } else if (data[idx] != 0x20) {

                    start = idx;
                    state = KS_HTTP_GETWORD_WORD;

                }
                break;

            default:
                return KS_NG;

        }

    }

    return KS_NG;

}

int ksHttpGetMethod(const char *data, const int datalen, int *method)
{

    char buff[32];

    if (KS_OK != ksHttpGetWord(data, datalen, 1, buff, sizeof(buff))) {

        return KS_NG;

    }

    if (0 == strcmp(buff, "GET")) {

        *method = KS_HTTP_METHOD_GET;
        return KS_OK;

    }

    if (0 == strcmp(buff, "POST")) {

        *method = KS_HTTP_METHOD_POST;
        return KS_OK;

    }

    if (0 == strcmp(buff, "PUT")) {

        *method = KS_HTTP_METHOD_PUT;
        return KS_OK;

    }

    return KS_NG;

}

int ksHttpGetURI(const char *data, const int datalen, char *uri, const int urilen)
{

    enum {

        KS_HTTP_GETURI_PRE,
        KS_HTTP_GETURI_DATA,
        KS_HTTP_GETURI_POST,

    } state = KS_HTTP_GETURI_PRE;

    char buff[KS_HTTP_URI_MAX];
    int pos = 0;

    if (urilen <= 0) {

        return KS_NG;

    }

    if (KS_OK != ksHttpGetWord(data, datalen, 2, buff, sizeof(buff))) {

        return KS_NG;

    }

    for (int cur = 0; buff[cur] != 0x00; cur++) {

        switch (state) {

            case KS_HTTP_GETURI_PRE:

                if (0x20 != buff[cur]) {

                    if (0x2F != buff[cur]) {

                        if (pos < urilen)   uri[pos++] = buff[cur];
                        else                return KS_NG;

                    }

                    state = KS_HTTP_GETURI_DATA;

                }
                break;

            case KS_HTTP_GETURI_DATA:

                if (0x20 != buff[cur]) {

                    if (pos < urilen)   uri[pos++] = buff[cur];
                    else                return KS_NG;

                } else {

                    state = KS_HTTP_GETURI_POST;

                }
                break;

            default:
                break;

        }

    }

    if (pos < urilen) {

        buff[pos] = 0x00;

    } else {

        return KS_NG;

    }

    return KS_OK;

}

int ksHttpGetVersion(const char *data, const int datalen, int *version)
{

    char buff[16];

    if (KS_OK != ksHttpGetWord(data, datalen, 3, buff, sizeof(buff))) {

        return KS_NG;

    }

    if (0 == strcmp(buff, "HTTP/1.1")) {

        *version = KS_HTTP_VERSION_1_1;
        return KS_OK;

    }

    if (0 == strcmp(buff, "HTTP/2")) {

        *version = KS_HTTP_VERSION_2;
        return KS_OK;

    }

    if (0 == strcmp(buff, "HTTP/1.0")) {

        *version = KS_HTTP_VERSION_1_0;
        return KS_OK;

    }

    return KS_NG;

}

int ksHttpFindHeaderField(const char *data, const int datalen, const char *key, char *buff, const int bufflen)
{

    int base, cur, pos = -1;

    for (base = 0; base < datalen; ) {

        for (cur = 0; cur < (datalen - base); cur++) {

            if (0x00 == key[cur]) {

                pos = base + cur;
                goto LFINISH;

            } else if (data[base + cur] == key[cur]) {

                continue;

            }

            break;

        }

        for (; base < datalen; base++) {

            if (0x0A == data[base]) {

                base++;
                break;

            }

        }

    }

LFINISH:

    if (0 <= pos) {

        for (base = 0, cur = pos; base < (bufflen - 1) && cur < datalen; base++, cur++) {

            if (0x0D == data[cur] ||
                0x0A == data[cur]) {

                break;

            }

            buff[base] = data[cur];

        }

        buff[base] = 0x00;

        return KS_OK;

    }

    return KS_NG;

}

int ksHttpParseHeader(ksSessionStateless *s, const char *buff, const int bufflen)
{

    int cnt = 0, end = 0, ret = KS_NG;
    char value[256];

    for (int idx = 0; idx < bufflen; idx++) {

        if (0x0A == buff[idx]) {

            cnt++;

        } else if (0x0D != buff[idx]) {

            cnt = 0;

        }

        if (cnt == 2) {

            end = idx;
            ret = KS_OK;
            break;

        }

    }

    if (KS_OK != ret) {

        return ret;

    }

    if (KS_OK != (ret = ksHttpGetMethod(buff, end, (int*)&s->method))) {

        return ret;

    }

    if (KS_OK != (ret = ksHttpGetURI(buff, end, s->uri, KS_HTTP_URI_MAX))) {

        return ret;

    }

    if (KS_OK != (ret = ksHttpGetVersion(buff, end, (int*)&s->version))) {

        if (KS_HTTP_VERSION_1_1 != s->version) {

            return KS_NG;

        }

    }

    if (KS_OK != (ret = ksHttpFindHeaderField(
            buff, end, "Host:", s->host, KS_HTTP_HOST_MAX))) {

        return ret;

    }

    return KS_OK;

}

int ksHttpReadBody(ksSessionStateless *s, const char *buff, const int bufflen)
{

    /* todo */

    return KS_OK;

}

int ksHttpThreadIORead(ksSessionStateless *s)
{

    char buff[KS_HTTP_BUFFER_MAX];
    int recvlen = 0, bufflen = 0;

    if (s->rstage == KS_HTTP_RSTAGE_FINISH) {

        return KS_END;

    }

    recvlen = recv(s->acc, buff + bufflen, KS_HTTP_BUFFER_MAX - bufflen, 0);

    if (0 >= recvlen) {

        return KS_NODATA;

    } else {

        bufflen += recvlen;

    }

    switch (s->rstage) {

        case KS_HTTP_RSTAGE_INIT:

            s->rstage = KS_HTTP_RSTAGE_HEADER;

        case KS_HTTP_RSTAGE_HEADER:

            if (KS_OK == ksHttpParseHeader(s, buff, bufflen)) {

                s->rstage = KS_HTTP_RSTAGE_BODY;

            }
            break;

        case KS_HTTP_RSTAGE_BODY:

            if (KS_OK == ksHttpReadBody(s, buff, bufflen)) {

                s->rstage = KS_HTTP_RSTAGE_FINISH;

            }
            break;

    }

    return KS_OK;

}

int ksHttpThreadIOWrite(ksSessionStateless *s)
{

    char buff[KS_HTTP_BUFFER_MAX];
    int  buffsize = 0, ret;

    if (s->wstage == KS_HTTP_WSTAGE_FINISH) {

        return KS_END;

    }

    switch (s->wstage) {

        case KS_HTTP_WSTAGE_INIT:

            if (s->rstage >= KS_HTTP_RSTAGE_BODY) {

                s->wstage = KS_HTTP_WSTAGE_HEADER;

            } else {

                KS_SLEEP(100);

            }
            break;

        case KS_HTTP_WSTAGE_HEADER:

            sprintf(buff, "HTTP/1.1 200 OK\n\n");
            send(s->acc, buff, strlen(buff), 0);

            s->wstage = KS_HTTP_WSTAGE_BODY;
            break;

        case KS_HTTP_WSTAGE_BODY:

            buffsize = KS_BUFFER_BLOCK_MAX;

            ret = ksBufferRead(s->bufferout, buff, &buffsize);

            if (ret == KS_OK && buffsize > 0) {

                send(s->acc, buff, buffsize, 0);

            } else if (ret == KS_END) {

                if (buffsize > 0) {

                    send(s->acc, buff, buffsize, 0);

                }

                s->wstage = KS_HTTP_WSTAGE_FINISH;

            }
            break;

        case KS_HTTP_WSTAGE_FINISH:

            return KS_END;

    }

    return KS_OK;

}

void* ksHttpThreadIO(void *param)
{

    ksSessionStateless *s = (ksSessionStateless*)param;

    while (1) {

        (void) ksHttpThreadIORead(s);

        if (KS_END == ksHttpThreadIOWrite(s)) {

            break;

        }

    }

    return 0;

}

int ksHttpDoGet(ksSessionStateless *s)
{

    FILE *fp = NULL;
    char filename[KS_HTTP_PATH_MAX], buff[16];
    int siz;

    sprintf(filename, "%s/%s", KS_HTTP_ROOT, s->uri);

    if (NULL == (fp = fopen(filename, "r"))) {

        /* todo 404 not found */

        return KS_NG;

    }

    while (siz = fread(buff, 1, KS_BUFFER_BLOCK_MAX, fp)) {

        while (KS_OK != ksBufferWrite(s->bufferout, buff, &siz, 0)) {

            KS_SLEEP(100);

        }

    }

    fclose(fp);

    siz = 0;

    while (KS_OK != ksBufferWrite(s->bufferout, NULL, &siz, KS_BUFFER_FLAG_END)) {

        KS_SLEEP(100);

    }

    return KS_OK;

}

void* ksHttpThreadTask(void *param)
{

    ksSessionStateless *s = (ksSessionStateless*)param;

    while (KS_HTTP_RSTAGE_HEADER >= s->rstage) {

        KS_SLEEP(100);

    }

    switch (s->method) {

        case KS_HTTP_METHOD_GET:

            ksHttpDoGet(s);
            break;

        default:
            break;

    }

    return 0;

}

int ksHttpAcceptAndRespond(int acc)
{

    ksSessionStateless *s = NULL;

    pthread_t threadio, threadtask;
    int stateio, statetask, ret = KS_OK;

    s = malloc(sizeof(ksSessionStateless));

    do {

        s->acc      = acc;
        s->rstage   = KS_HTTP_RSTAGE_INIT;
        s->wstage   = KS_HTTP_WSTAGE_INIT;
        s->method   = KS_HTTP_METHOD_UNKNOWN;
        s->version  = KS_HTTP_VERSION_UNKNOWN;

        if (KS_OK != ksBufferInit(&s->bufferin)) {

            s->bufferin = NULL;
            ret = KS_NG;
            break;

        }

        if (KS_OK != ksBufferInit(&s->bufferout)) {

            s->bufferout = NULL;
            ret = KS_NG;
            break;
        }

        stateio     = KS_FALSE;
        statetask   = KS_FALSE;

        if (0 != pthread_create( 

                &threadio,
                NULL,
                ksHttpThreadIO,
                s

            )) {

            ret = KS_NG;
            break;

        }

        stateio = KS_TRUE;

        if (0 != pthread_create(

                &threadtask,
                NULL,
                ksHttpThreadTask,
                s

            )) {

            ret = KS_NG;
            break;

        }

        statetask = KS_TRUE;

    } while(0);

    if (statetask) {

        pthread_join(threadtask, NULL);

    }

    if (stateio) {

        pthread_join(threadio, NULL);

    }

    if (s) {

        if (s->bufferin) {

            ksBufferUninit(&s->bufferin);
            s->bufferin = NULL;

        }

        if (s->bufferout) {

            ksBufferUninit(&s->bufferout);
            s->bufferout = NULL;

        }

        free(s);
        s = NULL;

    }

    return ret;

}

