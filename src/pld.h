//
// Created by riley on 12/10/18.
//

#ifndef PLD_H
#define PLD_H

#include <netinet/ip.h>

#include "packet.h"
#include "logger.h"

typedef struct PLD_t {
    double pDrop;
    double pDuplicate;
    double pCorrupt;
    double pOrder;
    double maxOrder;
    double pDelay;
    double maxDelay;
    double seed;
} *PLD;

//Init
PLD initPLDRTP(double, double, double, double, int, double, int, unsigned int);

//Free
void freePLDRTP(PLD);

//Misc
void sendPLDRTP(Packet,int,struct sockaddr_in*,PLD,Logger);

//Diag

#endif
