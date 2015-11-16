CC = g++ -std=gnu++11
COPTS = -Wall -Wno-deprecated 
LKOPTS = 

OBJS =\
HEADRES =\

all:	sendfile

sendfile: sendfile.cc
	$(CC) $(DEFS) $(CFLAGS) $(LIB) -o sendfile sendfile.cc



clean:
	rm -f *.o
	rm -f *~
	rm -f core.*.*
