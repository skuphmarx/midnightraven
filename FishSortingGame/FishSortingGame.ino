#include <PJON.h>
#include <GameCommUtils.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define NUM_POLES          3 // Total number of poles in the game
#define NUMPIXELS         30 // Number of LED lights in each pole

#define BREADBOARD_LED     3 // LED for testing on bread board only
#define POLE_1_LED         4 // Pixel signal pin 1
#define POLE_2_LED         5 // Pixel signal pin 2
#define POLE_3_LED         6 // Pixel signal pin 3
#define POLE_1_RFID_NSS    7 // RFID signal pin 1. Only HIGH/LOW required
#define POLE_2_RFID_NSS    8 // RFID signal pin 2. Only HIGH/LOW required
#define POLE_3_RFID_NSS    9 // RFID signal pin 3. Only HIGH/LOW required
#define RST_PIN           10 // RFID reset pin. Configurable but shared by all RFID readers

String color[] = {"r", "g", "b"}; // For testing purposes only. Pole 1 == red, Pole 2 == green, Pole 3 == blue

byte pixelLedPin[] = {POLE_1_LED, POLE_2_LED, POLE_3_LED}; // Map pole to LED pin
byte rfidNssPin[] = {POLE_1_RFID_NSS, POLE_2_RFID_NSS, POLE_3_RFID_NSS}; // Map pole to RFID pin

MFRC522 mfrc522[NUM_POLES];   // One RFID MFRC522 instance per pole
Adafruit_NeoPixel pixelLed[NUM_POLES]; // One Pixel LED instance per pole

void setup() {
  // setup code here runs once
  initCommunications(FISH_SORTING_GAME_NODE);
  setLocalEventHandler(receiveCommEvent);
  setLocalErrorHandler(errorHandler);

  Serial.begin(9600); // Initialize serial communications with the PC
  while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

  //Initialize RFID readers and Pixel LED lights
  SPI.begin();        // Init SPI bus to communicate with RFID cards
  
  //Force all RFID communication pins to HIGH to shield before running PCD_Init() on any of them
  //Might help(?) with inconsistent start up per https://www.sunfounder.com/forum/multiple-sunfounder-rfid-rc522-readers-on-one-arduino-mega/
//   for (uint8_t pole = 0; pole < NUM_POLES; pole++) {
//     pinMode(rfidNssPin[pole], OUTPUT);
//     digitalWrite(rfidNssPin[pole], HIGH);
//   }
  pinMode(POLE_1_RFID_NSS,OUTPUT);
  pinMode(POLE_2_RFID_NSS,OUTPUT);
  pinMode(POLE_3_RFID_NSS,OUTPUT);
  digitalWrite(POLE_1_RFID_NSS,HIGH);
  digitalWrite(POLE_2_RFID_NSS,HIGH);
  digitalWrite(POLE_3_RFID_NSS,HIGH);
  
  //Loop through each pole and initialize reader and pixel
  for (uint8_t pole = 0; pole < NUM_POLES; pole++) {
    mfrc522[pole].PCD_Init(rfidNssPin[pole], RST_PIN); // Init each MFRC522 card. Common RST pin, unique NSS pins
    pixelLed[pole] = Adafruit_NeoPixel(NUMPIXELS, pixelLedPin[pole], NEO_GRB + NEO_KHZ800); // Init each Pixel, unique pins
    pixelLed[pole].begin();
    //Serial.print(F("Reader ")); Serial.print(pole+1); Serial.print(F(": "));
    //mfrc522[pole].PCD_DumpVersionToSerial();
  }

  reset(); // Reset all game data and other state
  
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
  //This should reset self plus all connected game nodes (if any).
   Serial.print("Reset Fish Sorting Game data: ");
   Serial.println(eventData.data);
  for(int pole=0; pole<NUM_POLES; pole++) {
    for(int i=0;i<NUMPIXELS;i++) {
      pixelLed[pole].setPixelColor(i,0);
  	}
  	pixelLed[pole].show();
  }
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
    for (uint8_t pole = 0; pole < NUM_POLES; pole++) {
    // Check each pole for a new card. PICC_IsNewCardPresent() ensures you can leave a card on the pole and it will not re-register
    if (mfrc522[pole].PICC_IsNewCardPresent() && mfrc522[pole].PICC_ReadCardSerial()) {
      lightLED(0,color[pole]);
      flashLED(BREADBOARD_LED, 100, pole+1);
      Serial.print(F("Pole "));
      Serial.print(pole);
      // Show some details of the PICC (that is: the tag/card)
      Serial.print(F(": Card UID:"));
      dump_byte_array(mfrc522[pole].uid.uidByte, mfrc522[pole].uid.size);
      Serial.println();
//       Serial.print(F("Card type: "));
//       MFRC522::PICC_Type piccType = mfrc522[pole].PICC_GetType(mfrc522[pole].uid.sak);
//       Serial.println(mfrc522[pole].PICC_GetTypeName(piccType));

      // Halt PICC
      mfrc522[pole].PICC_HaltA();
      // Stop encryption on PCD
      mfrc522[pole].PCD_StopCrypto1();
    } //if (mfrc522[pole].PICC_IsNewC
  } //for(uint8_t pole
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

void flashLED (int ledPort, int duration, int numFlashes) {
  duration = max(1, duration); //do not allow duration < 1
  for (int i = 0; i < numFlashes; i++) {
	digitalWrite(ledPort,HIGH);
	delay(duration);
	digitalWrite(ledPort,LOW);
	delay(duration/2);
  }
}


void lightLED(int pole, String data) {
//   Serial.print("Light pole ");
//   Serial.print(pole);
  int r = 0, g = 0, b = 0;
  if (data.equals("r")) {
    // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
    r=150;
//     Serial.println(" red");
  } else if (data.equals("g")) {
    // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
    g=150;
//     Serial.println(" green");
  } else if (data.equals("b")) {
    // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
    b=150;
//     Serial.println(" blue");
  }
  for(int i=0;i<NUMPIXELS;i++) {
    pixelLed[pole].setPixelColor(i, pixelLed[pole].Color(r,g,b));
  }
  pixelLed[pole].show();
}