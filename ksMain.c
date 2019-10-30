/********************************************************

   ksMain.c
    Copyright 2019.10.27 konoar

 ********************************************************/

#include "ksCommon.h"
#include "ksHttp.h"
#include "ksBuffer.h"

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int ksNonblock(int soc)
{

    int val;

    if (-1 == (val = fcntl(soc, F_GETFL, 0))) {

        return KS_NG;

    }

    fcntl(soc, F_SETFL, val | O_NONBLOCK);

    return KS_OK;

}

int ksListen(int *soc)
{

    const char *port = "80";

    struct addrinfo hints, *res = NULL;
    int opt, ret = KS_OK;
    char nbuff[NI_MAXHOST], sbuff[NI_MAXSERV];

    if (soc)    *soc = -1;
    else        return KS_NG;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family     = AF_INET;
    hints.ai_flags      = AI_PASSIVE;
    hints.ai_socktype   = SOCK_STREAM;

    if (0 != getaddrinfo(NULL, port, &hints, &res)) {

        return KS_NG;

    }

    do {

        if (0 != getnameinfo(res->ai_addr, res->ai_addrlen,
            nbuff, sizeof(nbuff), sbuff, sizeof(sbuff), NI_NUMERICHOST | NI_NUMERICSERV)) {

            ret = KS_NG;
            break;

        }

        if (-1 == (*soc = socket(res->ai_family, res->ai_socktype, res->ai_protocol))) {

            ret = KS_NG;
            break;

        }

        opt = 1;

        if (-1 == setsockopt(*soc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {

            ret = KS_NG;
            break;

        }

        if (-1 == bind(*soc, res->ai_addr, res->ai_addrlen)) {

            ret = KS_NG;
            break;

        }

        if (-1 == listen(*soc, SOMAXCONN)) {

            ret = KS_NG;
            break;

        }

    } while(0);

    if (KS_OK != ret && -1 != *soc) {

        close(*soc);
        *soc = -1;

    }
    
    if (res) {

        freeaddrinfo(res);

    }

    return ret;

}

int main(int argc, const char* argv[])
{

    struct sockaddr_storage from;
    socklen_t len;

    int soc, acc;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

    if (KS_OK != ksListen(&soc)) {

        return KS_NG;

    }

    while(1) {

        len = (socklen_t)sizeof(from);

        if (-1 == (acc = accept(soc, (struct sockaddr*)&from, &len))) {
            continue;
        }

        if (KS_OK != ksNonblock(acc)) {
            break;
        }

        if (KS_OK != ksHttpRespond(acc)) {
            break;
        }

        close(acc);

    }

    close(soc);

    return 0;

}

