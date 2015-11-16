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

/*syntax:  sendfile -r 127.0.0.1:18000 -f testfile.txt*/

////////////////////////																							//////////////////
//  GLOBAL VARIABLES  //																							//////////////////
////////////////////////																							//////////////////
struct packet{
	char* buffer;
	unsigned int size;
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
void sendMessage(char *my_message, unsigned int messageLength);
void readfile(char *sendBuffer, unsigned int readSize);

//Helper
void printInputContents();
 
/////////////////////																								//////////////////
//  MAIN FUNCTION  //																								//////////////////
/////////////////////																								//////////////////
//ZMAIN
int main(int argc, char** argv){
	//initialize the network connection
	initConnection(argc, argv);
	//send the file to the receiver
	//threadSend();
	std::thread first (threadSend);
	std::thread second (threadRecv);
	first.join();
	second.join();

	//closes the file after the file is sent
	fclose(inputFile);
	//terminate the program
	return 0;
}
 
///////////////////////////////////////////																			//////////////////
//              SEND THREAD              //																			//////////////////
///////////////////////////////////////////																			//////////////////
//ZSEND

//Global Variables for this section:
int packetSize = 50000;
int readingPositionInTheInputFile=0;
int bytesLeftToRead;
bool finishedReading=false;
unsigned int filesize;



//Helper functions for this section:
int getPacketSize();
int readNextPacket(char *buffer);
void addInfoToMap(char *buffer, unsigned int size, unsigned int seqNum, std::map<unsigned int, std::shared_ptr<PACKET>> map);
void addPacketToMap(std::shared_ptr<PACKET> packet, unsigned int seqNum, std::map<unsigned int, std::shared_ptr<PACKET>> map);
void displayMap(std::map<unsigned int, std::shared_ptr<PACKET>> map, const char* name);

//Code for this section:
void threadSend(){
	//open the input file
	inputFile = fopen(filename,"r");
	if(inputFile==NULL){
		fputs ("File error",stderr);exit(1);
	}

	//obtain file size:
	fseek(inputFile,0,SEEK_END);
	filesize = ftell(inputFile);
	rewind(inputFile);

	unsigned int seqNum=1;
	int readSize=0;
	while(!finishedReading){
		//make a buffer of .05 MB to send
		char *sendBuffer = (char*) malloc(50000); 
		// Read the Next packet and put it in sendbuffer
		readSize=readNextPacket(sendBuffer);
		addInfoToMap(sendBuffer, readSize, seqNum, dataMap);
		seqNum++;
		//Send the contents of send buffer to the reciever
		sendMessage(sendBuffer,readSize);
		//print the contents for testing
		printf("sendBuffer[0]:%c\n",(char)sendBuffer[0]);
		//free the memory
		free(sendBuffer);
	}
	
}

//Helper functions
int readNextPacket(char *buffer){
	int readSize=getPacketSize();
	//Read the input file for 'readSize' amount of data and store it in buffer
	readfile(buffer,readSize);
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
void addInfoToMap(char *buffer, unsigned int size, unsigned int seqNum,std::map<unsigned int, std::shared_ptr<PACKET>> map){
	std::shared_ptr<PACKET> packet (new PACKET());
	packet->buffer = buffer;
	packet->size = size;
	addPacketToMap(packet,seqNum,map);
}
void addPacketToMap(std::shared_ptr<PACKET> packet, unsigned int seqNum, std::map<unsigned int, std::shared_ptr<PACKET>> map){
	map.insert(std::make_pair(seqNum, packet));
	printf("seqNum:%d\n",seqNum);
	printf("inserted, size:%d\n",map[seqNum]->size);
	//printf("  message:%s\n", map[seqNum]->buffer);
	displayMap(dataMap,"DataMapContents");
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
		printf("  Buffer:%s\n", buffer);
		
	}
}

void threadRecv(){
	printf("threadRecv executed.\n");
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
		printf("readResult:%d, accu:%d, readSize:%d\n",readResult, accu,readSize);
	}
	if(readResult!=readSize){
		printf("ERROR READING FILE\n");
		exit(1);
	}
}

void sendMessage(char *my_message, unsigned int messageLength){
	printf("sending message to %s\n", ip);
	if (sendto(sock, my_message, strlen(my_message), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) { 
		perror("sendto failed"); 
		printf("sendto failed\n");
		exit(1);
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

