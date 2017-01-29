
#ifndef GameCommUtils_h
#define GameCommUtils_h

// PJON 5.2 does not have this properly included.
#include <SoftwareSerial.h>
#include <PJON.h>

using namespace std;

// Uncomment for comm utils serial prints.
#define DO_COMM_UTILS_DEBUG

#define COMM_PIN 3

#define GAME_CONTROLLER_NODE 10         // This is the node controlling what is going on
#define MP3_PLAYER_NODE 11              // This is the old man talking
#define FISH_SORTING_GAME_NODE 20       // Fish On Posts
#define MASTER_MIND_POT_GAME_NODE 22    // FlowerPot Game

#define DOOR_KNOCKER 13

// EVENTS - Values from 10-99 Only
#define COMM_EVENT_PING                 50
#define COMM_EVENT_PONG                 51
#define COMM_EVENT_PUZZLE_COMPLETED     80
#define COMM_EVENT_PLAY_TRACK           70
#define COMM_EVENT_KNOCK_OCCURRED       81
#define COMM_EVENT_RESET_EVENT          99

// Keeping these for backward compatibility until code is updated
#define PUZZLE_COMPLETED     COMM_EVENT_PUZZLE_COMPLETED
#define PLAY_TRACK           COMM_EVENT_PLAY_TRACK
#define KNOCK_OCCURRED       COMM_EVENT_KNOCK_OCCURRED
#define RESET_EVENT          COMM_EVENT_RESET_EVENT
//////////////////////////////////////


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

int thisNode;

eventDataStruct eventData;     // This will be filled after an event is received

// PJON object
PJON<SoftwareBitBang> bus;

//void eventReceivedFromController(uint8_t *payload, uint8_t length, const PacketInfo &packet_info) {
// PJON 6.2
void eventReceivedFromControllerOld(uint8_t *payload, uint16_t length, const PacketInfo &packet_info) {

  Serial.print(F("EVENT RECEIVED length="));
  Serial.print(length);
  Serial.print(F(", SenderID: "));
  Serial.println(packet_info.sender_id);

// Serial.print(" Content: ");
//  for(uint8_t i = 0; i < length; i++)
//    Serial.print((char)payload[i]);

  if (length >= 2) {

    eventData.sentFrom=packet_info.sender_id;

    EventIdbuffer[0] = (char)payload[0];
    EventIdbuffer[1] = (char)payload[1];
    eventData.event = atoi(EventIdbuffer);

    if (length <= MAX_EVENT_DATA+2) {
      for(uint8_t i = 2; i < length; i++)  {
        EventDataBuffer[i-2] = (char)payload[i];
      }
      EventDataBuffer[length-2] = 0;
      strcpy(eventData.data, EventDataBuffer);

      gameEventOccurred();


    } else {
      Serial.print(F("Err: max event data size exceeded: "));
      Serial.println(length-2);
    }

  }

}

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
      EventDataBuffer[i-3] = (char)payload[i];
    }
    EventDataBuffer[length-3] = 0;
    strcpy(eventData.data, EventDataBuffer);

    gameEventOccurred();


  } else {
    Serial.print(F("--RECV got junk of length: "));
    Serial.println(length);
  }
}


void doComm() {
  bus.update();
  bus.receive(1000);
}

void processSend() {
  bus.update();
}

/*
 *
 * Send an event to a node
 *
*/
void sendEventToNodeOld(int nodeId, int eventId, String gameData) {

      String theData = eventId + gameData;

      const char* toSend =  theData.c_str();

      Serial.print(F("Sending comm data. NodeId: "));
      Serial.print(nodeId);
      Serial.print(F(" Length: "));
      Serial.println(strlen(toSend));

      bus.send(nodeId, toSend, strlen(toSend));

}

void sendEventToNode(int nodeId, int eventId, String gameData) {

  Serial.print(F("++INPUT: "));
  Serial.println(gameData);


  // First character is the Game Start Character
  // Character 2 and 3 are event ID. To make it easy only support event ID values
  // of 10 - 99.
  // Characters 4-20 are data.
  if (eventId >= 10 && eventId <= 99 && gameData.length() <= (MAX_EVENT_DATA-3) ) {
    sendBuffer[0] = GAME_START_PACKET_CHAR;
    itoa(eventId, &sendBuffer[1], 10);
    memcpy(&sendBuffer[3], gameData.c_str(), gameData.length());


//    String evIdStr = new String(itoa(eventId));
//    uint8_t dataLen = dataStr.length();
//    memcpy(&sendBuffer[1], &theData.c_str(), dataLen);

//    String theData = GAME_START_PACKET_CHAR + eventId + gameData;
//    uint8_t dataLen = theData.length();
//    strncpy(sendBuffer, theData.c_str(), dataLen);
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
  }
}


/**
 * Send an event to the game controller
 */
void sendEventToController(int eventId, String gameData) {
  sendEventToNode(GAME_CONTROLLER_NODE,eventId, gameData);
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
 * Use initCommunications() if you are fine using the default COMM pin which is 12
 * Some shields (like the music maker shield) use PIN 12 for other things in which case that node 
 * should use the initOverrideComm() and pass in the pin that IT wants to use. All nodes do not need
 * to use the same pin fortunately.
 */

void initOverrideComm(int nodeAddress, int commAddress) {

  // Initialize the pad buffer.
  for (byte ii=0; ii<MAX_EVENT_DATA; ii++) {
    padBuffer[ii] = ' ';
  }

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


//GameCommUtils_h
#endif

