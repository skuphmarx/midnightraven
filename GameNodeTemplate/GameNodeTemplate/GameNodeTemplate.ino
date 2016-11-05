#include <PJON.h>
#include <GameCommUtils.h>

//THIS WILL NORMALLY BE DEFINED IN GameCommUtils.h
//Defined here as an example only
#define THIS_NODE_NUM 20

void setup() {
  // put your setup code here, to run once:
  initCommunications(THIS_NODE_NUM);
  setLocalEventHandler(receiveCommEvent);
  setLocalErrorHandler(errorHandler);
}

void receiveCommEvent() {
  //use eventData.whatever to get what was sent and switch
  switch (eventData.event) {
    case RESET_EVENT:
      reset();
      break;
    default:
      ;
  }
}

void reset() {
  //put reset code here. This should reset self plus all connected game nodes.
}

//Sample Error Handler
void errorHandler(uint8_t code, uint8_t data) {
  if(code == CONNECTION_LOST) {
    Serial.print("Connection with device ID ");
    Serial.print(data);
    Serial.println(" is lost.");
  } else if(code == PACKETS_BUFFER_FULL) {
    Serial.print("Packet buffer is full, has now a length of ");
    Serial.println(data, DEC);
    Serial.println("Possible wrong bus configuration!");
    Serial.println("higher MAX_PACKETS in PJON.h if necessary.");
  } else if(code == CONTENT_TOO_LONG) {
    Serial.print("Content is too long, length: ");
    Serial.println(data);
  } else {
    Serial.print("Unknown error code received: ");
    Serial.println(code);
    Serial.print("With data: ");
    Serial.println(data);
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  doComm();
}
