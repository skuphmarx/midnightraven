
#include <GameCommUtils.h>
#include <SPI.h>
#include <RFID.h>

 //
 // This is the main GameController node. it will have an RFID reader attached to it and will read various cards and send
 // events.
 // CARDS include:
 // RESET GAME - Will send a message to ALL nodes to reset. Will expect a comm back from each node with success
 // RESET NODE X - There will be a card for each node that, when read, will send a RESET ONLY to that node and expect a success
 // START GAME - Will put this node into an active state and will trigger any starting events that need to being. 
 //              E.G. telling the first puzzle to begin

 // Tag definitions
static byte START_GAME[5] = {43,143,6,197,103};          // START GAME
static byte RESET_GAME[5] =  {203,251,253,196,9};          // RESET GAME
static byte RESET_DOOR_KNOCKER_NODE[5] ={59,144,6,197,104};                // RESET Door Knocker Node
static byte RESET_AND_START_DOOR_KNOCKER_NODE[5] ={123,232,6,197,80};      // Reset and start Door knocker node
static byte RESET_MP3_PLAYER_NODE[5]=  {235,156,32,197,146};                    // RESET MP3_PLAYER_NODE
static byte RESET_AND_START_MP3_PLAYER_NODE[5]=  {27,152,6,197,64};          // RESET and start MP3_PLAYER_NODE
static byte RESET_MASTER_MIND_POT_GAME_NODE[5] = {5,5,5,5,5};           // RESET MASTER_MIND_POT_GAME_NODE
static byte RESET_AND_START_MASTER_MIND_POT_GAME_NODE[5] = {5,5,5,5,5}; // RESET and start MASTER_MIND_POT_GAME_NODE
static byte RESET_FISH_SORTING_GAME_NODE[5] = {4,4,4,4,5};              // RESET FISH_SORTING_GAME_NODE
static byte RESET_AND_START_FISH_SORTING_GAME_NODE[5]=  {4,4,4,4,5};    // RESET and start FISH_SORTING_GAME_NODE
static byte RESET_DOCK_PLANKS_GAME_NODE[5] = {4,4,4,4,5};               // RESET DOCK_PLANKS_GAME_NODE
static byte RESET_AND_START_DOCK_PLANKS_GAME_NODE[5] = {4,4,4,4,5};     // RESET and start DOCK_PLANKS_GAME_NODE
static byte RESET_LICENSE_PLATE_GAME_NODE[5] = {4,4,4,4,5};             // RESET LICENSE_PLATE_GAME_NODE
static byte RESET_AND_START_LICENSE_PLATE_GAME_NODE[5] = {4,4,4,4,5};   // RESET and startLICENSE_PLATE_GAME_NODE
static byte RESET_HELP_RADIO_NODE[5] = {4,4,4,4,5};                     // RESET HELP_RADIO_NODE
static byte RESET_AND_START_HELP_RADIO_NODE[5]=  {4,4,4,4,5};           // RESET and start HELP_RADIO_NODE

byte lastCardRead[5] = {1,1,1,1,1};
unsigned long lastTimeRead;
unsigned long delayBetweenSameCardRead = 10000;  // 10 seconds

/* RFID Related setup
  * 3V -> VCC  NOTE 3V NOT 5V  RED
  * GND -> GND                 BLACK
  * Pin 10 -> NSS              BLUE
  * Pin 11 -> MOSI / ICSP-4    BROWN
  * Pin 12 -> MISO / ICSP-1    YELLOW
  * Pin 13 -> SCK / ISCP-3     ORANGE
  * Pin 8 -> RST               GREEN
   */
 #define SS_PIN 10  
 #define RST_PIN 8 // Was 5  
 RFID rfid(SS_PIN,RST_PIN); 

#define MOCK_EVENT false     // TURN OFF TO SEND REAL EVENTS

#define debugOn 1

void setup() {
     initCommunications(GAME_CONTROLLER_NODE);
      
      Serial.begin(9600);  
      setLocalEventHandler(localGameEventOccurred);

      /* RFID setup */
      SPI.begin();
      rfid.init();

      resetControllerNode();

      Serial.println("NODE READY");
}

void resetControllerNode() {
     Serial.println("Restting Game Controller");

    lastTimeRead = 0;
    resetAllNodes();
}

 void localGameEventOccurred() {
      debug("GOT GAME EVENT");
        if(eventData.event == CE_RESET_PUZZLE ) {
           resetControllerNode();
        } 
    }
//
// Tell all the nodes to reset
void resetAllNodes() {
   performSend(0, CE_RESET_PUZZLE,"");  //PJON_BROADCAST=0 but for some reason not defined here
 }

void loop() {

   doComm();

   checkForTag();

}

// See if one of the known tags is detected and if so trigger the appropriate event
void checkForTag() {
   if(tagPresent()) {
    if(isDesiredTag(RESET_GAME)) {
      resetAllNodes();
    } else if(isDesiredTag(START_GAME)) {
     // What do we need to do to start here?
    } else if(isDesiredTag(RESET_DOOR_KNOCKER_NODE)) {
       performSend(DOOR_KNOCKER_NODE,CE_RESET_PUZZLE,"");
    } else if(isDesiredTag(RESET_AND_START_DOOR_KNOCKER_NODE)) {
       performSend(DOOR_KNOCKER_NODE,CE_RESET_AND_START_PUZZLE,"");
    }
   } // End tag is present
}



 /*********************************************************
     * Check for any RFID reads
     */
 bool tagPresent() {
  bool ret = false;
  // TODO - If we are shown the ID then play the proper track. Puts us in game state 3
   if(rfid.isCard()){  
     if(rfid.readCardSerial()) {  
       ret = true;
     } // End if readCardSerial
   } // End if isCard
   return ret;
   
 } // End checkforRFID

void setLastCardRead() {
  for(int i=0;i<5;i++) {
     lastCardRead[i] = rfid.serNum[i];
  }
  lastTimeRead = millis();
}

   /*******************************************************
     * See if the indicated card is present on the reader
     */
bool isDesiredTag(byte item[]) {
          bool ret = false;
   
 
   
         if(rfid.serNum[0] == item[0] &&
            rfid.serNum[1] == item[1] &&
            rfid.serNum[2] == item[2] &&
            rfid.serNum[3] == item[3] &&
            rfid.serNum[4] == item[4]) {
              // Is this the same one as last time?
              if(!isSameAsLastCard()) {
                ret = true; 
                setLastCardRead();
                     Serial.print("RFID Tag number: ");
                     Serial.print(rfid.serNum[0]);
                     Serial.print(" ");
                     Serial.print(rfid.serNum[1]);
                     Serial.print(" ");
                     Serial.print(rfid.serNum[2]);
                     Serial.print(" ");
                     Serial.print(rfid.serNum[3]);
                     Serial.print(" ");
                     Serial.println(rfid.serNum[4]);
              } 
         }
   
       return ret;
} // End checkWhichCard 

bool isSameAsLastCard() {
      bool ret = false;
      if(rfid.serNum[0] == lastCardRead[0] &&
            rfid.serNum[1] == lastCardRead[1] &&
            rfid.serNum[2] == lastCardRead[2] &&
            rfid.serNum[3] == lastCardRead[3] &&
            rfid.serNum[4] == lastCardRead[4]) {
              // Has delay passed where it's okay to read again?
              if(millis() - lastTimeRead < delayBetweenSameCardRead) {
                 ret = true;
              } else {
                 lastTimeRead = millis();
              }
       }  // End card matches

       return ret;

}


void performSend(int node, int event, String data) {

  if(MOCK_EVENT) {
    Serial.print("MOCK EVENT: ");
    Serial.print("node=");
    Serial.print(node);
    Serial.print("   event=");
    Serial.println(event);
  } else {
    sendEventToNode(node,event,data);
  }
}

  void debug(String msg) {
   if(debugOn) {
      Serial.println(msg); 
   } 
  }

  



