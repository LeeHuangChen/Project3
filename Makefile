CC = g++
COPTS = -Wall -Wno-deprecated 
LKOPTS = 

OBJS =\
HEADRES =\

all:	sendfile recvfile

sendfile: sendfile.cc
	$(CC) $(DEFS) $(CFLAGS) $(LIB) -o sendfile sendfile.cc

recvfile: recvfile.cc
	$(CC) $(COPTS) -o recvfile recvfile.cc



clean:
	rm -f *.o
	rm -f *~
	rm -f core.*.*
