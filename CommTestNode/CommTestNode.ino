/**
 * Sketch to test GameCommUtils communication with another game node.
 */

#include <GameCommUtils.h>

// PIN for comm
#define COMM_PIN_NUMBER 12
//#define COMM_PIN_NUMBER 4

// Which Node is this App Mimicing
#define COMM_TEST_NODE GAME_CONTROLLER_NODE

// Which node are we communicating with
int targetGameNode = MASTER_MIND_POT_GAME_NODE;

// Ping/Pong Heartbeat ******************
#define HEARTBEAT_RATE_MILLIS 10000

long lastHeartbeatPing = 0;
int pingSendCount = 0;
String PING_STR = "Ping ";
bool shouldSendHeartbeat = false;

// **************************************

String inSerialMsg = "";



void setup() {

  // Initialize serial communications with the PC
  Serial.begin(9600); 
  while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

  Serial.println(F("Setup started."));

  // Initialize game event communications
  initializeGameEventComm(); 

  Serial.println(F("Comm initialized."));
  Serial.println(F("Setup finished."));
  Serial.println(F("You can ask for help."));

}

void loop() {

  doComm();

  doSerialInput();

  processInputCommand();

  if (shouldSendHeartbeat) {
    sendHeartbeatPing();
  }

}

void initializeGameEventComm() {

  Serial.print(F("Initializing comm node: "));
  Serial.print(COMM_TEST_NODE);
  Serial.print(F(", Using Pin: "));
  Serial.println(COMM_PIN_NUMBER);

  //setCommUtilsReceiveTimeout(3000);
  
  initOverrideComm(COMM_TEST_NODE, COMM_PIN_NUMBER);
  
  setLocalEventHandler(receiveCommEvent);
  setLocalErrorHandler(errorHandler);
  
}


void receiveCommEvent() {
  //use eventData.whatever to get what was sent and switch
  switch (eventData.event) {
    case CE_RESET_PUZZLE:
      Serial.print(F("Received RESET event."));
      break;
    case CE_PONG:
      Serial.print(F("Received PONG event."));
      break;
    case CE_PING:
      Serial.print(F("Received PING event: "));
      Serial.println(eventData.data);
      sendEventToNode(targetGameNode, CE_PONG, "pong");
      Serial.print(F("Send PONG event."));
      break;
    case CE_PLAY_TRACK:
      Serial.print(F("Received Play Audio Track event: "));
      Serial.print(eventData.data);
      respondAckToSender();
      break;
    case CE_PUZZLE_COMPLETED:
      Serial.print(F("Received puzzle complete event: "));
      Serial.print(eventData.data);
      respondAckToSender();
      break;
    case CE_ACK:
      Serial.print(F("Received ACK."));
      break;
    default:
      Serial.print(F("Received something unknown at this point."));
  }

  Serial.println();
  
}

//Sample Error Handler
void errorHandler(uint8_t code, uint8_t data) {
  if(code == CONNECTION_LOST) {
    Serial.print(F("CT Connection with device ID is lost: "));
    Serial.println(data);
  } else if(code == PACKETS_BUFFER_FULL) {
    Serial.print(F("CT Packet buffer is full, has now a length of "));
    Serial.println(data, DEC);
  } else if(code == CONTENT_TOO_LONG) {
    Serial.print(F("CT Content is too long, length: "));
    Serial.println(data);
  } else {
    Serial.print(F("CT Unknown error code received: "));
    Serial.println(code);
    Serial.print(F("With data: "));
    Serial.println(data);
  }
}

/**
 * 
 */
 void doSerialInput() {
   bool looping = true;
   while (looping) {
     looping = false;
     while (Serial.available() > 0) {
       inSerialMsg += (char) Serial.read();
       looping = true;
     }
     if (looping) {
       delay(10);
     }
   }
 }

void processInputCommand() {

  if (inSerialMsg.length() >= 4) {
    
    if (inSerialMsg.equals("help")) {
      doHelp();
    } else if (inSerialMsg.startsWith("ping")) {
      shouldSendHeartbeat = shouldSendHeartbeat ? false : true;
      Serial.print(F("Pinging is now "));
      Serial.println(shouldSendHeartbeat ? F("on") : F("off"));
    } else if (inSerialMsg.startsWith("start")) {
      doSendStart();
    } else if (inSerialMsg.startsWith("reset")) {
      doSendReset();
    } else {
      Serial.print(F("Unrecognized command: "));
      Serial.println(inSerialMsg);
    }
     inSerialMsg = "";
  
  }

}


void doHelp() {

   Serial.println(F("You can send the following commands:"));
   Serial.println(F("  help -  Get this help."));
   Serial.println(F("  ping -  Toggle ping sends on or off."));
   Serial.println(F("  start - Send a start event to the node."));
   Serial.println(F("  reset - Send a reset event to the node."));
   
}

void doSendStart() {
  Serial.println(F("Sending start."));
  sendEventToNode(targetGameNode, CE_START_PUZZLE, "");
}

void doSendReset() {
  Serial.println(F("Sending start."));
  sendEventToNode(targetGameNode, CE_RESET_PUZZLE, "");
}

void sendHeartbeatPing() {
  if ((millis() - lastHeartbeatPing) > HEARTBEAT_RATE_MILLIS) {
    pingSendCount++;
    Serial.print(F("Sending ping "));
    Serial.println(pingSendCount);
    String s = PING_STR + pingSendCount;
    sendEventToNode(targetGameNode, CE_PING, s);
    lastHeartbeatPing = millis(); 
  }
}


