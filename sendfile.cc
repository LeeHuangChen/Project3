#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

#include <map>
#include <string>
#include <iterator>
#include <iostream>
#include <thread>         // std::thread
#include <memory>

/*syntax:  ./sendfile -r 127.0.0.1:18000 -f testfile2.txt*/

////////////////////////																							//////////////////
//  GLOBAL VARIABLES  //																							//////////////////
////////////////////////																							//////////////////
struct packet{
	char* buffer;
	unsigned int size;
	unsigned int start;
};
typedef struct packet PACKET;
enum ePacketType{
	ACK,
	DATA,
	FILENAME,
	FILE_END
};
//Send and Receive
std::map<unsigned int, std::shared_ptr<PACKET>> dataMap;
std::map<unsigned int, std::shared_ptr<PACKET>> ackMap;

//File i/o
char* filename;
FILE *inputFile; /* File to send*/
int packetSize = 2048;
unsigned int filesize;
unsigned int totalNumPackets;

//Network Variables
char* ip;
unsigned short portnum = -1; 
struct sockaddr_in servaddr; /* server address */
int sock;
struct sockaddr_in myaddr;


/////////////////////////////																						//////////////////
//  FUNCTION DECLARATIONS  //																						//////////////////
/////////////////////////////																						//////////////////

//Sending Data/Filename functions
void threadSend();

//Receiving ack functions
void threadRecv();

//Network Connection and Initialization
void initConnection(int argc, char** argv);
void handleOptions(int argc, char** argv);
int setupSocket();
int setupServerAddress();

//File i/o
void sendMessage(const char *my_message, unsigned int messageLength);
void readfile(char *sendBuffer, unsigned int readSize);
void recvMessage(char *my_message, unsigned int messageLength);

//Helper
void printInputContents();
 
/////////////////////																								//////////////////
//  MAIN FUNCTION  //																								//////////////////
/////////////////////																								//////////////////
//ZMAIN
int main(int argc, char** argv){
	//initialize the network connection
	initConnection(argc, argv);
	
	//open the input file
	inputFile = fopen(filename,"r");
	if(inputFile==NULL){
		fputs ("File error",stderr);exit(1);
	}

	//obtain file size:
	fseek(inputFile,0,SEEK_END);
	filesize = ftell(inputFile);
	rewind(inputFile);
	//+2 for the filename and file_end packet
	totalNumPackets=(filesize+(packetSize-1))/packetSize+2;
	printf("&&& totalNumPackets:%d\n",totalNumPackets);
	

	std::thread first (threadSend);
	std::thread second (threadRecv);
	first.join();
	second.join();

	//closes the file after the file is sent
	fclose(inputFile);
	//terminate the program
	printf("[completed]\n", );
	return 0;
}
 
///////////////////////////////////////////																			//////////////////
//              SEND THREAD              //																			//////////////////
///////////////////////////////////////////																			//////////////////
//ZSEND

//Global Variables for this section:

//int packetSize = 500;
int readingPositionInTheInputFile=0;
int bytesLeftToRead;
bool finishedReading=false;
bool finishedReceiving=false;
unsigned int readLocationInFile=0;

//window management
unsigned int windowStart=0;
unsigned int windowSize = 5;





//Helper functions for this section:
int getPacketSize();
int readNextPacket(char *buffer);
void addInfoToDataMap(unsigned int seqNum, char *buffer, unsigned int size, unsigned int start);
void addInfoToAckMap(unsigned int seqNum, char *buffer, unsigned int size);
void displayMap(std::map<unsigned int, std::shared_ptr<PACKET>> map, const char* name);
void makeBuffer(ePacketType type, unsigned int seqNum, char *payload, int payloadSize, char *result);
unsigned short checksum(char *buf, unsigned short size);
bool isDropPkt(char *buf, unsigned short size);
void addNextPacketToDataMap(unsigned int index);

//Code for this section:
void threadSend(){
	/*
	//open the input file
	inputFile = fopen(filename,"r");
	if(inputFile==NULL){
		fputs ("File error",stderr);exit(1);
	}

	//obtain file size:
	fseek(inputFile,0,SEEK_END);
	filesize = ftell(inputFile);
	rewind(inputFile);
	//+2 for the filename and file_end packet
	totalNumPackets=(filesize+(packetSize-1))/packetSize+2;
	printf("&&& totalNumPackets:%d\n",totalNumPackets);
	*/
	//local variables for this function
	unsigned int seqNum=1;
	int readSize=0;
	unsigned int nextPacketToSend=0;
	unsigned int nextPacketToRead=0;

	

	//read in the data inside the initial window and put it into dataMap
	for(nextPacketToRead=0;((nextPacketToRead<windowSize) && (nextPacketToRead<totalNumPackets));nextPacketToRead++){
		addNextPacketToDataMap(nextPacketToRead);
		//displayMap(dataMap,"dataMap");
		printf("nextPacketToRead:%d\n",nextPacketToRead );
	}
	//printf("### Finished first For\n");

	while(windowStart<totalNumPackets){
		for(int i=windowStart;
			(i<(windowStart+windowSize)) && (i<totalNumPackets);
			i++){
			if(ackMap.find(i)== ackMap.end()){
				//send(encode(dataMap[nextPacketToSend]));
				if(i==0){
					printf("Send Message #%d  i.e. [send data] %d (%d)\n", i,0,0);
				}
				else{
					printf("Send Message #%d  i.e. [send data] %d (%d)\n", i,dataMap[i]->start,dataMap[i]->size-8);
				}
				
				sendMessage((const char*) dataMap[i]->buffer,dataMap[i]->size);
			}
		}
		printf("  ackMap.find(windowStart)!= ackMap.end():%d\n",(ackMap.find(windowStart)!= ackMap.end()));
		printf("  WindowStart:%d\n", windowStart);

		//nanosleep((const struct timespec[]){{0, 500000000L}}, NULL);
		nanosleep((const struct timespec[]){{0, 5000000L}}, NULL);

		while(ackMap.find(windowStart)!= ackMap.end() &&
			  (windowStart<totalNumPackets)  ){
			
			if(nextPacketToRead<totalNumPackets){
				addNextPacketToDataMap(nextPacketToRead);
			}
			printf("windowStart:%d\n", windowStart);
			printf("nextPacketToRead:%d\n",nextPacketToRead);
			printf("ackMap.find(windowStart)!= ackMap.end():%d\n",(ackMap.find(windowStart)!= ackMap.end()));
			ackMap.erase(windowStart);
			dataMap.erase(windowStart);
			windowStart++;
			nextPacketToRead++;
		}
	}
	
	
	printf("SendThreadEnd\n");
}

//Helper functions
void addNextPacketToDataMap(unsigned int index){
	printf("addNextPacket_Index:%d\n", index);
	if(index==0){
		//add the filenameBuffer to the map
		printf("* add Title\n");
		char *filenameBuffer = new char[strlen(filename)+8];
		printf("filenameBufferLen:%d\n",strlen(filename)+8);
		printf("filename:%s\n", filename);
		makeBuffer(FILENAME,index,filename,strlen(filename),filenameBuffer);
		addInfoToDataMap(index,filenameBuffer,strlen(filename)+8, 0);
		//free(filenameBuffer);
		//displayMap(dataMap,"DataMapContents");
	}
	else if(index==totalNumPackets-1){
		printf("* add file end\n");
		char *endBuffer= new char[8];
		makeBuffer(FILE_END,index,new char[0],0,endBuffer);
		addInfoToDataMap(index,endBuffer,8,readLocationInFile);
		//free(endBuffer);
	}
	else if(index>=totalNumPackets){

	}
	else{
		printf("* add Data\n");
		char *sendBuffer = (char*) malloc(50000); 
		// Read the Next packet and put it in sendbuffer
		unsigned int startIndex=readLocationInFile;

		int readSize=readNextPacket(sendBuffer);
		char *formattedBuffer=new char[readSize+8];
		makeBuffer(DATA,index,sendBuffer,readSize,formattedBuffer);
		addInfoToDataMap(index,formattedBuffer,readSize+8,startIndex);
		free(sendBuffer);
	}
}
int readNextPacket(char *buffer){
	int readSize=getPacketSize();
	//Read the input file for 'readSize' amount of data and store it in buffer
	readfile(buffer,readSize);
	readLocationInFile+=readSize;
	printf("==============readLocationInFile:%d\n",readLocationInFile);
	return readSize;
}
int getPacketSize(){
	int readSize=0;
	bytesLeftToRead= filesize - readingPositionInTheInputFile;
	if(bytesLeftToRead<packetSize){
		readSize=bytesLeftToRead;
		finishedReading=true;
	}
	else{
		readSize=packetSize;
		readingPositionInTheInputFile+=packetSize;
	}
	return readSize;
}
void addInfoToDataMap(unsigned int seqNum, char *buffer, unsigned int size, unsigned int start){
	std::shared_ptr<PACKET> packet (new PACKET());
	packet->buffer = buffer;
	packet->size = size;
	packet->start=start;
	dataMap.insert(std::make_pair(seqNum, packet));
}

void addInfoToAckMap(unsigned int seqNum, char *buffer, unsigned int size){
	std::shared_ptr<PACKET> packet (new PACKET());
	packet->buffer = buffer;
	packet->size = size;
	ackMap.insert(std::make_pair(seqNum, packet));
}
void makeBuffer(ePacketType type, unsigned int seqNum, char *payload, int payloadSize, char *result){
	memset(result,0,sizeof(char)*(payloadSize+8));
	//ePacketType type=DATA;
	unsigned short net_type=htons(type);
	memcpy(&result[0],&net_type,2);
	unsigned int net_seqNum=htonl(seqNum);
	memcpy(&result[2],&net_seqNum,4);
	printf("net_seqNum:%d\n", net_seqNum);
	//printf("() makeBuffer_Payload:%.20s\n",payload);
	//printf("() makeBuffer_seqNum:%d\n",seqNum);
	memcpy(result+8,(const char*)payload,payloadSize);

	unsigned short checksumValue=checksum(result,payloadSize+8);
	memcpy(result+6,&checksumValue,2);
	
}
unsigned short checksum(char *buf, unsigned short size)
{
    register long sum=0;

    for(int i=0; i<size; i+=sizeof(unsigned short))
    {
        if(i==size-1)
            break;
        sum += *(unsigned short *)buf;
        buf += sizeof(unsigned short);

        if (sum & 0xFFFF0000)
        {
            sum &= 0xFFFF;
            sum++;
        }
    }
    return ~(sum & 0xFFFF);
}

bool isDropPkt(char *buf, unsigned short size)
{
    unsigned short pkt_checksum = *(unsigned short *)(buf + sizeof(char)* 6);
    //unsigned short pkt_checksum = ntohs(*(unsigned short *)(buf + sizeof(char)* 5));
    *(unsigned short *)(buf + sizeof(char)* 6)=0;
    if (pkt_checksum != checksum(buf, size))
          return 1;
    else
          return 0;
}

void displayMap(std::map<unsigned int, std::shared_ptr<PACKET>> map, const char* name){
	typedef std::map<unsigned int, std::shared_ptr<PACKET>>::iterator it_type;
	printf("\n");
	printf("MapName:%s\n", name);
	for(it_type iterator = map.begin(); iterator != map.end(); iterator++) {
	    // iterator->first = key
	    // iterator->second = value
		unsigned int SeqNum=iterator->first;
		char *buffer = iterator->second->buffer;
		unsigned int size = iterator->second->size;
		printf("  Entry(SeqNum):%d\n", SeqNum);
		printf("  Size:%d\n", size);
		printf("  SeqNumFromPayload:%d\n",(unsigned int)(buffer[2]));
		printf("  CheckSumFromPayload:%d\n", (unsigned short)(buffer[6]));
		printf("  BufferFromPayload:%.30s\n\n", buffer+8);
		//printf("  isDropPkt:%d\n",(isDropPkt(buffer,size)));
		

		
		
	}
}

int recv_seqNum;
void threadRecv(){
	int recv_seqNum=0;
	printf("#   Receiving Message\n");
	while(recv_seqNum<totalNumPackets){
		char *recvBuffer = (char*) malloc(50000);
		recvMessage(recvBuffer,3000);
		//printf("#   Received Message #%d\n", recv_seqNum);
		
		unsigned int readSeqNum = atoi(recvBuffer); 
		addInfoToAckMap(readSeqNum,recvBuffer,sizeof(&recvBuffer));
		//unsigned int readSeqNum = (unsigned int) recvBuffer[2]; 
		//unsigned int netSeqNum = (unsigned int) recvBuffer[0];
		//unsigned int readSeqNum = ntohl(netSeqNum); 
		//unsigned int readSeqNum = (unsigned int) hostRecvBuffer; 
		
		//unsigned int readSeqNum = memcpy();
		//displayMap(ackMap,"AckMap");
		//printf("#   ReadSeqNum:%d\n", readSeqNum);
		//printf("#   recv_seqNum:%d\n", recv_seqNum);
		while(ackMap.find(recv_seqNum) != ackMap.end()){
			recv_seqNum++;
		}
		/*if(readSeqNum==recv_seqNum){
			recv_seqNum++;
		}
		else if((char)recvBuffer[0]=='a'){
			recv_seqNum++;//for debugging 
			printf("#   Test Ack Recieved.\n");
		}*/
		//printf("#   WindowStart:%d\n", windowStart);
		//printf("windowStart<totalNumPackets-1:%d \n",(windowStart<totalNumPackets));
	}
	printf("#   Receive Thread End\n");
}
 
///////////////////////////////////////////////////////																//////////////////
//  NETWORK CONNECTION AND INITIALIZATION FUNCTIONS  //																//////////////////
///////////////////////////////////////////////////////																//////////////////
//ZNETINIT
void initConnection(int argc, char** argv){
	handleOptions(argc, argv);
	//print read contents
	printInputContents();
	//setup the socket
	if(setupSocket()==0){printf("ERROR SETTING UP SOCKET\n");exit (1);}
	//setup the server address
	if(setupServerAddress()==0){printf("ERROR SETTING UP ADDRESS\n");exit (1);}
}
void handleOptions(int argc, char** argv){
	char* parseStr;
	char* IPAndPortnum;
	int option = 0;
	while ((option = getopt(argc,argv,"r:f:")) != -1){
		switch (option){
			case 'r': IPAndPortnum = optarg; 
				break;
			case 'f': filename = optarg;
				break;
			default: 
				exit(EXIT_FAILURE);
		}
	}
	parseStr=strtok(IPAndPortnum,":");
	ip=parseStr;
	parseStr=strtok(NULL,":");
	portnum=atoi(parseStr);
	
}

int setupSocket(){
	//setting up the socket
	printf("setting up the socket\n");
	if( (sock=socket(AF_INET, SOCK_DGRAM,0)) < 0 ){
		perror("cannot create socket");
		printf("cannot create socket\n" );
		return 0;
	}
	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family=AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port=htons(0);
	if (bind(sock, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) { 
		perror("bind failed"); 
		printf("%s\n", "bind failed");
		return 0; 

	}
	return 1;
}

int setupServerAddress(){ 
	printf("setting up the server address\n");
	/* fill in the server's address and data */ 
	memset((char*)&servaddr, 0, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_port = htons(portnum); 

	/* put the host's address into the server address structure */ 
	//memcpy((void *)&servaddr.sin_addr, hp->h_addr_list[0], hp->h_length);
	if (inet_aton(ip, &servaddr.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}
	return 1;
}

//////////////////////////																							//////////////////
//  FILE I/O FUNCTIONS  //																							//////////////////
//////////////////////////																							//////////////////
//ZFILEIO
void readfile(char *sendBuffer, unsigned int readSize){
	int readResult;
	unsigned int accu=0;
	while(accu<readSize){
		readResult = fread(sendBuffer+accu,1,readSize,inputFile);
		accu+=readResult;
		//printf("readResult:%d, accu:%d, readSize:%d\n",readResult, accu,readSize);
	}
	if(readResult!=readSize){
		printf("ERROR READING FILE\n");
		exit(1);
	}
}

void sendMessage(const char *my_message, unsigned int messageLength){
	//printf("sending message to %s\n", ip);
	if (sendto(sock, my_message, messageLength/*strlen(my_message)*/, 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) { 
		perror("sendto failed"); 
		printf("sendto failed\n");
		exit(1);
	}
}

void recvMessage(char *my_message, unsigned int messageLength){
	//printf("receiving message from %s\n", ip);
	int slen = sizeof(servaddr);
	int recvlen = recvfrom(sock, my_message, messageLength, 0, (struct sockaddr *)&servaddr, &slen);
	if (recvlen< 0) { 
		perror("recvfrom failed"); 
		printf("recvfrom failed\n");
		exit(1);
	}
	if (recvlen >= 0) {
        my_message[recvlen] = 0;	/* expect a printable string - terminate it */
        //printf("received message: \"%s\"\n", my_message);
    }
}

////////////////////////																							//////////////////
//  HELPER FUNCTIONS  //																							//////////////////
////////////////////////																							//////////////////
//ZHELP
void printInputContents(){
	printf("\nprogram running, the following flags are read:\n");
	printf("filename:%s\n",filename);
	printf("portnum:%d\n",portnum);
	printf("ip:%s\n\n",ip);
}

