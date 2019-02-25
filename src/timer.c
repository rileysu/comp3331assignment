#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "timer.h"
#include "rtp.h"

Timer initTimerRTP(){
    Timer t = malloc(sizeof(struct Timer_t));
    if (t == NULL){
        errorRTP("Could not allocate memory for timer", ERROR_INIT);
    }

    return t;
}

void freeTimerRTP(Timer t){
    free(t);
}

void resetTimerRTP(Timer t){
    t->init = clock();
}

int checkTimerRTP(Timer t){
    clock_t curr = clock();

    if (t->init + t->diff > curr){
        return 0;
    } else {
        return 1;
    }
}

void setTimerRTP(clock_t diff, Timer t){
    t->diff = diff;
}

void sleepTimerRTP(Timer t) {
    if (!checkTimerRTP(t)) {
        double sleep = (t->init + t->diff - clock()) * 1000000 / CLOCKS_PER_SEC;
        usleep((clock_t)sleep);
    }
}