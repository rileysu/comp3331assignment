#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "rtp.h"
#include "packet.h"
#include "list.h"
#include "timer.h"
#include "logger.h"

size_t mssRTP;
size_t mpsRTP;
int gammaRTP;
unsigned int windowsizeRTP;

static void awaitCloseSocketDefRTP(Socket s, int recvfin);

static void updateTimeout(clock_t init, clock_t post, Socket s){
    clock_t diff = post - init;

    double estRTT = 0.875 * s->estRTT + 0.125 * diff;
    if (estRTT < 0)
        s->estRTT = 100 * CLOCKS_PER_SEC / 1000;
    else
        s->estRTT = (clock_t)estRTT;

    double devRTT = 0.75 * s->devRTT + 0.25 * (diff - s->estRTT);
    if (devRTT < 0)
        s->devRTT = 50 * CLOCKS_PER_SEC / 1000;
    else
        s->devRTT = (clock_t)devRTT;

    double timeout = s->estRTT + gammaRTP*s->devRTT;
    if (timeout < 0 || timeout > 10000000)
        timeout = 1000000;
    s->timeout = (clock_t)timeout;

    //printf("samp:%ld est:%lf dev:%lf timeout:%lf\n", diff, estRTT, devRTT, timeout);
}

void errorRTP(char *errstr, int errcode) {
    fprintf(stderr, "Error occured!\n");
    fprintf(stderr, "Message: %s\n", errstr);
    fprintf(stderr, "Code: %d\n", errcode);
    fprintf(stderr, "Errno: %d\n", errno);
    exit(errcode);
}

Socket initSocketRTP(sa_family_t safamily, size_t mss, size_t mws, int gamma) {
    mssRTP = mss;
    mpsRTP = mssRTP + sizeof(struct Header_t);
    gammaRTP = gamma;
    windowsizeRTP = (unsigned int)mws / mss;

    Socket s = malloc(sizeof(struct Socket_t));

    if ((s->fd = socket(safamily, SOCK_DGRAM, 0)) < 0) {
        errorRTP("Could not init socket", ERROR_INIT);
    }

    memset(&s->localaddr, 0, sizeof(struct sockaddr_in));
    memset(&s->peeraddr, 0, sizeof(struct sockaddr_in));

    s->timeout = 1500000;
    s->estRTT = 500 * CLOCKS_PER_SEC / 1000;
    s->devRTT = 250 * CLOCKS_PER_SEC / 1000;
    s->connected = 0;
    s->pld = NULL;

    s->logger = initLogger();

    fcntl(s->fd, F_SETFL, O_NONBLOCK);

    return s;
}

void freeSocketRTP(Socket s) {
    freeLogger(s->logger);
    close(s->fd);
    free(s);
}

// AF_INET INADDR_ANY htons(PORT)
void bindSocketRTP(sa_family_t safamily, in_port_t localport, in_addr_t localaddr, Socket s) {
    s->localaddr.sin_family = safamily;
    s->localaddr.sin_port = localport;
    s->localaddr.sin_addr.s_addr = localaddr;

    //Bind udp for default input address when using s->fd with recv
    if (bind(s->fd, (struct sockaddr *) &(s->localaddr), sizeof(s->localaddr)) == -1) {
        errorRTP("Error binding socket", ERROR_INIT);
    }
}

void connectSocketRTP(sa_family_t safamily, in_port_t peerport, in_addr_t peeraddr, Socket s) {
    s->peeraddr.sin_family = safamily;
    s->peeraddr.sin_port = peerport;
    s->peeraddr.sin_addr.s_addr = peeraddr;

    //Implicit bind on sent packet

    //Init
    char buf[mpsRTP];
    size_t bufsize;
    int recvvalid = 0;

    Packet psend = initPacketRTP(0, 0, SYN, NULL, 0);

    Timer t = initTimerRTP();
    setTimerRTP(s->timeout, t);

    //SYN
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    memset(&addr, 0, sizeof(addr));


    while (!recvvalid) {
        setTimerRTP(s->timeout, t);
        resetTimerRTP(t);
        sendto(s->fd, psend->head, psend->size, 0, (struct sockaddr *) &s->peeraddr, sizeof(struct sockaddr_in));
        logTimeLogger(LOGGERTIME_SND, clock(), psend, s->logger);
        s->logger->segs++;

        if (checkTimerRTP(t))
            updateTimeout(t->init, clock(), s); //Drop timeout if its slow
        while (!checkTimerRTP(t) && !recvvalid) {
            if (recvfrom(s->fd, &buf, sizeof(struct Header_t), MSG_PEEK, (struct sockaddr *) &addr, &len) == sizeof(struct Header_t)) {
                bufsize = readDataSizeRTP(&buf[0]);
                if (bufsize > mssRTP)
                    errorRTP("Packet that is too large was received", ERROR_MISC);
                while (recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), MSG_PEEK) != bufsize + sizeof(struct Header_t));
                recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), 0);

                Packet precv = readPacketRTP(buf);
                updateTimeout(t->init, clock(), s);
                if (precv == NULL) {
                    printf("Packet was not valid!\n");
                } else {
                    logTimeLogger(LOGGERTIME_RCV, clock(), precv, s->logger);
                    s->logger->segr++;
                    if (precv->head->flags == (SYN | ACK) && precv->head->ack == psend->head->ack) {
                        recvvalid = 1;
                    }
                    freePacketRTP(precv);
                }

                memset(&buf, 0, sizeof(buf));

            }
        }
    }

    printf("%s\n", inet_ntoa(addr.sin_addr));

    freePacketRTP(psend);

    //ACK
    recvvalid = 1;
    //setTimerRTP(s->timeout * 2, t);
    psend = initPacketRTP(0, 0, ACK, NULL, 0);

    while (recvvalid) {
        setTimerRTP(s->timeout, t);
        resetTimerRTP(t);
        sendto(s->fd, psend->head, psend->size, 0, (struct sockaddr *) &s->peeraddr, sizeof(struct sockaddr_in));
        logTimeLogger(LOGGERTIME_SND, clock(), psend, s->logger);
        s->logger->segs++;
        recvvalid = 0;

        if (checkTimerRTP(t))
            updateTimeout(t->init, clock(), s); //Drop timeout if its slow
        while (!checkTimerRTP(t) && !recvvalid) {
            if (recvfrom(s->fd, &buf, sizeof(struct Header_t), MSG_PEEK, (struct sockaddr *) &addr, &len) == sizeof(struct Header_t)) {
                bufsize = readDataSizeRTP(&buf[0]);
                if (bufsize > mssRTP)
                    errorRTP("Packet that is too large was received", ERROR_MISC);
                while (recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), MSG_PEEK) != bufsize + sizeof(struct Header_t));
                recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), 0);

                Packet precv = readPacketRTP(buf);
                updateTimeout(t->init, clock(), s);
                if (precv == NULL)
                    printf("Packet was not valid!\n");
                else {
                    logTimeLogger(LOGGERTIME_RCV, clock(), precv, s->logger);
                    s->logger->segr++;
                    freePacketRTP(precv);
                    recvvalid = 1;
                }

                memset(&buf, 0, sizeof(buf));
            }
        }
    }

    freePacketRTP(psend);

    s->connected = 1;

    freeTimerRTP(t);

}

void acceptSocketRTP(Socket s) {
    char buf[mpsRTP];
    size_t bufsize;
    int recvvalid = 0;

    Timer t = initTimerRTP();
    setTimerRTP(s->timeout, t);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    memset(&addr, 0, sizeof(addr));

    //RECV SYN

    while (!recvvalid) {
        if (recvfrom(s->fd, &buf, sizeof(struct Header_t), MSG_PEEK, (struct sockaddr *) &addr, &len) == sizeof(struct Header_t)) {
            bufsize = readDataSizeRTP(&buf[0]);
            if (bufsize > mssRTP)
                errorRTP("Packet that is too large was received", ERROR_MISC);
            while (recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), MSG_PEEK) != bufsize + sizeof(struct Header_t));
            recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), 0);

            Packet precv = readPacketRTP(buf);
            if (precv == NULL)
                printf("Packet was not valid!\n");
            else {
                logTimeLogger(LOGGERTIME_RCV, clock(), precv, s->logger);
                s->logger->segr++;
                if (precv->head->flags == SYN) {
                    recvvalid = 1;
                }
                freePacketRTP(precv);
            }

            memset(&buf, 0, sizeof(buf));

        }
    }

    printf("%s\n", inet_ntoa(addr.sin_addr));
    s->peeraddr.sin_family = addr.sin_family;
    s->peeraddr.sin_port = addr.sin_port;
    s->peeraddr.sin_addr.s_addr = addr.sin_addr.s_addr;

    recvvalid = 0;
    Packet psend = initPacketRTP(0, 0, SYN | ACK, NULL, 0);

    while (!recvvalid) {
        setTimerRTP(s->timeout, t);
        resetTimerRTP(t);
        sendto(s->fd, psend->head, psend->size, 0, (struct sockaddr *) &s->peeraddr, sizeof(struct sockaddr_in));
        logTimeLogger(LOGGERTIME_SND, clock(), psend, s->logger);
        s->logger->segs++;

        if (checkTimerRTP(t))
            updateTimeout(t->init, clock(), s); //Drop timeout if its slow
        while (!checkTimerRTP(t) && !recvvalid) {
            if (recvfrom(s->fd, &buf, sizeof(struct Header_t), MSG_PEEK, (struct sockaddr *) &addr, &len) == sizeof(struct Header_t)) {
                bufsize = readDataSizeRTP(&buf[0]);
                if (bufsize > mssRTP)
                    errorRTP("Packet that is too large was received", ERROR_MISC);
                while (recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), MSG_PEEK) != bufsize + sizeof(struct Header_t));
                recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), 0);

                Packet precv = readPacketRTP(buf);
                updateTimeout(t->init, clock(), s);
                if (precv == NULL)
                    printf("Packet was not valid!\n");
                else {
                    logTimeLogger(LOGGERTIME_RCV, clock(), precv, s->logger);
                    s->logger->segr++;
                    if (precv->head->flags == ACK && precv->head->ack == psend->head->ack) {
                        recvvalid = 1;
                    }
                    freePacketRTP(precv);
                }

                memset(&buf, 0, sizeof(buf));

            }
        }
    }

    freePacketRTP(psend);

    s->connected = 1;

    freeTimerRTP(t);

}

void sendSocketRTP(void *data, size_t size, Socket s) {

    s->logger->size = size;

    int64_t max = (size / mssRTP);
    int64_t odd = size % mssRTP;

    ListRTP l = initListRTP(sizeof(struct Packet_t), 2);

    for (int64_t i = 0; i < max; i++) {
        Packet p = initPacketRTP(i * mssRTP, 0, 0, (char *) data + i * mssRTP, mssRTP);
        addListRTP(p, l);
        free(p); //Free the rep since we arent using it anymore
    }
    if (odd) {
        Packet p = initPacketRTP(max * mssRTP, 0, 0, (char *) data + max * mssRTP, size % mssRTP);
        addListRTP(p, l);
        free(p); //See above
    }

    int inittrans = 1;

    char buf[mpsRTP];
    size_t bufsize;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    memset(&addr, 0, sizeof(addr));

    Timer t = initTimerRTP();
    setTimerRTP(s->timeout, t);

    while (l->clength > 0) {

        setTimerRTP(s->timeout, t);
        resetTimerRTP(t);

        for (int64_t i = 0; i < windowsizeRTP && i < l->clength; i++) {
            Packet p = getListRTP(i, l);
            printf("latest packet:%d\n", p->head->seq);
            sendPLDRTP(p, s->fd, &s->peeraddr, s->pld, s->logger);
        }

        if (checkTimerRTP(t))
            updateTimeout(t->init, clock(), s); //Drop timeout if its slow
        while (!checkTimerRTP(t) && l->clength > 0) {
            if (recvfrom(s->fd, &buf, sizeof(struct Header_t), MSG_PEEK, (struct sockaddr *) &addr, &len) == sizeof(struct Header_t)) {
                bufsize = readDataSizeRTP(&buf[0]);
                if (bufsize > mssRTP)
                    errorRTP("Packet that is too large was received", ERROR_MISC);
                while (recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), MSG_PEEK) != bufsize + sizeof(struct Header_t));
                recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), 0);

                Packet precv = readPacketRTP(buf);
                updateTimeout(t->init, clock(), s);
                if (precv == NULL) {
                    logTimeLogger(LOGGERTIME_RCVCORR, clock(), precv, s->logger);
                    s->logger->segr++;
                    //s->logger->scorrupted++; //Distorts sent corrupted segments
                    printf("Packet was not valid!\n");
                } else {
                    if (precv->head->flags == ACK) { //Accept the packet if its an ACK (and its valid ofc but we have already done this)
                        int removed = 0;
                        for (int64_t i = 0; i < windowsizeRTP && i < l->clength;) {
                            Packet prem = getListRTP(i, l);
                            if (prem->head->seq + prem->head->len == precv->head->ack) {
                                removed = 1;
                                free(prem->head);
                                removeListRTP(i, l);
                                if (l->clength >= windowsizeRTP) {
                                    Packet p = getListRTP(windowsizeRTP - 1, l);
                                    resetTimerRTP(t);
                                    sendPLDRTP(p, s->fd, &s->peeraddr, s->pld, s->logger); //Send the receiver another packet if we can send it
                                }
                            } else {
                                i++; //So we don't increment after we remove, which would make us miss a packet
                            }
                        }
                        if (removed == 0){
                            logTimeLogger(LOGGERTIME_RCVDA, clock(), precv, s->logger);
                            s->logger->dack++;
                            s->logger->segr++;
                        } else {
                            logTimeLogger(LOGGERTIME_RCV, clock(), precv, s->logger);
                            s->logger->segr++;
                        }
                        //resetTimerRTP(t);
                    }
                    freePacketRTP(precv);
                }
                memset(&buf, 0, sizeof(buf));
            }
        }
    }

    //Send CMP
    Packet psend = initPacketRTP(size + 1, 0, CMP, NULL, 0);
    setTimerRTP(s->timeout * 2, t);
    int recvvalid = 0;

    while (!recvvalid) {
        setTimerRTP(s->timeout, t);
        resetTimerRTP(t);
        sendto(s->fd, psend->head, psend->size, 0, (struct sockaddr *) &s->peeraddr, sizeof(struct sockaddr_in));
        logTimeLogger(LOGGERTIME_SND, clock(), psend, s->logger);
        s->logger->segs++;

        if (checkTimerRTP(t))
            updateTimeout(t->init, clock(), s); //Drop timeout if its slow
        while (!checkTimerRTP(t) && !recvvalid) {
            if (recvfrom(s->fd, &buf, sizeof(struct Header_t), MSG_PEEK, (struct sockaddr *) &addr, &len) == sizeof(struct Header_t)) {
                bufsize = readDataSizeRTP(&buf[0]);
                if (bufsize > mssRTP)
                    errorRTP("Packet that is too large was received", ERROR_MISC);
                while (recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), MSG_PEEK) !=
                       bufsize + sizeof(struct Header_t));
                recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), 0);

                Packet precv = readPacketRTP(buf);
                updateTimeout(t->init, clock(), s);
                if (precv == NULL) {
                    logTimeLogger(LOGGERTIME_RCVCORR, clock(), precv, s->logger);
                    s->logger->segr++;
                    //s->logger->scorrupted++;
                    printf("Packet was not valid!\n");
                } else {
                    logTimeLogger(LOGGERTIME_RCV, clock(), precv, s->logger);
                    s->logger->segr++;
                    if (precv->head->flags == ACK && precv->head->ack == size + 1) {
                        recvvalid = 1;
                    }
                    freePacketRTP(precv);
                }

                memset(&buf, 0, sizeof(buf));

            }
        }
    }

    freePacketRTP(psend);

    freeListRTP(l);
    freeTimerRTP(t);
}

ssize_t recvSocketRTP(void *outbuf, size_t maxsize, Socket s) {
    char buf[sizeof(struct Header_t)];
    void *largebuf = NULL;
    size_t bufsize;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    memset(&addr, 0, sizeof(addr));

    u_int32_t cmpack = 0;

    Timer t = initTimerRTP();
    setTimerRTP(s->timeout, t);

    ListRTP l = initListRTP(sizeof(struct Packet_t), 2);

    int recvcmp = 0;

    while (!recvcmp) {
        if (recv(s->fd, &buf[0], sizeof(struct Header_t), MSG_PEEK) == sizeof(struct Header_t)) {
            bufsize = readDataSizeRTP(&buf[0]);
            if (largebuf == NULL) {
                largebuf = malloc(bufsize + sizeof(struct Header_t));
                if (largebuf == NULL){
                    errorRTP("Could not init largebuf in recv", ERROR_INIT);
                } else {
                    memset(largebuf, 0, bufsize);
                }
            }
            while (recv(s->fd, largebuf, bufsize + sizeof(struct Header_t), MSG_PEEK) != bufsize + sizeof(struct Header_t));
            recv(s->fd, largebuf, bufsize + sizeof(struct Header_t), 0);

            Packet precv = readPacketRTP(largebuf);
            if (precv == NULL) {
                logTimeLogger(LOGGERTIME_RCVCORR, clock(), precv, s->logger);
                s->logger->segr++;
                s->logger->scorrupted++;
                printf("Packet was not valid!\n");
            } else {
                if (precv->head->flags == 0) {
            	    printf("latest packet:%d\n", precv->head->seq);
                    Packet p = initPacketRTP(0, precv->head->seq + precv->head->len, ACK, NULL, 0);
                    sendto(s->fd, p->head, p->size, 0, (struct sockaddr *) &s->peeraddr, sizeof(struct sockaddr_in));
                    clock_t timesnd = clock();
                    clock_t timercv = clock();

                    if (l->clength == 0){
                        addListRTP(precv, l);
                        logTimeLogger(LOGGERTIME_RCV, timercv, precv, s->logger);
                        s->logger->segr++;
                        logTimeLogger(LOGGERTIME_SND, timesnd, p, s->logger);
                        s->logger->segs++;
                        free(precv);
                    } else {
                        int added = 0;
                        for (int64_t i = l->clength - 1; i >= -1 && !added; i--) {
                            if (i > -1) {
                                Packet plist = getListRTP(i, l);
                                if (plist->head->seq == precv->head->seq) {
                                    added = 1; //Drop it if we already have it
                                    logTimeLogger(LOGGERTIME_RCV, timercv, precv, s->logger);
                                    s->logger->segr++;
                                    s->logger->sduplicated++;
                                    logTimeLogger(LOGGERTIME_SNDDA, timesnd, p, s->logger);
                                    s->logger->segs++;
                                    s->logger->dack++;
                                    freePacketRTP(precv);
                                } else if (plist->head->seq < precv->head->seq) {
                                    insertListRTP(i + 1, precv, l);
                                    added = 1; //Dont bother putting the segment anywhere else
                                    logTimeLogger(LOGGERTIME_RCV, timercv, precv, s->logger);
                                    s->logger->segr++;
                                    logTimeLogger(LOGGERTIME_SND, timesnd, p, s->logger);
                                    s->logger->segs++;
                                    free(precv);
                                }
                            } else { //Place it at the beginning (probably will not happen)
                                insertListRTP(0, precv, l);
                                added = 1; //Similarly don't bother putting it anywhere else
                                logTimeLogger(LOGGERTIME_RCV, timercv, precv, s->logger);
                                s->logger->segr++;
                                logTimeLogger(LOGGERTIME_SND, timesnd, p, s->logger);
                                s->logger->segs++;
                                free(precv);
                            }
                        }
                    }
                    freePacketRTP(p);
                } else if (precv->head->flags == CMP){ //Maybe add a check for if its actually the right seq num rather than blindly trusting
                    logTimeLogger(LOGGERTIME_RCV, clock(), precv, s->logger);
                    s->logger->segr++;
                    recvcmp = 1;
                    cmpack = precv->head->seq;
                    freePacketRTP(precv);
                }

            }

            memset(&buf[0], 0, sizeof(buf));
            memset(largebuf, 0, bufsize);

        }
    }

    free(largebuf);

    //receive CMP
    int recvvalid = 1;
    Packet psend = initPacketRTP(0, cmpack, ACK, NULL, 0);

    while (recvvalid) {
        setTimerRTP(s->timeout, t);
        resetTimerRTP(t);
        sendto(s->fd, psend->head, psend->size, 0, (struct sockaddr *) &s->peeraddr, sizeof(struct sockaddr_in));
        logTimeLogger(LOGGERTIME_SND, clock(), psend, s->logger);
        s->logger->segs++;
        recvvalid = 0;

        if (checkTimerRTP(t))
            updateTimeout(t->init, clock(), s); //Drop timeout if its slow
        while (!checkTimerRTP(t) && !recvvalid) {
            if (recvfrom(s->fd, &buf, sizeof(struct Header_t), MSG_PEEK, (struct sockaddr *) &addr, &len)  == sizeof(struct Header_t)) {
                bufsize = readDataSizeRTP(&buf[0]);
                if (bufsize > mssRTP)
                    errorRTP("Packet that is too large was received", ERROR_MISC);
                while (recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), MSG_PEEK) != bufsize + sizeof(struct Header_t));
                recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), 0);

                Packet precv = readPacketRTP(buf);
                updateTimeout(t->init, clock(), s);
                if (precv == NULL)
                    printf("Packet was not valid!\n");
                else {
                    logTimeLogger(LOGGERTIME_RCV, clock(), precv, s->logger);
                    s->logger->segr++;
                    if (precv->head->flags == CMP) {
                        recvvalid = 1;
                    } else if (precv->head->flags == FIN){
                        awaitCloseSocketDefRTP(s, 1);
                    }
                    freePacketRTP(precv);
                }

                memset(&buf, 0, sizeof(buf));
            }
        }
    }

    freePacketRTP(psend);

    //Assuming our list is full of all the packets now;

    size_t size = 0;
    for (int64_t i = 0; i < l->clength; i++){
        Packet p = getListRTP(i, l);
        if (size + l->nsize <= maxsize){
            memcpy((char*)outbuf + size, p->data, p->dsize);
            size += p->dsize;
        }
        free(p->head);
    }

    freeListRTP(l);

    freeTimerRTP(t);

    s->logger->size = size;
    return size;
}

void closeSocketRTP(Socket s){

    if (s->connected == 0){
        return;
    }

    char buf[mpsRTP];
    size_t bufsize;
    int recvvalid = 0;

    Timer t = initTimerRTP();
    setTimerRTP(s->timeout, t);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    memset(&addr, 0, sizeof(addr));


    //FIN
    Packet psend = initPacketRTP(0, 0, FIN, NULL, 0);

    while (!recvvalid) {
        setTimerRTP(s->timeout, t);
        resetTimerRTP(t);
        sendto(s->fd, psend->head, psend->size, 0, (struct sockaddr *) &s->peeraddr, sizeof(struct sockaddr_in));
        logTimeLogger(LOGGERTIME_SND, clock(), psend, s->logger);
        s->logger->segs++;

        if (checkTimerRTP(t))
            updateTimeout(t->init, clock(), s); //Drop timeout if its slow
        while (!checkTimerRTP(t) && !recvvalid) {
            if (recvfrom(s->fd, &buf, sizeof(struct Header_t), MSG_PEEK, (struct sockaddr *) &addr, &len) == sizeof(struct Header_t)) {
                bufsize = readDataSizeRTP(&buf[0]);
                if (bufsize > mssRTP)
                    errorRTP("Packet that is too large was received", ERROR_MISC);
                while (recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), MSG_PEEK) != bufsize + sizeof(struct Header_t));
                recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), 0);

                Packet precv = readPacketRTP(buf);
                updateTimeout(t->init, clock(), s);
                if (precv == NULL)
                    printf("Packet was not valid!\n");
                else {
                    logTimeLogger(LOGGERTIME_RCV, clock(), precv, s->logger);
                    s->logger->segr++;
                    if (precv->head->flags == ACK && precv->head->ack == psend->head->ack) {
                        recvvalid = 1;
                    }
                    freePacketRTP(precv);
                }

                memset(&buf, 0, sizeof(buf));

            }
        }
    }

    freePacketRTP(psend);

    //RECV FIN

    recvvalid = 0;

    while (!recvvalid) {
        if (recvfrom(s->fd, &buf, sizeof(struct Header_t), MSG_PEEK, (struct sockaddr *) &addr, &len) == sizeof(struct Header_t)) {
            bufsize = readDataSizeRTP(&buf[0]);
            if (bufsize > mssRTP)
                errorRTP("Packet that is too large was received", ERROR_MISC);
            while (recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), MSG_PEEK) != bufsize + sizeof(struct Header_t));
            recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), 0);

            Packet precv = readPacketRTP(buf);
            if (precv == NULL)
                printf("Packet was not valid!\n");
            else {
                logTimeLogger(LOGGERTIME_RCV, clock(), precv, s->logger);
                s->logger->segr++;
                if (precv->head->flags == FIN) {
                    recvvalid = 1;
                }
                freePacketRTP(precv);
            }

            memset(&buf, 0, sizeof(buf));

        }
    }

    psend = initPacketRTP(0, 0, ACK, NULL, 0);
    recvvalid = 1;

    while (recvvalid) {
        setTimerRTP(s->timeout, t);
        resetTimerRTP(t);
        sendto(s->fd, psend->head, psend->size, 0, (struct sockaddr *) &s->peeraddr, sizeof(struct sockaddr_in));
        logTimeLogger(LOGGERTIME_SND, clock(), psend, s->logger);
        s->logger->segs++;
        recvvalid = 0;

        if (checkTimerRTP(t))
            updateTimeout(t->init, clock(), s); //Drop timeout if its slow
        while (!checkTimerRTP(t) && !recvvalid) {
            if (recvfrom(s->fd, &buf, sizeof(struct Header_t), MSG_PEEK, (struct sockaddr *) &addr, &len) == sizeof(struct Header_t)) {
                bufsize = readDataSizeRTP(&buf[0]);
                if (bufsize > mssRTP)
                    errorRTP("Packet that is too large was received", ERROR_MISC);
                while (recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), MSG_PEEK) != bufsize + sizeof(struct Header_t));
                recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), 0);

                Packet precv = readPacketRTP(buf);
                updateTimeout(t->init, clock(), s);
                if (precv == NULL)
                    printf("Packet was not valid!\n");
                else {
                    logTimeLogger(LOGGERTIME_RCV, clock(), precv, s->logger);
                    s->logger->segr++;
                    freePacketRTP(precv);
                    recvvalid = 1;
                }

                memset(&buf, 0, sizeof(buf));
            }
        }
    }

    freePacketRTP(psend);

    freeTimerRTP(t);

    s->connected = 0;

}

void awaitCloseSocketRTP(Socket s){
    awaitCloseSocketDefRTP(s, 0);
}

static void awaitCloseSocketDefRTP(Socket s, int recvfin){

    if (s->connected == 0){
        return;
    }

    char buf[mpsRTP];
    size_t bufsize;
    int recvvalid;

    Timer t = initTimerRTP();
    setTimerRTP(s->timeout, t);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    memset(&addr, 0, sizeof(addr));

    //RECV FIN
    recvvalid = recvfin;

    while (!recvvalid) {
        if (recvfrom(s->fd, &buf, sizeof(struct Header_t), MSG_PEEK, (struct sockaddr *) &addr, &len) == sizeof(struct Header_t)) {
            bufsize = readDataSizeRTP(&buf[0]);
            if (bufsize > mssRTP)
                errorRTP("Packet that is too large was received", ERROR_MISC);
            while (recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), MSG_PEEK) != bufsize + sizeof(struct Header_t));
            recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), 0);

            Packet precv = readPacketRTP(buf);
            if (precv == NULL)
                printf("Packet was not valid!\n");
            else {
                logTimeLogger(LOGGERTIME_RCV, clock(), precv, s->logger);
                s->logger->segr++;
                if (precv->head->flags == FIN) {
                    recvvalid = 1;
                }
                freePacketRTP(precv);
            }

            memset(&buf, 0, sizeof(buf));

        }
    }

    Packet psend = initPacketRTP(0, 0, ACK, NULL, 0);
    recvvalid = 1;

    while (recvvalid) {
        setTimerRTP(s->timeout, t);
        resetTimerRTP(t);
        sendto(s->fd, psend->head, psend->size, 0, (struct sockaddr *) &s->peeraddr, sizeof(struct sockaddr_in));
        logTimeLogger(LOGGERTIME_SND, clock(), psend, s->logger);
        s->logger->segs++;
        recvvalid = 0;

        if (checkTimerRTP(t))
            updateTimeout(t->init, clock(), s); //Drop timeout if its slow
        while (!checkTimerRTP(t) && !recvvalid) {
            if (recvfrom(s->fd, &buf, sizeof(struct Header_t), MSG_PEEK, (struct sockaddr *) &addr, &len) == sizeof(struct Header_t)) {
                bufsize = readDataSizeRTP(&buf[0]);
                if (bufsize > mssRTP)
                    errorRTP("Packet that is too large was received", ERROR_MISC);
                while (recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), MSG_PEEK) != bufsize + sizeof(struct Header_t));
                recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), 0);

                Packet precv = readPacketRTP(buf);
                updateTimeout(t->init, clock(), s);
                if (precv == NULL)
                    printf("Packet was not valid!\n");
                else {
                    logTimeLogger(LOGGERTIME_RCV, clock(), precv, s->logger);
                    s->logger->segr++;
                    freePacketRTP(precv);
                    recvvalid = 1;
                }

                memset(&buf, 0, sizeof(buf));
            }
        }
    }


    freePacketRTP(psend);

    //FIN
    psend = initPacketRTP(0, 0, FIN, NULL, 0);

    while (!recvvalid) {
        setTimerRTP(s->timeout, t);
        resetTimerRTP(t);
        sendto(s->fd, psend->head, psend->size, 0, (struct sockaddr *) &s->peeraddr, sizeof(struct sockaddr_in));
        logTimeLogger(LOGGERTIME_SND, clock(), psend, s->logger);
        s->logger->segs++;

        if (checkTimerRTP(t))
            updateTimeout(t->init, clock(), s); //Drop timeout if its slow
        while (!checkTimerRTP(t) && !recvvalid) {
            if (recvfrom(s->fd, &buf, sizeof(struct Header_t), MSG_PEEK, (struct sockaddr *) &addr, &len) == sizeof(struct Header_t)) {
                bufsize = readDataSizeRTP(&buf[0]);
                if (bufsize > mssRTP)
                    errorRTP("Packet that is too large was received", ERROR_MISC);
                while (recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), MSG_PEEK) != bufsize + sizeof(struct Header_t));
                recv(s->fd, &buf[0], bufsize + sizeof(struct Header_t), 0);

                Packet precv = readPacketRTP(buf);
                updateTimeout(t->init, clock(), s);
                if (precv == NULL)
                    printf("Packet was not valid!\n");
                else {
                    logTimeLogger(LOGGERTIME_RCV, clock(), precv, s->logger);
                    s->logger->segr++;
                    if (precv->head->flags == ACK && precv->head->ack == psend->head->ack) {
                        recvvalid = 1;
                    }
                    freePacketRTP(precv);
                }

                memset(&buf, 0, sizeof(buf));

            }
        }
    }

    freePacketRTP(psend);

    freeTimerRTP(t);

    s->connected = 0;

}

void setSocketOptRTP(int level, int option_name, void *option_value, socklen_t socklen, Socket s) {
    if (setsockopt(s->fd, level, option_name, option_value, socklen) == -1) {
        errorRTP("Could not set socketopt", ERROR_MISC);
    }
}
