#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <map>
#include <string>
#include <iterator>
#include <iostream>
#include <memory>

struct packet{
	char* buffer;
	int size;
};
 typedef struct packet PACKET;

//FUNCTION DECLARATIONS
void displayMap(std::map<unsigned int, std::shared_ptr<PACKET>> map, const char* name);
std::map<unsigned int, std::shared_ptr<PACKET>> mapOfMessages;


//MAIN CODE
int main(){
	
	
	for(unsigned int testint=1; testint<3;testint++ ){
		std::shared_ptr<PACKET> packet (new PACKET());

		packet->buffer = (char*) "First Message";
		packet->size = sizeof(packet->buffer);

		
		
		mapOfMessages.insert(std::make_pair(testint, packet));
		printf("added the following message to the map:\n");
		printf("%s\n", mapOfMessages[testint]->buffer); 
		printf("size:%d\n", mapOfMessages[testint]->size);
		
		displayMap(mapOfMessages,"mapOfMessages");
	}
	

	mapOfMessages.erase(1);

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
		printf("  Buffer:%s\n", buffer);
		
	}
}
