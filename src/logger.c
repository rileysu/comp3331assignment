//
// Created by riley on 12/10/18.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "logger.h"
#include "rtp.h"
#include "packet.h"

Logger initLogger(){
    Logger l = malloc(sizeof(struct Logger_t));
    if (l == NULL){
        errorRTP("Could not init logger", ERROR_INIT);
    }

    memset(l, 0, sizeof(struct Logger_t));

    l->timel = initListRTP(sizeof(struct LoggerTime_t), 2);
    l->inittime = clock();

    return l;
}

void freeLogger(Logger l){
    freeListRTP(l->timel);
    free(l);
}

void logTimeLogger(LoggerTimeType ltt, clock_t time, Packet p, Logger l) {
    LoggerTime lt;
    lt.ltt = ltt;
    lt.time = time - l->inittime;
    if (p != NULL) {
        lt.flags = p->head->flags;
        lt.seq = p->head->seq;
        lt.len = p->head->len;
        lt.ack = p->head->ack;
    } else {
        lt.flags = 0;
        lt.seq = 0;
        lt.len = 0;
        lt.ack = 0;
    }

    addListRTP(&lt, l->timel);
}

void writeLogger(char *file, LoggerPrintType type, Logger l){
    FILE *fp = fopen(file, "w");

    if (fp == NULL)
        errorRTP("Could not open log file", ERROR_INIT);

    char *lttypes[] = {"snd", "rcv", "snd/RXT", "rcv/RXT", "snd/corr", "rcv/corr", "snd/DA", "rcv/DA", "snd/drop", "snd/reord", "snd/dup", "snd/delay"};

    //fprintf(fp, "")

    for (int64_t i = 0; i < l->timel->clength; i++){
        LoggerTime lt = *(LoggerTime*) getListRTP(i, l->timel);
        fprintf(fp, "%s\t%lf\t", lttypes[lt.ltt], (double) (lt.time) / (CLOCKS_PER_SEC));
        if (lt.flags & SYN){
            fprintf(fp, "S");
        }
        if (lt.flags & FIN){
            fprintf(fp, "F");
        }
        if (lt.flags & ACK){
            fprintf(fp, "A");
        }
        if (lt.flags & CMP){
            fprintf(fp, "C");
        }
        fprintf(fp, "\t%u\t%u\t%u\n", lt.seq, lt.len, lt.ack);
    }

    for (int i = 0; i < 30; i++)
        fprintf(fp, "=");
    fprintf(fp, "\n");

    fprintf(fp, "Size of the file (in Bytes)\t%lu\n", l->size);
    if (type == LOGGER_SENDER || type == LOGGER_ALL){
        fprintf(fp, "Segments sent\t%u\n", l->segs);
        fprintf(fp, "Segments dropped\t%u\n", l->sdropped);
        fprintf(fp, "Segments corrupted\t%u\n", l->scorrupted);
        fprintf(fp, "Segments re-ordered\t%u\n", l->sreordered);
        fprintf(fp, "Segments duplicated\t%u\n", l->sduplicated);
        fprintf(fp, "Segments delayed\t%u\n", l->sdelayed);
        //fprintf(fp, "Retransmitted\t%u\n", l->rtimeout);
        //fprintf(fp, "Fast retransmits\t%u\n", l->fretransmit);
        fprintf(fp, "Duplicate ACKs\t%u\n", l->dack);

    }
    if (type == LOGGER_RECEIVER || type == LOGGER_ALL){
        fprintf(fp, "Segments received\t%u\n", l->segr);
        fprintf(fp, "Segments bit errors\t%u\n", l->scorrupted);
        fprintf(fp, "Segments duplicated\t%u\n", l->sduplicated);
        fprintf(fp, "Duplicate ACKs\t%u\n", l->dack);
    }

    for (int i = 0; i < 30; i++)
        fprintf(fp, "=");
    fprintf(fp, "\n");

    fclose(fp);
}

void printLoggerDiags(Logger l){
    FILE *fp = stdout;
    LoggerPrintType type = LOGGER_ALL;

    char *lttypes[] = {"snd", "rcv", "snd/RXT", "rcv/RXT", "snd/corr", "rcv/corr", "snd/DA", "rcv/DA", "snd/drop", "snd/reord", "snd/dup", "snd/delay"};

    //fprintf(fp, "")

    for (int64_t i = 0; i < l->timel->clength; i++){
        LoggerTime lt = *(LoggerTime*) getListRTP(i, l->timel);
        fprintf(fp, "%s\t%.2lf\t", lttypes[lt.ltt], (double) (lt.time) / (CLOCKS_PER_SEC));
        if (lt.flags & SYN){
            fprintf(fp, "S");
        }
        if (lt.flags & FIN){
            fprintf(fp, "F");
        }
        if (lt.flags & ACK){
            fprintf(fp, "A");
        }
        if (lt.flags & CMP){
            fprintf(fp, "C");
        }
        fprintf(fp, "\t%u\t%u\t%u\n", lt.seq, lt.len, lt.ack);
    }

    fprintf(fp, "Clength: %ld\n", l->timel->clength);

    for (int i = 0; i < 30; i++)
        fprintf(fp, "=");
    fprintf(fp, "\n");

    fprintf(fp, "Size of the file (in Bytes)\t%lu\n", l->size);
    if (type == LOGGER_SENDER || type == LOGGER_ALL){
        fprintf(fp, "Segments sent\t%u\n", l->segs);
        fprintf(fp, "Segments dropped\t%u\n", l->sdropped);
        fprintf(fp, "Segments corrupted\t%u\n", l->scorrupted);
        fprintf(fp, "Segments re-ordered\t%u\n", l->sreordered);
        fprintf(fp, "Segments duplicated\t%u\n", l->sduplicated);
        fprintf(fp, "Segments delayed\t%u\n", l->sdelayed);
        //fprintf(fp, "Retransmitted\t%u\n", l->rtimeout);
        //fprintf(fp, "Fast retransmits\t%u\n", l->fretransmit);
        fprintf(fp, "Duplicate ACKs\t%u\n", l->dack);

    }
    if (type == LOGGER_RECEIVER || type == LOGGER_ALL){
        fprintf(fp, "Segments received\t%u\n", l->segr);
        fprintf(fp, "Segments bit errors\t%u\n", l->scorrupted);
        fprintf(fp, "Segments duplicated\t%u\n", l->sduplicated);
        fprintf(fp, "Duplicate ACKs\t%u\n", l->dack);
    }

    for (int i = 0; i < 30; i++)
        fprintf(fp, "=");
    fprintf(fp, "\n");
}
