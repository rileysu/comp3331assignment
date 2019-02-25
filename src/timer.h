#ifndef TIMER_H
#define TIMER_H

#include <time.h>

typedef struct Timer_t{
    clock_t init;
    clock_t diff;
} *Timer;

//Init
Timer initTimerRTP();

//Free
void freeTimerRTP(Timer);

//Misc
void resetTimerRTP(Timer);
int checkTimerRTP(Timer);
void setTimerRTP(clock_t,Timer);

void sleepTimerRTP(Timer);

#endif
