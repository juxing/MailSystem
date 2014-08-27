#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>

#include "common.h"

#define PORT 5517
#define LISTENQ 10

int startServer() {
    int sd;
    int myport;
    
    if((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Can't create socket.\n");
        return -1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(PORT);

    if(bind(sd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        fprintf(stderr, "Can't bind socket.\n");
        close(sd);
        return -1;
    }

    listen(sd, LISTENQ);
    printf("Welcome to Ming's E-mail Server, running on port %d.\n", PORT);
    return sd;
}

int hookToServer(char *serverIP, char *serverPort) {
    int sd;
    if((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Can't create socket.\n");
        return -1;
    }

    struct sockaddr_in sin;
    bzero(&sin, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET; 
    sin.sin_addr.s_addr = inet_addr(serverIP);
    sin.sin_port = htons(atoi(serverPort));

    if(connect(sd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        fprintf(stderr, "Can't connect to server.\n");
        close(sd);
        return -1;
    }

    return sd;
}
void sendPkt(int sd, char type, long len, char *buf) {
    char tmp[8];
    long nlen;

    bcopy(&type, tmp, sizeof(type));
    nlen = htonl(len);
    bcopy((char*)&nlen, tmp + sizeof(type), sizeof(nlen));
    write(sd, tmp, sizeof(type) + sizeof(nlen));
    
    //printf("sendPkt: %s\n", buf);
    if(len > 0)
        write(sd, buf, len); 
}

Packet *recvPkt(int sd) {
    Packet *pkt = NULL;

    pkt = (Packet*)calloc(1, sizeof(Packet));
    if(!pkt) {
        fprintf(stderr, "Can't calloc.\n");
        return NULL;
    }

    if(!readn(sd, (char*)&pkt->type, sizeof(pkt->type))) {
        free(pkt);
        return NULL;
    }

    if(!readn(sd, (char*)&pkt->len, sizeof(pkt->len))) {
        free(pkt);
        return NULL;
    }
    pkt->len = ntohl(pkt->len);

    if(pkt->len > 0) {
        pkt->text = (char*)malloc(pkt->len);
        if(!pkt->text) {
            fprintf(stderr, "Can't malloc.\n");
            free(pkt);
            return NULL;
        }

        if(!readn(sd, pkt->text, pkt->len)) {
            free(pkt);
            return NULL;
        }
    }

    //printf("recvPkt: type %d len %d text %s\n", pkt->type, pkt->len, pkt->text);
    return pkt;
}

int readn(int sd, char *buf, int n) {
    int toberead;
    char *ptr;

    toberead = n;
    ptr = buf;
    while(toberead > 0) {
        int byteread;
      
        byteread = read(sd, ptr, toberead);
        if(byteread < 0) {
            fprintf(stderr, "Failed at read.\n");
            return 0;
        }

        toberead -= byteread;
        ptr += byteread;
    }

    return 1;
}
