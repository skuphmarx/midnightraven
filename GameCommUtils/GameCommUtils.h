

using namespace std;

#define COMM_PIN 12

#define GAME_CONTROLLER_NODE 10    // This is the node controlling what is going on
#define MP3_PLAYER_NODE 11         // This is the old man talking
#define FISH_SORTING_GAME_NODE 20  // Fish On Posts

#define DOOR_KNOCKER 13

// EVENTS 
#define PUZZLE_COMPLETED 80
#define PLAY_TRACK 70
#define KNOCK_OCCURRED 81
#define RESET_EVENT 99

int resetCard[5] = {185,233,104,133,189};  // This is the ID(s) of the reset card


// Bus id definition
//uint8_t bus_id[] = {0, 0, 0, 1};
//int packet;


struct eventDataStruct {
		int sentFrom = -1;
		int event = -1;
		char data[20] = "";
};

static void (*gameEventOccurred)(void);

char EventIdbuffer[2];
char EventDataBuffer[20]; // Max send of 20 characters



int thisNode;

eventDataStruct eventData;     // This will be filled after an event is received

// PJON object
PJON<SoftwareBitBang> bus;

void eventReceivedFromController(uint8_t *payload, uint8_t length, const PacketInfo &packet_info) {
   
   Serial.print("EVENT RECEIVED length=");
Serial.println(length);
  

 // Serial.print(" Content: ");
//  for(uint8_t i = 0; i < length; i++) 
//    Serial.print((char)payload[i]);

  
  eventData.sentFrom=packet_info.sender_id;


  EventIdbuffer[0] = (char)payload[0];
  EventIdbuffer[1] = (char)payload[1];
  eventData.event = atoi(EventIdbuffer);
  
   

  for(uint8_t i = 2; i < length; i++)  {
     EventDataBuffer[i-2] = (char)payload[i];
  }
  EventDataBuffer[length-2] = 0;
  strcpy(eventData.data,EventDataBuffer);

  gameEventOccurred();
  
}





void doComm() {
  bus.update();
  bus.receive();
}

void processSend() {
     bus.update();
}

/*
 *
 * Send an event to a node
 *
*/
void sendEventToNode(int nodeId, int eventId, String gameData) {

      String theData = eventId + gameData;

      const char* toSend =  theData.c_str();
	  
      bus.send(nodeId, toSend, strlen(toSend));
}

/*
 *
 * Send an event to the game controller
 *
*/
void sendEventToController(int eventId, String gameData) {

    sendEventToNode(GAME_CONTROLLER_NODE,eventId, gameData);
}



void error_handler(uint8_t code, uint8_t data) {
  if(code == CONNECTION_LOST) {
    Serial.print("Connection with device ID ");
    Serial.print(data);
    Serial.println(" is lost.");
  }
  if(code == PACKETS_BUFFER_FULL) {
    Serial.print("Packet buffer is full, has now a length of ");
    Serial.println(data, DEC);
    Serial.println("Possible wrong bus configuration!");
    Serial.println("higher MAX_PACKETS in PJON.h if necessary.");
  }
  if(code == CONTENT_TOO_LONG) {
    Serial.print("Content is too long, length: ");
    Serial.println(data);
  }
}

void setLocalEventHandler(void (*function)()) {
    gameEventOccurred = function; 
}

void setLocalErrorHandler(void (*function)(uint8_t, uint8_t)) {
  	bus.set_error(function);
}


/*
 *
 * INIT Method
 * Use initCommunications() if you are fine using the default COMM pin which is 12
 * Some shields (like the music maker shield) use PIN 12 for other things in which case that node 
 * should use the initOverrideComm() and pass in the pin that IT wants to use. All nodes do not need
 * to use the same pin fortunately.
 *
*/

void initOverrideComm(int nodeAddress, int commAddress) {
    thisNode = nodeAddress;
    bus.set_id(nodeAddress);
	bus.strategy.set_pin(commAddress);
	bus.begin();
	bus.set_receiver(eventReceivedFromController);
	bus.set_error(error_handler);
	bus.include_sender_info(true);
}

void initCommunications(int nodeAddress) {
    initOverrideComm(nodeAddress, COMM_PIN);
}





