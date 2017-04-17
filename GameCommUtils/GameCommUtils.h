
#ifndef GameCommUtils_h
#define GameCommUtils_h

// PJON 5.2 does not have this properly included.
#include <SoftwareSerial.h>

// Turn on Asynchronous Acknowledgments at the PJON packet level.
#define INCLUDE_ASYNC_ACK true
// Set the number of send attempts in PJON - default in library is 42. NO IT ISN'T. WHERE IS THIS?
//#define MAX_ATTEMPTS 200
#define SWBB_MAX_ATTEMPTS 200
#define PJON_INCLUDE_SWBB
#include <PJON.h>

using namespace std;

// Uncomment for comm utils serial prints.
#define DO_COMM_UTILS_DEBUG

#define COMM_PIN 3

// The various game nodes connected together
#define GAME_CONTROLLER_NODE      10       // This is the node controlling what is going on
#define MP3_PLAYER_NODE           20       // This is the old man talking
#define DOOR_KNOCKER_NODE         30       // Front door knocker to wake the old man
#define MASTER_MIND_POT_GAME_NODE 40       // FlowerPot Game
#define FISH_SORTING_GAME_NODE    50       // Fish On Posts
#define DOCK_PLANKS_GAME_NODE     60       // Dock planks
#define LICENSE_PLATE_GAME_NODE   70	   // License plate game. Duh.
#define HELP_RADIO_NODE           80       // The help radio system. 

#define COMM_TEST_NODE            94       // Just a node for testing


// COMM_EVENTS (CE) - Values from 10-99 Only
// Events < 50 are typically sent form the CONTROLLER to the PUZZLE. Add 50 to the event to get the event that goes in the opposite direction
// Events > 50 are typically sent from a PUZZLE to the CONTROLLER  
#define CE_START_PUZZLE          10
#define CE_START_SUB_PUZZLE      15      // The Front Door Knocker intervenes between other puzzles
#define CE_PUZZLE_START_SUCCESS  60      // Event passed from PUZZLE to Game Master

#define CE_RESET_NODE			 11
#define CE_NODE_RESET_SUCCESS    61      // Event passed from PUZZLE to Game Master

#define CE_RESET_AND_START_PUZZLE  12
#define CE_RESET_AND_START_SUCCESS 62

#define CE_REQUEST_PUZZLE_STATUS 20
#define CE_PUZZLE_NOT_STARTED    70
#define CE_PUZZLE_IN_PROGRESS    71
#define CE_PUZZLE_COMPLETED      72
#define CE_PUZZLE_COMPLETED_SUCCESS 62   // Event passed from Game Controller to Puzzle in response to puzzle complete

// Health related events
#define CE_PING                 50
#define CE_PONG                 51
#define CE_ACK                  55

// Common events that the MP3 will receive from other games.
#define CE_PLAY_TRACK           80     // DATA: The Track NUMBER to play
#define CE_PLAY_TRACK_SUCCESS   81


// Keeping these for backward compatibility until code is updated
//#define PUZZLE_COMPLETED     CE_PUZZLE_COMPLETED
//#define PLAY_TRACK           CE_PLAY_TRACK
//#define KNOCK_OCCURRED       CE_KNOCK_OCCURRED
//#define RESET_EVENT          CE_RESET_EVENT
//////////////////////////////////////


// This can be changed by a given node as makes sense.
int commUtilsReceiveTimeout = 2000;

int resetCard[5] = {185,233,104,133,189};  // This is the ID(s) of the reset card


// Bus id definition
//uint8_t bus_id[] = {0, 0, 0, 1};
//int packet;

#define MAX_EVENT_DATA 20

struct eventDataStruct {
  int sentFrom = -1;
  int event = -1;
  char data[MAX_EVENT_DATA+1] = "";  // Need the +1 for the string terminator.
};



static void (*gameEventOccurred)(void);

char EventIdbuffer[2];
char EventDataBuffer[MAX_EVENT_DATA+1]; // Max send of 20 characters, Need the +1 for the string terminator.

// First character in each comm packet. Use this to validate that we have an actual beginning of a packet.
const char GAME_START_PACKET_CHAR = '^';

char sendBuffer[MAX_EVENT_DATA];

char padBuffer[MAX_EVENT_DATA];

char intDataBuffer[MAX_EVENT_DATA];

int thisNode;

eventDataStruct eventData;     // This will be filled after an event is received
unsigned int responseWaitTime = 10000;  // We will wait 1 seconds for a response
bool eventSentSuccessfully = false;  // used for events that need a response. START and COMPLETE mostly
int importantEventResponse = 0;  // The important event response we are waiting to receive

// PJON object
PJON<SoftwareBitBang> bus;

// Local funtions
void processSend();
void processReceive();



void eventReceivedFromController(uint8_t *payload, uint16_t length, const PacketInfo &packet_info) {

#ifdef DO_COMM_UTILS_DEBUG
  Serial.print(F("--RECV len: "));
  Serial.print(length);
  if (length > 0) {
    Serial.print(F(", Char0: "));
    Serial.print((char)payload[0]);
  }
  Serial.print(F(", SenderID: "));
  Serial.println(packet_info.sender_id);
#endif

  if (length == MAX_EVENT_DATA && ((char)payload[0]) == GAME_START_PACKET_CHAR) {

    eventData.sentFrom=packet_info.sender_id;

    EventIdbuffer[0] = (char)payload[1];
    EventIdbuffer[1] = (char)payload[2];
    eventData.event = atoi(EventIdbuffer);

    for(uint8_t i=3; i<length; i++)  {
	  if((char)payload[i] == ' ') {
	    EventDataBuffer[i-3] = 0;      // Put a null and break out
        break; 
	  } else {
        EventDataBuffer[i-3] = (char)payload[i];
	  }
    }
    EventDataBuffer[length-3] = 0;      // I think this is unnecessary but doesn't hurt anything
    strcpy(eventData.data, EventDataBuffer);
	
	if(importantEventResponse > 0 && eventData.event == importantEventResponse) {
	     eventSentSuccessfully = true;
	} else {	
       gameEventOccurred();
	}

  } else {
#ifdef DO_COMM_UTILS_DEBUG
    Serial.print(F("--RECV junk, length: "));
    Serial.println(length);
#endif
  }
}


void doComm() {
  processSend();
  processReceive();
}

void processSend() {
  uint8_t numToSend = bus.update();
//#ifdef DO_COMM_UTILS_DEBUG
//  if (numToSend > 0) {
//    Serial.print("~~ProcSend: ");
//    Serial.println(numToSend);
//  }
//#endif
}

void processReceive() {
  uint16_t recvState = bus.receive(commUtilsReceiveTimeout);
//#ifdef DO_COMM_UTILS_DEBUG
//  if (ACK == recvState) {
//    Serial.println(F("**ProcRecv: ACK"));
//  }
//  else if (NAK == recvState) {
//    Serial.println(F("**ProcRecv: NAK"));
//  }
//#endif
}




/**
 * Send an Event to the specified Node.
 */
void sendEventToNode(int nodeId, int eventId, String gameData) {
    


#ifdef DO_COMM_UTILS_DEBUG
//  Serial.print(F("++INPUT: "));
//  Serial.println(gameData);
#endif

  // First character is the Game Comm Packet Start Character
  // Character 2 and 3 are event ID. To make it easy only support event ID values
  // of 10 - 99.
  // Characters 4-20 are data.
  if (eventId >= 10 && eventId <= 99 && gameData.length() <= (MAX_EVENT_DATA-3) ) {
    sendBuffer[0] = GAME_START_PACKET_CHAR;
    itoa(eventId, &sendBuffer[1], 10);
    memcpy(&sendBuffer[3], gameData.c_str(), gameData.length());

    int dataLen = gameData.length() + 3;
    if (dataLen < MAX_EVENT_DATA) {
      memcpy(&sendBuffer[dataLen], padBuffer, MAX_EVENT_DATA - dataLen);
    }

#ifdef DO_COMM_UTILS_DEBUG
    Serial.print(F("++SEND NodeId: "));
    Serial.print(nodeId);
    Serial.print(F(" EventId: "));
    Serial.print(eventId);
    Serial.print(F(" ["));
    for (byte ii=0; ii<MAX_EVENT_DATA; ii++) {
      Serial.print((char) sendBuffer[ii]);
    }
    Serial.println(F("]"));
#endif

    // Always send a full packet - then we know we have always received a full packet.
    bus.send(nodeId, sendBuffer, MAX_EVENT_DATA);
    processSend();
	
  } else {
#ifdef DO_COMM_UTILS_DEBUG
    Serial.println(F("Bad request in commUtils"));
#endif
  }
}


/**
 * Send an event to the game controller
 */
void sendEventToController(int eventId, String gameData) {
  sendEventToNode(GAME_CONTROLLER_NODE,eventId, gameData);
}

/**
 * Send an Event where the data is an integer.
 */
void sendIntEventToNode(int nodeId, int eventId, int intGameData) {
  itoa(intGameData, &intDataBuffer[0], 10);
  sendEventToNode(nodeId, eventId, intDataBuffer);
}

void sendPuzzleCompleteEvent(int fromNode) {
  itoa(fromNode, &intDataBuffer[0], 10);
  sendEventToNode(GAME_CONTROLLER_NODE,CE_PUZZLE_COMPLETED, intDataBuffer);

}

void sendControllerImportantEvent(int theEvent, int responseEvent, int fromNode) {
   unsigned long startTime;
   unsigned int numberOfRetrys = 0;
   
   importantEventResponse = responseEvent;
   
   itoa(fromNode, &intDataBuffer[0], 10);
   eventSentSuccessfully = false;
   while(!eventSentSuccessfully && numberOfRetrys < 5) {
     numberOfRetrys++;
     if(numberOfRetrys > 1) {
        Serial.println(F("RESENDING Important EVENT AS NO RESPONSE RECEIVED"));
     }
     sendEventToNode(GAME_CONTROLLER_NODE,theEvent, intDataBuffer);
     // now wait a specified time for a response
     startTime = millis();
     while(millis()-startTime < responseWaitTime && !eventSentSuccessfully) {
        doComm();
     }
     Serial.println(F("Finished Sending Important Event"));
   }
}

void sendPuzzleStartSuccess() {
     sendEventToNode(GAME_CONTROLLER_NODE,CE_PUZZLE_START_SUCCESS, "");
}



/**
 *
 */
void respondSuccessToSender() {
  int returnEvent = CE_ACK;
 
  
  switch(eventData.event) {
     case CE_PLAY_TRACK:
	   returnEvent = CE_PLAY_TRACK_SUCCESS;
	 break;
  }
  Serial.println(F("Sending Success Response"));
   sendEventToNode(eventData.sentFrom, returnEvent, "");
   processSend();
}

/**
 * PACKETS_BUFFER_FULL - Possible wrong bus configuration. Higher MAX_PACKETS in PJON.h if necessary.
 */
void error_handler(uint8_t code, uint8_t data) {
  if(code == CONNECTION_LOST) {
    Serial.print(F("Connection with device ID "));
    Serial.print(data);
    Serial.println(F(" is lost."));
  }
  if(code == PACKETS_BUFFER_FULL) {
    Serial.print(F("Packet buffer full, length: "));
    Serial.println(data, DEC);
  }
  if(code == CONTENT_TOO_LONG) {
    Serial.print(F("Content too long, length: "));
    Serial.println(data);
  }
}

void setLocalEventHandler(void (*function)()) {
  gameEventOccurred = function;
}

void setLocalErrorHandler(void (*function)(uint8_t, uint8_t)) {
  bus.set_error(function);
}


/**
 * INIT Method
 * Use initCommunications() if you are fine using the default COMM pin which is defined by COMM_PIN.
 * Some shields (like the music maker shield) may use the default COMM_PIN for other things in which
 * case that node should use the initOverrideComm() and pass in the pin that it wants to use.
 * All nodes do not need to use the same pin fortunately.
 */
void initOverrideComm(int nodeAddress, int commAddress) {

  // Initialize the pad buffer.
  for (byte ii=0; ii<MAX_EVENT_DATA; ii++) {
    padBuffer[ii] = ' ';
  }

  thisNode = nodeAddress;
  bus.set_id(nodeAddress);
  bus.strategy.set_pin(commAddress);

// Set the Async ACK flag
#if(INCLUDE_ASYNC_ACK)
  bus.set_asynchronous_acknowledge(true);
#endif

  //bus.set_synchronous_acknowledge(true);

  bus.begin();
  bus.set_receiver(eventReceivedFromController);
  bus.set_error(error_handler);
  bus.include_sender_info(true);

}

void initCommunications(int nodeAddress) {
  initOverrideComm(nodeAddress, COMM_PIN);
}

void setCommUtilsReceiveTimeout(int value) {
  commUtilsReceiveTimeout = value;
}


//GameCommUtils_h
#endif

