//
// Created by riley on 12/10/18.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "pld.h"
#include "rtp.h"
#include "timer.h"

static int orderc = 0;
static Packet orderp = NULL;

PLD initPLDRTP(double pDrop, double pDuplicate, double pCorrupt, double pOrder, int maxOrder, double pDelay, int maxDelay, unsigned int seed){
    PLD pld = malloc(sizeof(struct PLD_t));
    if (pld == NULL){
        errorRTP("Could not init PLD", ERROR_INIT);
    }

    pld->pDrop = pDrop;
    pld->pDuplicate = pDuplicate;
    pld->pCorrupt = pCorrupt;
    pld->pOrder = pOrder;
    pld->maxOrder = maxOrder;
    pld->pDelay = pDelay;
    pld->maxDelay = maxDelay;
    pld->seed = seed;

    srand(seed);

    return pld;
}

void freePLDRTP(PLD pld){
    free(pld);
}

void sendPLDRTP(Packet p, int fd, struct sockaddr_in *peeraddr, PLD pld, Logger logger) {

    if (orderp != NULL){
        sendto(fd, p->head, p->size, 0, (struct sockaddr *) peeraddr, sizeof(struct sockaddr_in));
        logTimeLogger(LOGGERTIME_SND, clock(), p, logger);
        logger->segs++;
        orderc++;
        if (orderc == pld->maxOrder){
            sendto(fd, orderp->head, orderp->size, 0, (struct sockaddr *) peeraddr, sizeof(struct sockaddr_in));
            logTimeLogger(LOGGERTIME_SNDREORD, clock(), orderp, logger);
            logger->segs++;
            logger->sreordered++;
            freePacketRTP(orderp);
            orderp = NULL;
            orderc = 0;
        }
        return;
    }

    double ran = rand()/((double)(RAND_MAX) + 1);
    if (ran < pld->pDrop){
        logTimeLogger(LOGGERTIME_SNDDROP, clock(), p, logger);
        logger->sdropped++;
        return;
    }

    ran = rand()/((double)(RAND_MAX) + 1);
    if (ran < pld->pDuplicate){
        sendto(fd, p->head, p->size, 0, (struct sockaddr *) peeraddr, sizeof(struct sockaddr_in));
        logTimeLogger(LOGGERTIME_SNDDUP, clock(), p, logger);
        logger->segs++;
        sendto(fd, p->head, p->size, 0, (struct sockaddr *) peeraddr, sizeof(struct sockaddr_in));
        logTimeLogger(LOGGERTIME_SNDDUP, clock(), p, logger);
        logger->segs++;
        logger->sduplicated++;
        return;
    }

    ran = rand()/((double)(RAND_MAX) + 1);
    if (ran < pld->pCorrupt){
        ((char*)p->data)[(int)ran * p->dsize] ^= 0x1;
        sendto(fd, p->head, p->size, 0, (struct sockaddr *) peeraddr, sizeof(struct sockaddr_in));
        ((char*)p->data)[(int)ran * p->dsize] ^= 0x1;
        logTimeLogger(LOGGERTIME_SNDCORR, clock(), p, logger);
        logger->segs++;
        logger->scorrupted++;
        return;
    }

    ran = rand()/((double)(RAND_MAX) + 1);
    if (ran  < pld->pOrder){
        if (orderp == NULL){
            orderp = initPacketRTP(p->head->seq, p->head->ack, p->head->flags, p->data, p->dsize);
            if (orderp == NULL)
                errorRTP("Could not allocate packet in PLD", ERROR_INIT);
            orderc = 0;
        }
        return;
    }

    ran = rand()/((double)(RAND_MAX) + 1);
    if (ran < pld->pDelay){
        Timer t = initTimerRTP();
        double time = ran * ((pld->maxDelay / 1000) * CLOCKS_PER_SEC);
        setTimerRTP((clock_t)time, t);
        resetTimerRTP(t);
        while (!checkTimerRTP(t));
        freeTimerRTP(t);

        sendto(fd, p->head, p->size, 0, (struct sockaddr *) peeraddr, sizeof(struct sockaddr_in));
        logTimeLogger(LOGGERTIME_SNDDELAY, clock(), p, logger);
        logger->segs++;
        logger->sdelayed++;

        return;
    }

    sendto(fd, p->head, p->size, 0, (struct sockaddr *) peeraddr, sizeof(struct sockaddr_in));
    logTimeLogger(LOGGERTIME_SND, clock(), p, logger);
    logger->segs++;
}
