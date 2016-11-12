#include <PJON.h>
#include <GameCommUtils.h>
#include <SPI.h>
#include <MFRC522.h>

#define BREADBOARD_LED     3 // LED for testing on bread board only. 

#define POLE_1_LED         4 // Pixel signal
#define POLE_2_LED         5 // Pixel signal
#define POLE_3_LED         6 // Pixel signal
#define POLE_1_RFID_NSS    7 // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to other SS pins
#define POLE_2_RFID_NSS    8 // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to other SS pins
#define POLE_3_RFID_NSS    9 // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to other SS pins

#define RST_PIN           10 // Configurable, see typical pin layout above

#define NUM_RFID_READERS   3

byte ssPins[] = {POLE_1_RFID_NSS, POLE_2_RFID_NSS, POLE_3_RFID_NSS};

MFRC522 mfrc522[NUM_RFID_READERS];   // Create MFRC522 instance.

void setup() {
  // put your setup code here, to run once:
  initCommunications(FISH_SORTING_GAME_NODE);
  setLocalEventHandler(receiveCommEvent);
  setLocalErrorHandler(errorHandler);

  Serial.begin(9600); // Initialize serial communications with the PC
  while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

  SPI.begin();        // Init SPI bus

  //Force all RFID communication pins to HIGH to shield 
  //Might help(?) with inconsistent start up per https://www.sunfounder.com/forum/multiple-sunfounder-rfid-rc522-readers-on-one-arduino-mega/
  pinMode(POLE_1_RFID_NSS,OUTPUT); 
  pinMode(POLE_2_RFID_NSS,OUTPUT); 
  pinMode(POLE_3_RFID_NSS,OUTPUT);
  digitalWrite(POLE_1_RFID_NSS,HIGH); 
  digitalWrite(POLE_2_RFID_NSS,HIGH); 
  digitalWrite(POLE_2_RFID_NSS,HIGH);
  
  //Loop through each reader and initialize
  for (uint8_t reader = 0; reader < NUM_RFID_READERS; reader++) {
    mfrc522[reader].PCD_Init(ssPins[reader], RST_PIN); // Init each MFRC522 card. Common RST pin, unique NSS pins
    //Serial.print(F("Reader ")); Serial.print(reader+1); Serial.print(F(": "));
    //mfrc522[reader].PCD_DumpVersionToSerial();
  }

  //Flash bread board LED to indicate startup completed
  pinMode(BREADBOARD_LED,OUTPUT);
  flashLED(BREADBOARD_LED, 175, 3);
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
    for (uint8_t reader = 0; reader < NUM_RFID_READERS; reader++) {
    // Look for new cards
    if (mfrc522[reader].PICC_IsNewCardPresent() && mfrc522[reader].PICC_ReadCardSerial()) {
      flashLED(BREADBOARD_LED,100, reader+1);
      Serial.print(F("Reader "));
      Serial.print(reader);
      // Show some details of the PICC (that is: the tag/card)
      Serial.print(F(": Card UID:"));
      dump_byte_array(mfrc522[reader].uid.uidByte, mfrc522[reader].uid.size);
      Serial.println();
      Serial.print(F("PICC type: "));
      MFRC522::PICC_Type piccType = mfrc522[reader].PICC_GetType(mfrc522[reader].uid.sak);
      Serial.println(mfrc522[reader].PICC_GetTypeName(piccType));

      // Halt PICC
      mfrc522[reader].PICC_HaltA();
      // Stop encryption on PCD
      mfrc522[reader].PCD_StopCrypto1();
    } //if (mfrc522[reader].PICC_IsNewC
  } //for(uint8_t reader
}

/**
 * Helper routine to dump a byte array as hex values to Serial.
 */
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

void flashLED (int ledPort, int duration, int times) {
  duration = max(1, duration); //do not allow duration < 1
  for (int i = 0; i < times; i++) {
	digitalWrite(ledPort,HIGH);
	delay(duration);
	digitalWrite(ledPort,LOW);
	delay(duration/2);
  }
}