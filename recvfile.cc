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
#include <iostream>
#include <fstream>

#include <pthread.h>
#include <limits>
#include <map>
#include <string>
#include <iterator>
#include <memory>

////////////////////////																							//////////////////
//  GLOBAL VARIABLES  //																							//////////////////
////////////////////////																							//////////////////
enum ePacketType{
	ACK,
	DATA,
	FILENAME,
	FILE_END
};
struct packet{
	char* buffer;
	unsigned int size;
	ePacketType type;
};
typedef struct packet PACKET;

unsigned int NumPkts = std::numeric_limits<int>::max();;
unsigned int NumPktsWritten = 0;
unsigned int NextPacketToWriteToFile;
char* filename = NULL;
std::ofstream file;
bool doneReceiving=false;
std::map<unsigned int, PACKET *> dataMap;

void *threadRecvAndAck(void *arg);
void recvAndAck();
void displayMap(std::map<unsigned int, PACKET *> map, const char* name);
void makeBuffer(ePacketType type, unsigned int seqNum, char *payload, int payloadSize, char* result);
unsigned short checksum(char *buf, unsigned short size);
bool isDropPkt(char *buf, unsigned short size);
void *threadWritefile(void *arg);
void writefile();
void handle_packet();
unsigned short get_port(int argc, char** argv);
unsigned short recvPort;


int main(int argc, char** argv) {
	
	NextPacketToWriteToFile=1; //because the first (Seq# 0) packet is the filename

	//####get input recv port

	recvPort = get_port(argc,argv);  
	//start the recv and write threads
	
	pthread_t *recv = new pthread_t;
	if (pthread_create(recv, NULL, threadRecvAndAck, NULL) != 0)
	{
		perror("Create thread error\n");
		exit(0);
	}
	
	pthread_t *write = new pthread_t;
	if (pthread_create(write, NULL, threadWritefile, NULL) != 0)
	{
		perror("Create thread error\n");
		exit(0);
	}
	
	//sync recv and write threads
	pthread_join(*(pthread_t *)recv,NULL);
	pthread_join(*(pthread_t *)write,NULL);
	
	
	
  return 0;
}



void *threadRecvAndAck(void *arg){
	
	recvAndAck();
	//printf("end receiving###############################\n");
	return 0;
}


void recvAndAck(){
  //####recv packet

	//########initialization
	int recvSize;
	int bytesReceived = 0;
	// allocate a memory buffer in the heap
	unsigned int buf_size = 50000;
	

	// create a socket
	int sock;	//our recv socket
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
	  perror ("opening TCP socket");
	  abort ();
	}

	//setup source address
	struct sockaddr_in srcAddr;
	/* fill in the address of the server socket */
	memset (&srcAddr, 0, sizeof (srcAddr));
	srcAddr.sin_family = AF_INET;
	srcAddr.sin_addr.s_addr = INADDR_ANY;
	srcAddr.sin_port = htons (recvPort); 
	/* bind server socket to the address */
	if (bind(sock, (struct sockaddr *) &srcAddr, sizeof (srcAddr)) < 0)
	{
	  perror("binding socket to address");
	  abort();
	}  

	//####wait to receive

	while(!doneReceiving){
		//set up buffer to receive
		char *buf = (char *)malloc(buf_size);	//1MB size
		if (!buf)
		{
		  perror("failed to allocated buffer");
		  abort();
		}
		sockaddr_in recv_addr;
		socklen_t addrlen = sizeof(recv_addr);
		//finish receiving
		if(NumPktsWritten+1==NumPkts){
			break;
		}
		
		//receive packet
		recvSize=recvfrom(sock, buf, buf_size, 0, (struct sockaddr *) &recv_addr, &addrlen);		
		if(recvSize<= 0 ){
		  perror("Receive error\n");
		  //abort();
		}
	  

		// printf("  SeqNumFromPayload:%d\n",(unsigned int)(buf[2]));
		// unsigned short checksumInput = *(unsigned short *)(buf + sizeof(char)* 6);
		// printf("  CheckSumFromPayload:%d\n", (unsigned short)(buf[6]));
		// printf("  BufferFromPayload:%.20s\n", buf+8);
		
		
		//####check completeness and send back ACK
		//store packet info
		unsigned short typeNet = *(unsigned short *)(buf);
		ePacketType type = (ePacketType)ntohs(typeNet);
		unsigned int seqNumNet = *(unsigned int*)(buf+2);
		unsigned int seqNum = ntohl(seqNumNet);
		char *payload = buf+8;
		unsigned int payloadSize = recvSize-8;
		//checksum
		bool drop = isDropPkt(buf, recvSize);
		// printf("is drop packet %d\n", drop );
		
		
		if(!drop){
			// printf("type: %d, seqNum:%d, payload:%s, payloadSize:%d, checksumInput: %d \n",type,seqNum,payload,payloadSize,checksumInput);
			if(type == FILENAME){
				// set file name so that write thread can open the file and then write later
				char filenameEnd[strlen(".recv")];
				strcpy(filenameEnd,".recv");
				filename = payload;
				strcat(filename,filenameEnd);
				file.open ((const char*)filename, std::ofstream::out | std::ofstream::app);
				PACKET *packet = new PACKET();
				if(dataMap.count(seqNum) == 0)
					printf("[recv data] %d (%d) (in-order) filename:%s  seq:%d \n", bytesReceived, payloadSize, filename,seqNum);
				dataMap[seqNum] =  packet;
				
				//NumPktsWritten++;
				// std::ofstream file ((const char*)payload, std::ofstream::out);
				// file.close();
				
			}
			else if(type == DATA || type == FILE_END){
				// add packet to map
				PACKET *packet = (new PACKET());
				//memcpy(&packet->size, &payloadSize,4);
				
				//memcpy(&packet->type, &type,2);
				packet->size = payloadSize;
				packet->type = type;
				//printf("packet->type: %d ,packet->size: %d  \n", packet->type,packet->size);
				//packet->buffer = (char *)malloc(payloadSize);
				//memcpy(&packet->buffer, (const char*)&payload, payloadSize);
				packet->buffer = payload;
				//printf("packet->type: %d ,packet->size: %d  \n", packet->type,packet->size);
				//printf("packet->buffer:%s \n",packet->buffer);
				//printf("type: %d, seqNum:%d, payload:%s, payloadSize:%d, checksumInput: %d \n",type,seqNum,payload,payloadSize,checksumInput);
				//printf("packet->size:%d\n",packet->size);
				
				//std::shared_ptr<PACKET> newpacket (new PACKET());
				//dataMap.insert(std::make_pair(55, newpacket));
				
				// if(dataMap.count(seqNum) == 0){	//do not have that in datamap
					printf("[recv data] %d (%d) (in-order)  seq:%d \n", bytesReceived, payloadSize,seqNum);
					bytesReceived += payloadSize;
				// }
				
				//if(dataMap.find(seqNum)->second==NULL)
				// printf("seqNum: %d, packet->type: %d , packet->size: %d  ,packet->buffer: %s  \n", seqNum, packet->type,packet->size,packet->buffer);
				dataMap[seqNum] = packet;
				//printf("datamap seqnum is null?:%d\n",dataMap[seqNum]==NULL);
				//displayMap(dataMap,"mapOfMessages");
				
				
				
				
				// printf("added the following message to the map\n");
				// printf("seqNum:%d\n",seqNum);
				// printf("size:%d\n", dataMap[seqNum]->size);
				// printf("type:%d\n", dataMap[seqNum]->type);
				// printf("%s\n", dataMap[seqNum]->buffer); 
				
				
				// std::shared_ptr<PACKET> datapacket = dataMap.find(seqNum)->second;
				// printf("type:%d\n", datapacket->type);
				
				// printf("size:%d\n", dataMap[seqNum]->size);
				// printf("type:%d\n", dataMap[seqNum]->type);
				// printf("buffer:%s\n", dataMap.find(seqNum)->second->buffer); 
				
				//if get the FILE_END packet, get the last sequence number to aid the receive thread to end.
				if(type == FILE_END){
					
					NumPkts = seqNum;
				}
				// displayMap(dataMap,"mapOfMessages");
			}
			else {
				// ERROR: file corruption not detected by checksum.
						// The header might be corrupted
			}
			
		
			//send back ack packet
			// printf("before make buffer\n");
			// //char* ACKBuf;
			// printf("type: %d, seqNum:%d, payload:%s, payloadSize:%d, checksumInput: %d \n",type,seqNum,payload,payloadSize,checksumInput);
			// //makeBuffer(type, seqNum, payload, payloadSize, ACKBuf);	/////////////////////////////////////////////////////////
			// printf("after make buffer\n");
			// if (sendto(sock, buf, strlen(buf), 0, (struct sockaddr *)&recv_addr, addrlen) < 0)
				// perror("sendto");
			
			char* ACKBuf = new char[buf_size];
			sprintf(ACKBuf, "%d", seqNum);
			printf("sending ACK seq=%s \n", ACKBuf);
			if (sendto(sock, ACKBuf, strlen(ACKBuf), 0, (struct sockaddr *)&recv_addr, addrlen) < 0)
				perror("sendto");
			
			
			
		}
		else{
			printf("[recv corrupt packet]\n");
		}
		
		//display datamap after each time of receiving a packet
		// displayMap(dataMap,"mapOfMessages");
	
	}//end while
}//end recvAndAck


void displayMap(std::map<unsigned int, PACKET *> map, const char* name){
    typedef std::map<unsigned int, PACKET *> ::iterator it_type;
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
        //printf("  SeqNumFromPayload:%d\n",(unsigned int)(buffer[2]));
        //printf("  CheckSumFromPayload:%d\n", (unsigned short)(buffer[6]));
        printf("  BufferFromPayload:%.30s\n\n", buffer);
        //printf("  isDropPkt:%d\n",(isDropPkt(buffer,size)));
        

        
        
    }
}



void makeBuffer(ePacketType type, unsigned int seqNum, char *payload, int payloadSize, char* result){
	//make a buffer to store the info to send out.
    // result=new char[payloadSize+8];
    memset(result,0,sizeof(char)*(payloadSize+8));
    //ePacketType type=DATA;
    memcpy(&result[0],&type,2);
    memcpy(&result[2],&seqNum,4);
    memcpy(result+8,(const char*)payload,payloadSize);
    unsigned short checksumValue=checksum(result,payloadSize+8);
    memcpy(result+6,&checksumValue,2);
	// printf("make buffer: type: %d, seqNum:%d, payload:%s, payloadSize:%d, checksumValue: %d \n",type,seqNum,payload,payloadSize,checksumValue);
    // printf("seqnum in buffer: %d\n",(unsigned int)result[2]);
}

unsigned short checksum(char *buffer, unsigned short size)
{
    register long sum=0;

    for(int i=0; i<size; i+=sizeof(unsigned short))
    {
        if(i==size-1)
            break;
        sum += *(unsigned short *)buffer;
        buffer += sizeof(unsigned short);

        if (sum & 0xFFFF0000)
        {
            sum &= 0xFFFF;
            sum++;
        }
    }
    return ~(sum & 0xFFFF);
}

bool isDropPkt(char *buffer, unsigned short size)
{
    unsigned short pkt_checksum = *(unsigned short *)(buffer + sizeof(char)* 6);
	// printf("pkt_checksum:%d\n",pkt_checksum);
    //unsigned short pkt_checksum = ntohs(*(unsigned short *)(buffer + sizeof(char)* 5));
    *(unsigned short *)(buffer + sizeof(char)* 6)=0;
    if (pkt_checksum != checksum(buffer, size))
          return 1;
    else
          return 0;
}


void *threadWritefile(void *arg){
	// printf("in thread write\n");
	//while not done receiving and writing
	while(!doneReceiving){
		//if we already get the filename packet, start trying to write
		if(filename!=NULL){
			writefile();
		}
	}
	file.close();
	// printf("end writing################################\n");
	printf("[Completed] \n");
	return 0;
}


void writefile(){
	// file.open ((const char*)filename, std::ofstream::out | std::ofstream::app);
	//go over the datamap in sequence to write packets to file in order.
	
	PACKET * nextPacket = dataMap[NextPacketToWriteToFile];
	// printf("next packet is null? %d\n", nextPacket==NULL);
	if(NextPacketToWriteToFile==0 ){
		NextPacketToWriteToFile++;
		return ;
	}
	
	if(nextPacket!=NULL){
		// printf("writing next packet \n");
		if(nextPacket->type==DATA){
			//write packet to file
			file.write (nextPacket->buffer, nextPacket->size);
			// printf("nextPacket->size: %d \n",nextPacket->size);
			// printf("NextPacketToWriteToFile %d",NextPacketToWriteToFile);
			free(dataMap[NextPacketToWriteToFile]->buffer);
			dataMap.erase(NextPacketToWriteToFile);
		}
		else if(nextPacket->type==FILE_END){
			//finish writing and receiving
			doneReceiving=true;
			free(dataMap[NextPacketToWriteToFile]->buffer);
			dataMap.erase(NextPacketToWriteToFile);
		}
		else{
			//ERROR, INAPPROPRIATE PACKETS ARE STORED IN THE DATA MAP
		}
		NumPktsWritten++;
		NextPacketToWriteToFile++;
		// printf("NumPktsWritten:%d, NumPkts:%d  in write thread\n",NumPktsWritten,NumPkts);
	}
	// file.close();
}



  //handle the received packet
void handle_packet(){}

unsigned short get_port(int argc, char** argv){	  
  int c;
  unsigned short recvPort;
  while ((c = getopt (argc, argv, "p:")) != -1)
	switch (c)
	  {
	  case 'p':
		recvPort =atoi(optarg);
		break;
	  default:
		abort ();
	  }  
	// printf("recvPort: %d\n",recvPort);
	return recvPort;
	
}