//
// Created by riley on 12/10/18.
//

#ifndef LOGGER_H
#define LOGGER_H

#include <time.h>

#include "list.h"
#include "packet.h"

typedef struct Logger_t{
    size_t size;
    unsigned int segs;
    unsigned int segr;
    unsigned int sdat;
    unsigned int sdropped;
    unsigned int scorrupted;
    unsigned int sreordered;
    unsigned int sduplicated;
    unsigned int sdelayed;
    unsigned int rtimeout;
    unsigned int fretransmit;
    unsigned int dack;
    ListRTP timel;
    clock_t inittime;
} *Logger;

typedef enum LoggerTimeType_t{
    LOGGERTIME_SND = 0,
    LOGGERTIME_RCV = 1,
    LOGGERTIME_SNDRXT = 2,
    LOGGERTIME_RCVRXT = 3,
    LOGGERTIME_SNDCORR = 4,
    LOGGERTIME_RCVCORR = 5,
    LOGGERTIME_SNDDA = 6,
    LOGGERTIME_RCVDA = 7,
    LOGGERTIME_SNDDROP = 8,
    LOGGERTIME_SNDREORD = 9,
    LOGGERTIME_SNDDUP = 10,
    LOGGERTIME_SNDDELAY = 11
} LoggerTimeType;

typedef struct LoggerTime_t{
    LoggerTimeType ltt;
    clock_t time;
    int8_t flags;
    u_int32_t seq;
    u_int16_t len;
    u_int32_t ack;
} LoggerTime;

typedef enum LoggerPrintType_t{
    LOGGER_SENDER = 0,
    LOGGER_RECEIVER = 1,
    LOGGER_ALL = 2
} LoggerPrintType;

//Init
Logger initLogger();

//Free
void freeLogger(Logger l);

//Misc
void logTimeLogger(LoggerTimeType, clock_t, Packet, Logger);
void writeLogger(char*, LoggerPrintType, Logger l);

//Diag
void printLoggerDiags(Logger);

#endif
