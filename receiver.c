#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "rtp.h"
#include "list.h"
#include "packet.h"
#include "timer.h"

#define DEFAULT_MSS 150
#define DEFAULT_MWS 600
#define DEFAULT_GAMMA 4

int main(int argc, char *argv[]){
    if (argc != 3)
        errorRTP("Incorrect amount of arguments entered!", ERROR_INIT);

    in_port_t receiverport = htons(atoi(argv[1]));

    Socket s = initSocketRTP(AF_INET, DEFAULT_MSS, DEFAULT_MWS, DEFAULT_GAMMA);
    bindSocketRTP(AF_INET, receiverport, INADDR_ANY, s);
    acceptSocketRTP(s);

    FILE *fp = fopen(argv[2], "w");
    if (fp == NULL){
        errorRTP("Could not open file\n", ERROR_INIT);
    }

    char *buf = calloc(sizeof(char), 20971520); //20MB limit

    ssize_t bufsize = recvSocketRTP(buf, 20971520, s);

    awaitCloseSocketRTP(s);

    for (int i = 0; i < bufsize; i++){
        fputc(buf[i], fp);
    }

    writeLogger("receiver.log", LOGGER_RECEIVER, s->logger);

    fclose(fp);
    
    freeSocketRTP(s);

    free(buf);

    return 0;

}
