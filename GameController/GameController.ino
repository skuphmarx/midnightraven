 #include <PJON.h>
 #include <SPI.h>
#include <MFRC522.h>
 #include <GameCommUtils.h>

 //
 // This is the main GameController node. it will have an RFID reader attached to it and will read various cards and send
 // events.
 // CARDS include:
 // RESET GAME - Will send a message to ALL nodes to reset. Will expect a comm back from each node with success
 // RESET NODE X - There will be a card for each node that, when read, will send a RESET ONLY to that node and expect a success
 // START GAME - Will put this node into an active state and will trigger any starting events that need to being. 
 //              E.G. telling the first puzzle to begin

 // Tag definitions
static byte START_GAME[4] = {1,2,3,4};          // START GAME
static byte RESET_GAME[4] =  {2,2,2,2};          // RESET GAME
static byte RESET_DOOR_KNOCKER_NODE[4] ={3,3,3,3};                // RESET Door Knocker Node
static byte RESET_AND_START_DOOR_KNOCKER_NODE[4] ={3,3,3,3};      // Reset and start Door knocker node
static byte RESET_MP3_PLAYER_NODE=  {4,4,4,4};                    // RESET MP3_PLAYER_NODE
static byte RESET_AND_START_MP3_PLAYER_NODE=  {4,4,4,4};          // RESET and start MP3_PLAYER_NODE
static byte RESET_MASTER_MIND_POT_GAME_NODE  {5,5,5,5};           // RESET MASTER_MIND_POT_GAME_NODE
static byte RESET_AND_START_MASTER_MIND_POT_GAME_NODE  {5,5,5,5}; // RESET and start MASTER_MIND_POT_GAME_NODE
static byte RESET_FISH_SORTING_GAME_NODE  {4,4,4,4};              // RESET FISH_SORTING_GAME_NODE
static byte RESET_AND_START_FISH_SORTING_GAME_NODE  {4,4,4,4};    // RESET and start FISH_SORTING_GAME_NODE
static byte RESET_DOCK_PLANKS_GAME_NODE  {4,4,4,4};               // RESET DOCK_PLANKS_GAME_NODE
static byte RESET_AND_START_DOCK_PLANKS_GAME_NODE  {4,4,4,4};     // RESET and start DOCK_PLANKS_GAME_NODE
static byte RESET_LICENSE_PLATE_GAME_NODE  {4,4,4,4};             // RESET LICENSE_PLATE_GAME_NODE
static byte RESET_AND_START_LICENSE_PLATE_GAME_NODE  {4,4,4,4};   // RESET and startLICENSE_PLATE_GAME_NODE
static byte RESET_HELP_RADIO_NODE  {4,4,4,4};                     // RESET HELP_RADIO_NODE
static byte RESET_AND_START_HELP_RADIO_NODE  {4,4,4,4};           // RESET and start HELP_RADIO_NODE
 

#define debugOn 1

void setup() {
     initCommunications(GAME_CONTROLLER_NODE);
      
      Serial.begin(9600);  
      setLocalEventHandler(localGameEventOccurred);

}

 void localGameEventOccurred() {
      debug("GOT GAME EVENT");
        if(eventData.event == RESET_EVENT ) {
           resetAllNodes();
        } 
    }
//
// Tell all the nodes to reset
void resetAllNodes() {
   Serial.println("GOT RESET EVENT");
   sendEvent(PJON_BROADCAST, CE_RESET_PUZZLE,"");
 }

void loop() {

   doComm();

   checkForTag();

}

// See if one of the known tags is detected and if so trigger the appropriate event
void checkForTag() {
   if(tagPresent(RESET_GAME)) {
      resetAlLNodes();
   } else if(tagPresent(START_GAME)) {
    
   }
}

  void debug(String msg) {
   if(debugOn) {
      Serial.println(msg); 
   } 
  }

  

