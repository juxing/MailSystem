#define MAXPKTLEN 1024

#define LIST 0
#define CLOSE 1
#define MAIL 2
#define REGISTER 3
#define REG_ACCEPTED 4
#define REG_REJECTED 5

typedef struct _packet {
    char type;
    long len;
    char *text;
} Packet;

extern int startServer();
extern int hookToServer(char*, char*);
extern void sendPkt(int, char, long, char*);
extern Packet *recvPkt(int);
extern int readn(int, char*, int);
