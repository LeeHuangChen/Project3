CC      = g++
LD      = g++
CFLAGS      = -Wall -g -Wno-deprecated -pthread

all:	sendfile recvfile

sendfile: sendfile.cc
	$(CC) $(DEFS) $(CFLAGS) $(LIB) -o sendfile sendfile.cc

recvfile: recvfile.cc
	$(CC) $(COPTS) $(CFLAGS) $(LIB) -o recvfile recvfile.cc



clean:
	rm -f *.o
	rm -f *~
	rm -f core.*.*
