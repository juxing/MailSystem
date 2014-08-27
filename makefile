.c.o:
	gcc -g -c $?

# compile client and server
all: mailclient mailserver

# compile client only
mailclient: mailclient.o maillinker.o
	gcc -g -o mailclient mailclient.o  maillinker.o 

# compile server program
mailserver: mailserver.o maillinker.o
	gcc -g -o mailserver mailserver.o  maillinker.o -pthread
clean:
	rm *.o mailserver mailclient 
