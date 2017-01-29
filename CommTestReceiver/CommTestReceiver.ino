/**
 * Sketch to test GameCommUtils communication with another game node.
 */

//#include <SoftwareSerial.h>
//#include <PJON.h>
#include <GameCommUtils.h>

// Which Node is this App Mimicing
#define COMM_TEST_RECEIVER MASTER_MIND_POT_GAME_NODE

// PIN for comm
#define COMM_PIN_NUMBER 12

// Uncomment to send pings from this node
//#define DO_SEND_PINGS

// Which node are we communicating with
int targetNode = GAME_CONTROLLER_NODE;

int pingSendCount = 0;
String PING_STR = "Ping ";

// Local method declarations
void dump_byte_array(byte *buffer, byte bufferSize);


String inSerialMsg = "";

long lastHeartbeatPing = 0;


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

#ifdef DO_SEND_PINGS
  sendHeartbeatPing();
#endif

  processSend();

}

void initializeGameEventComm() {

  Serial.print(F("Initializing comm node: "));
  Serial.print(COMM_TEST_RECEIVER);
  Serial.print(F(", Using Pin: "));
  Serial.println(COMM_PIN_NUMBER);
  
  initOverrideComm(COMM_TEST_RECEIVER, COMM_PIN_NUMBER);
  
  setLocalEventHandler(receiveCommEvent);
  setLocalErrorHandler(errorHandler);
  
}


void receiveCommEvent() {
  //use eventData.whatever to get what was sent and switch
  switch (eventData.event) {
    case RESET_EVENT:
      Serial.print(F("**Received RESET event."));
      break;
    case COMM_EVENT_PONG:
      Serial.print(F("++Received PONG event."));
      break;
    case COMM_EVENT_PING:
      Serial.print(F("--Received PING event: Length: "));
      Serial.print(strlen(eventData.data));
      Serial.print(F(", Data: ["));
      Serial.print(eventData.data);
      Serial.println(F("]"));
      sendEventToNode(targetNode, COMM_EVENT_PONG, "pong");
      //Serial.print(F("--Send PONG event."));
      break;
    default:
      Serial.print(F("??Received something unknown at this point."));
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
    } else if (inSerialMsg.startsWith("send")) {
      doSendComm();
    } else {
      Serial.println("Unrecognized command: " + inSerialMsg);
    }
     inSerialMsg = "";
  
  }

}


void doHelp() {

   Serial.println(F("You can send the following commands:"));
   Serial.println(F("  help - Get this help."));
   Serial.println(F("  send xxxxx - Send the string xxxxx to the comm channel."));
   
}

void doSendComm() {

  Serial.println("Got a send command: " + inSerialMsg);

  sendEventToNode(targetNode, COMM_EVENT_PING, "ping");

}

void sendHeartbeatPing() {
  if ((millis() - lastHeartbeatPing) > 5000) {
    pingSendCount++;
    Serial.print(F("++Sending ping "));
    Serial.println(pingSendCount);
    String s = PING_STR + pingSendCount;
    sendEventToNode(targetNode, COMM_EVENT_PING, s);
    lastHeartbeatPing = millis(); 
  }
}


/**
 * Helper routine to dump a byte array as hex values to Serial.
 */
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i=0; i<bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}




