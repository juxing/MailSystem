#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "common.h"

typedef struct _mail {
    char *sender;
    char *receiver;
    char *recvIP;
    char *text;
    struct _mail *next;
    struct _mail *prev;
} Mail;

typedef struct _user {
    char *name;
    int sock;
    struct _user *next;
    struct _user *prev;
} User;

Mail *myMails = NULL;
User *myUsers = NULL;
int stop = 0;

int sregister(char *name, int sock) {
    //printf("%s\n", name);
    User *u = myUsers;
    while(u != NULL) {
        if(strcmp(u->name, name) == 0) {  //User exits, cannot register.
            return 1;
        }
        u = u->next;
    }

    u = (User*)malloc(sizeof(User));
    u->name = strdup(name);
    u->sock = sock;
    u->prev = NULL;
    u->next = myUsers;
    if(myUsers != NULL)
        myUsers->prev = u;
    myUsers = u;

    return 0;
}

void unregister(int sock) {
    User *u = myUsers;
    while(u != NULL) {
        if(u->sock == sock) {
            if(u->next) {
                u->next->prev = u->prev;
            }

            if(u == myUsers) {
                myUsers = u->next;
            }
            else {
                u->prev->next = u->next;
            }

            free(u->name);
            free(u);
            break;
        }
        u = u->next;
    }
}

//Must find one.
User *findUserBySock(int sock) {
    User *u = myUsers;
    while(u != NULL) {
        if(u->sock == sock)
            return u;
        u = u->next;
    }
}

Mail *preprocessMail(char *text, int sock) {
    Mail *mail = (Mail*)malloc(sizeof(Mail));
    char *idxAt = index(text, '@');
    char *idxSpace = index(text, ' ');
    if((idxAt == NULL) || (idxAt == text)) {
        free(mail);
        return NULL;
    }
    else {
        *idxAt = '\0';
        mail->receiver = strdup(text);
    }

    idxAt++;
    if(!(idxSpace > idxAt)) {
        free(mail->receiver);
        free(mail);
        return NULL;
    }
    else {
        *idxSpace = '\0';
        mail->recvIP = strdup(idxAt);
    }

    idxSpace++;
    if(*idxSpace != '\0') {
        mail->text = strdup(idxSpace);
    }
    else {
        free(mail->receiver);
        free(mail->recvIP);
        free(mail);
        return NULL;
    }

    User *u = findUserBySock(sock);
    mail->sender = strdup(u->name);

    return mail;
}

void addToMailList(Mail *mail) {
    mail->prev = NULL;
    mail->next = myMails;
    if(myMails != NULL)
        myMails->prev = mail;
    myMails = mail;
}

void removeFromMailList(Mail *mail) {
    if(mail->next) {
        mail->next->prev = mail->prev;
    }
    if(mail == myMails) {
        myMails = mail->next;
    }
    else {
        mail->prev->next = mail->next;
    }
   
    free(mail->sender);
    free(mail->receiver);
    free(mail->recvIP);
    free(mail->text);
    free(mail);
}

void sendMail(User *u, Mail *mail) {
    char pktBuf[MAXPKTLEN];
    char *bufPtr;
    long len;

    bufPtr = pktBuf;
    strcpy(bufPtr, mail->sender);
    bufPtr += strlen(bufPtr);
    *bufPtr = ':';
    bufPtr += 1;
    strcpy(bufPtr, mail->text);
    bufPtr += strlen(bufPtr) + 1;
    len = bufPtr - pktBuf;

    //fprintf(stderr, "Send mail from %s to %s:%d -- %s", mail->sender, mail->receiver, u->sock, pktBuf);
    sendPkt(u->sock, MAIL, len, pktBuf);
    removeFromMailList(mail);
}

void sendListInfo(int sock) {
    char pktBuf[MAXPKTLEN];
    char *bufPtr;
    long len;
    Mail *mail;
    User *u;
    //sleep(1);
    mail = myMails;
    while(mail != NULL) {
        bufPtr = pktBuf;
        strcpy(bufPtr, mail->sender);
        bufPtr += strlen(bufPtr);
        *bufPtr = ' ';
        bufPtr += 1;
        strcpy(bufPtr, mail->receiver);
        bufPtr += strlen(bufPtr);
        *bufPtr = ' ';
        bufPtr += 1;
        strcpy(bufPtr, mail->text);
        bufPtr += strlen(bufPtr) + 1;
        len = bufPtr - pktBuf;
        sendPkt(sock, LIST, len, pktBuf);

        mail = mail->next;
    }

    u = myUsers;
    while(u != NULL) {
        bufPtr = pktBuf;
        strcpy(bufPtr, u->name);
        bufPtr += strlen(bufPtr) + 1;
        len = bufPtr - pktBuf;
        sendPkt(sock, LIST, len, pktBuf);

        u = u->next;
    }
}

void *detectTask(void *data) {
    //int s = (int)data;
    //int *ss = (int*)data;
    while(stop == 0) {
        sleep(30);
        Mail *mail = myMails;
        while(mail != NULL) {
            User *u = myUsers;
            while(u != NULL) {
                if(strcmp(u->name, mail->receiver) == 0) {
                    User *tmp = u->next;
                    sendMail(u, mail);
                    u = tmp;
                    continue;
                }           
                u = u->next;
            }
            mail = mail->next;
        }
        //debugMails();
    }
    //if(myUsers == NULL) {}
    //if(stop) {}
    //fprintf(stderr, "Thread exit.\n");
    pthread_exit(NULL);
}

void debugUsers() {
    User *u = myUsers;
    if(u == NULL) {
        printf("No users.\n");
        return;
    }

    while(u != NULL) {
        printf("UserName: %s sock: %d\n", u->name, u->sock);
        u = u->next;
    }
}

void debugMails() {
    Mail *mail = myMails;
    if(mail == NULL) {
        printf("No mails.\n");
        return;
    }

    while(mail != NULL) {
        printf("%s %s %s %s\n", mail->sender, mail->receiver, mail->recvIP, mail->text);
        mail = mail->next;
    }
}

int main(int argc, char *argv[]) {
    int servSock;
    int maxsd;

    if((servSock = startServer()) == -1) {
        exit(1);
    }

    //Statt thread.
    pthread_t tid;
    //pthread_attr_t attr;
    //pthread_attr_init(&attr);
    //pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    int retval;
    retval = pthread_create(&tid, NULL, detectTask, NULL);
    //printf("After create thread.\n");
    if(retval) {
        fprintf(stderr, "Cannot start detector thread.\n");
        exit(1);
    }
    //retval = pthread_join(tid, NULL);
    //printf("%d", retval);
    //printf("hello");
    //if(retval != 0)
    //    printf("Thread is wrong.");

    maxsd = servSock;

    fd_set livesdset, currset;
    FD_ZERO(&livesdset);
    FD_ZERO(&currset);
    FD_SET(servSock, &livesdset);
    FD_SET(0, &livesdset);

    while(1) {
        int sock;
        currset = livesdset;
        if(select(maxsd + 1, &currset, NULL, NULL, NULL) < 0) {
            fprintf(stderr, "Failed at select.\n");
            continue;
        }

        for(sock = 3; sock <= maxsd; sock++) {
            if(sock == servSock)
                continue;

            if(FD_ISSET(sock, &currset)) {
                Packet *pkt = NULL;
                pkt = recvPkt(sock);
                if(!pkt) {
                    unregister(sock);
                    close(sock);
                    FD_CLR(sock, &livesdset);
                }
                else {
                    Mail *newMail = NULL;
                    switch(pkt->type) {
                        case REGISTER :
                            //printf("receive mesg\n");
                            //printf("pkt text is: %s\n", pkt->text);
                            if(sregister(pkt->text, sock) != 0)
                                sendPkt(sock, REG_REJECTED, 0, NULL);
                            else {
                                char buf[MAXPKTLEN];
                                strcpy(buf, "Welcome to Ming's E-mail Server, running on port 5517.\n");
                                sendPkt(sock, REG_ACCEPTED, strlen(buf) + 1, buf);
                            }
                            //debugUsers();
                            break;
                        case CLOSE :
                            unregister(sock);
                            close(sock);
                            FD_CLR(sock, &livesdset);
                            //debugUsers();
                            break;
                        case MAIL :
                            //Mail* newMail = NULL;  Bug, you cannot declare variable here.
                            newMail = preprocessMail(pkt->text, sock);
                            if(newMail != NULL)
                                addToMailList(newMail);
                            //debugMails();
                            break;
                        case LIST :
                            sendListInfo(sock);
                            break;
                    }
                    if(pkt->text != NULL)
                        free(pkt->text);
                    free(pkt);
                }
            }
        }

        if(FD_ISSET(servSock, &currset)) {
            int csd;

            struct sockaddr_in caddr;
            socklen_t clen = sizeof(caddr);
            csd = accept(servSock, (struct sockaddr*)&caddr, &clen);

            if(csd != -1) {
                FD_SET(csd, &livesdset);
                if(csd > maxsd)
                    maxsd = csd;
            }
        }

        if(FD_ISSET(0, &currset)) {
            char buf[MAXPKTLEN];
            fgets(buf, MAXPKTLEN, stdin);
            //printf("User input: %s", buf);
            if(strncmp(buf, "close", strlen("close")) == 0) {
                //printf("Server will close.\n");
                stop = 1;
                //sleep(2);  //For debug.
                close(servSock);
                break;
            }
        }
    }

}
