#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>

#include "common.h"

int cregister(int sock, char *name) {
    Packet *pkt = NULL;

    sendPkt(sock, REGISTER, strlen(name) + 1, name);
    //printf("before recv\n");
    pkt = recvPkt(sock);
    //printf("after recv\n");
    if(!pkt) {
        fprintf(stderr, "Can't register to server.\n");
        return 1;
    }

    if(pkt->type == REG_REJECTED) {
        fprintf(stderr, "Can't register to server.\n");
        free(pkt);
        return 1;
    }

    if(pkt->type == REG_ACCEPTED) {
        //fprintf(stderr, "Register accepted.\n");
        printf("%s", pkt->text);
        free(pkt);
        return 0;
    }
    free(pkt);
    //fprintf(stderr, "No msg\n");
}

int main(int argc, char *argv[]) {
    if(argc != 4) {
        fprintf(stderr, "usage: mailclient username server_ip_address 5xyz");
        exit(1);
    }

    char *userName = argv[1];
    char *serverIP = argv[2];
    char *serverPort = argv[3];
    int sock = hookToServer(serverIP, serverPort);
    if(sock == -1) {
        exit(1);
    }

    //fprintf(stderr, "Before register.\n");
    if(cregister(sock, userName)) {
        exit(0);
    }

    fd_set mySet;
    FD_ZERO(&mySet);
    FD_SET(0, &mySet);
    FD_SET(sock, &mySet);

    int fdMax = sock;
    while(1) {
        FD_ZERO(&mySet);
        FD_SET(0, &mySet);
        FD_SET(sock, &mySet);

        if(select(fdMax + 1, &mySet, NULL, NULL, NULL) < 0) {
            fprintf(stderr, "Failed at select.");
            continue;
        }

        if(FD_ISSET(sock, &mySet)) {
            Packet *pkt = NULL;
            pkt = recvPkt(sock);
            if(!pkt) {
                exit(0);           
            } 
            else {
                switch(pkt->type) {
                    case MAIL:
                        printf("%s", pkt->text);
                        break;
                    case LIST:
                        printf("%s\n", pkt->text);
                        break;
                }
            }
            free(pkt->text);
            free(pkt);
        }

        if(FD_ISSET(0, &mySet)) {
            char buf[MAXPKTLEN];
            fgets(buf, MAXPKTLEN, stdin);
            //printf("fgets length is: %d\n", strlen(buf));
            if(strncmp(buf, "list", strlen("list")) == 0) {
                sendPkt(sock, LIST, 0, NULL);
            }
            else if(strncmp(buf, "close", strlen("close")) == 0) {
                sendPkt(sock, CLOSE, 0, NULL);
                break;
            }
            else {
                if(strlen(buf) > 1)
                    sendPkt(sock, MAIL, strlen(buf) + 1, buf);    
            }
        }
    }
}


