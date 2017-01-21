/**
 * Mastermind Flower Pot Game Node.
 */

// Wires to RFID Readers, right to left: red, brown, black, blue, orange/purple, yellow, green/white. Last pin is empty.

// Wire to arduino:
// yellow: 13
// blue:   12
// orange/purple: 11
// brown:  10
// Greens/white: 7, 8, 9
// Black: GND 1 opposite side
// red: 3 Volt


#include <SoftwareSerial.h>

#include <PJON.h>
#include <GameCommUtils.h>
#include <SPI.h>
#include <MFRC522.h>
#include <string.h>

using namespace std;

// Uncomment to enable event communications
//#define DO_GAME_EVENT_COMM

// Uncomment to enable Serial Debug Prints
#define DO_DEBUG

// Uncomment to enable printing game status details
#define DO_GAME_STATUS_DETAILS

// Board locations *******************
  
#define BREADBOARD_LED     3 // LED for testing on bread board only

#define RST_PIN           10 // RFID reset pin. Configurable but shared by all RFID readers

// Reader pin numbers are assumed sequential starting with this value.
// These are the slave select lines of the RFID readers.
#define READER_INITIAL_RFID_NSS_PIN    7
#define NUM_MM_READERS 3

// This is the number of flower pots that are known to the logic
#define NUM_KNOWN_FLOWER_POTS 10

// Active Flower Pots that can be part of a solution.
// Each pot has 2 tags, one is a backup. Only one of the two tags can be
// active in any of the pots at game time.
// The RFID has 4 bytes.
byte tagIdsByFlowerPot[NUM_KNOWN_FLOWER_POTS][2][4] = {
  { {0xC2, 0x66, 0x7F, 0x64}, {0xFF, 0xFF, 0xFF, 0xFF} },   // 1.1, 1.2
  { {0x54, 0x99, 0xDE, 0xFC}, {0xFF, 0xFF, 0xFF, 0xFF} },   // 2.1, 2.2
  { {0xE6, 0x93, 0x2A, 0x12}, {0xFF, 0xFF, 0xFF, 0xFF} },   // 3.1, 3.2
  { {0xD4, 0x5E, 0x15, 0xFC}, {0xFF, 0xFF, 0xFF, 0xFF} },   // 4.1, 4.2
  { {0x52, 0xC1, 0x78, 0x64}, {0xFF, 0xFF, 0xFF, 0xFF} },   // 5.1, 5.2
  { {0xE6, 0xD3, 0x2D, 0x12}, {0xFF, 0xFF, 0xFF, 0xFF} },   // 6.1, 6.2
  { {0xF2, 0x92, 0x83, 0x64}, {0xFF, 0xFF, 0xFF, 0xFF} },   // 7.1, 7.2
  { {0x86, 0xF0, 0x2A, 0x12}, {0xFF, 0xFF, 0xFF, 0xFF} },   // 8.1, 8.2
  { {0x92, 0x0F, 0x43, 0x6D}, {0xFF, 0xFF, 0xFF, 0xFF} },   // 9.1, 9.2
  { {0xF4, 0x68, 0x9B, 0xFC}, {0xFF, 0xFF, 0xFF, 0xFF} }    // 10.1, 10.2
};


// Local method declarations
void flashLED (int ledPort, int duration, int numFlashes);
void dump_byte_array(byte *buffer, byte bufferSize);

/**
 * RFID Tag.
 */
class RFIDTag {

    byte size = 0;
    byte id[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };  // Size matches MFRC522 Uid struct
    bool clearFlag = true;
    
  public:

    RFIDTag() {
      clearId();
    }

    RFIDTag(byte* uidBytes, byte inSize) {
      clearId();
      setId(uidBytes, inSize);
    }

    void clearId() {
      memset(id, 0, 10);
      clearFlag = true;
    }

    void setId(MFRC522::Uid *uid) {
      setId(uid->uidByte, uid->size);
    }

    void setId(byte *uidBytes, byte inSize) {
      memcpy(id, uidBytes, inSize);
      size = inSize;
      clearFlag = false;
    }

    bool isEqualId(MFRC522::Uid *uid) {
      return (size == uid->size) && (memcmp(id, uid->uidByte, size) == 0); 
    }

    bool isIdClear() {
      return clearFlag;
    }

    bool operator==(const RFIDTag& tag) {
      return (this->size == tag.size) &&
             (this->clearFlag == tag.clearFlag) &&
             (memcmp(this->id, tag.id, this->size) == 0);
    }

    void printIdToSerial() {
      for (byte i=0; i<size; i++) {
        Serial.print(id[i] < 0x10 ? " 0" : " ");
        Serial.print(id[i], HEX);
      }
    }
  
};

/**
 * Represents an individual FlowerPot. Contains the Tags that are associated with the flowerpot.
 */
class FlowerPot {

    const static int MAX_TAGS_PER_POT = 2;

    RFIDTag* knownTags[MAX_TAGS_PER_POT];
    int numberKnownTags = 0;

    int id;  // ID of the pot; 1-10 or whatever.

  public:

    FlowerPot( int potId ) {
      id = potId;
    }

    int getId() {
      return id;
    }

    /**
     * Initialize the pot with known tags.
     */
    void addKnownTag(RFIDTag* tag) {
      if (numberKnownTags < MAX_TAGS_PER_POT) {
        knownTags[numberKnownTags] = tag;
        numberKnownTags++;
      } else {
        Serial.println(F("Error: Maximum tags exceeded in FlowerPot.addKnownTag()"));
      }
    }

    /**
     * Returns true if the input tag is one of the tags that is associated with this pot.
     */
    bool isKnownTag(RFIDTag* tag) {
      for (int i=0; i<numberKnownTags; i++) {
        if (*tag == *knownTags[i]) {
          return true;
        }
      }
      return false;
    }
  
};


/**
 * Base Game RFID Reader. Contains non game specific methods and state of an individual RFID reader card.
 */
class GameRFIDReader: public MFRC522 {

    const static byte EXPECTED_FIRMWARE_VERSION = 0x92;

    int pinId;
    byte firmwareVersionValue = 0x00;

  public:

    GameRFIDReader(): MFRC522() {}
    GameRFIDReader(byte resetPowerDownPin): MFRC522(resetPowerDownPin) {}
    GameRFIDReader(byte chipSelectPin, byte resetPowerDownPin): MFRC522(chipSelectPin, resetPowerDownPin) {
      pinId = chipSelectPin;
    }
    
    int getPinId() {
      return pinId;
    }

    bool isTagDataAvailable()
    {
      return PICC_IsNewCardPresent() && PICC_ReadCardSerial();
    }

    /**
     * The logic in here related to firmware version might need to change if new cards are introduced.
     * Might need a list.
     */
    bool isExpectedFirmwareVersion() {
      return firmwareVersionValue == EXPECTED_FIRMWARE_VERSION;
    }

    byte getFirmwareVersionValue() {
      return firmwareVersionValue;
    }

    bool initializeFirmwareCommunication() {

      // Initialize the RFID reader card
      // Retires do not seem to help.
      byte v = 0x00;
      for (byte bb=0; bb<1; bb++) {
        // Maybe help inconsistent startup? Does not seem to help.
        //delay(500);
        
        this->PCD_Init();
        firmwareVersionValue = this->PCD_ReadRegister(this->VersionReg);

        if (isExpectedFirmwareVersion()) {
          break;
        }
        
      }

      // So far have not found a good way to determine if communication is good or bad.
      return true;
    }
  
};


/**
 * Mastermind specific RFID Reader methods and state.
 */
class MasterMindReader: public GameRFIDReader {

    RFIDTag* currentRFIDTag = new RFIDTag();
    unsigned long timeLastChanged = millis();
    unsigned long timeLastUpdated = millis();
    bool currentTagIsCorrect = false;
    FlowerPot* currentFlowerPot = NULL;
  
  public:

    MasterMindReader():GameRFIDReader() {}

    MasterMindReader(byte chipSelectPin, byte resetPowerDownPin): GameRFIDReader(chipSelectPin, resetPowerDownPin) {}

    RFIDTag* getCurrentTag() {
      return currentRFIDTag;
    }

    bool isTagPresent() {
      return !currentRFIDTag->isIdClear();
    }

    bool isTagCorrect() {
      return isTagPresent() && currentTagIsCorrect;
    }

    void setTagCorrect(bool val) {
      currentTagIsCorrect = val;
    }

    /**
     * This method sets the current tag ID if the current tagId is unset or updates the last update time
     * if the provided uid equals the current tag Id. We want only one tag per reader at at time, the first
     * one wins. If this update was a new tag then the true is returned, otherwise false is returned.
     */
    boolean processTagUpdate(MFRC522::Uid *uid) {
      bool updateWasNewTag = false;
      if (currentRFIDTag->isIdClear()) {
        currentRFIDTag->setId(uid);
        timeLastChanged = millis();
        timeLastUpdated = timeLastChanged;
        updateWasNewTag = true;
      } else if (currentRFIDTag->isEqualId(uid)) {
        timeLastUpdated = millis();
      }
      return updateWasNewTag;
    }

    void clearCurrentTag() {
      currentRFIDTag->clearId();
      timeLastChanged = millis();
      timeLastUpdated = timeLastChanged;
      currentTagIsCorrect = false;
      currentFlowerPot = NULL;
    }

    unsigned long getTimeLastChanged() {
      return timeLastChanged;
    }

    unsigned long getTimeLastUpdated() {
      return timeLastUpdated;
    }

    void setCurrentFlowerPot(FlowerPot* p) {
      currentFlowerPot = p;
    }

    FlowerPot* getCurrentFlowerPot() {
      return currentFlowerPot;
    }

};

/**
 * This class contains one of the possible game solutions.
 */
class MasterMindGameSolution {

    const static int MAX_POTS_PER_SOLUTION = 3;

    FlowerPot* solutionPots[MAX_POTS_PER_SOLUTION];
    int numberSolutionPots = 0;

  public:

    MasterMindGameSolution() {
      numberSolutionPots = 0;
    }

    /**
     * Initialize the solutions with the pots in the solution.
     */
    void addSolutionFlowerPot(FlowerPot* p) {
      if (numberSolutionPots < MAX_POTS_PER_SOLUTION) {
        solutionPots[numberSolutionPots] = p;
        numberSolutionPots++;
      } else {
        Serial.println(F("Error: Maximum flowerpots exceeded in MasterMindGameSolution.addSolutionFlowerPot()"));
      }
    }

    FlowerPot** getSolutionFlowerPots() {
      return solutionPots;
    }

    int getNumberSolutionPots() {
      return numberSolutionPots;
    }

    FlowerPot* getSolutionFlowerPot(int index) {
      return solutionPots[index];
    }

    /**
     * Returns true if the input tag is part of the solution, false otherwise.
     */
    bool isTagInSolution(RFIDTag* tag) {
      for (int ip=0; ip<numberSolutionPots; ip++) {
        if (solutionPots[ip]->isKnownTag(tag)) {
          return true;      
        }
      }
      return false;
    }
  
};

/**
 * The MasterMind Game Class. Contains the logic for maintaining the game state.
 */
class MasterMindFlowerPotGame {

    const static int MAX_READERS = 5;
    const static int MAX_FLOWER_POTS = 12;
    const static int NUM_GAME_SOLUTIONS = 3;
    const static int NUM_POTS_PER_SOLUTION = 3;

    MasterMindReader* readers[MAX_READERS];
    int numReaders = 0;

    FlowerPot* flowerPots[MAX_FLOWER_POTS];
    int numFlowerPots = 0;

    MasterMindGameSolution* solutions[NUM_GAME_SOLUTIONS];

    bool gameStatusChanged = false;
    byte numberFlowerPotsOnReaders = 0;
    byte numberCorrectFlowerPots = 0;

    int gameSolutionNumber = 0;

    /**
     * Chooses a new random solution from one of the possible solutions and returns the index
     * of the choosen solution.
     */
    int initRandomSolution() {
      if (NUM_GAME_SOLUTIONS > 1) {
        gameSolutionNumber = random(0, NUM_GAME_SOLUTIONS);  // Returns a long value from 0 to NUM_GAME_SOLUTIONS-1
      }
    return gameSolutionNumber;
    }

    /**
     * Processes updates from the RFID cards. Called from the update status method.
     * Returns true if at lease one new tag was found, false otherwise.
     */
    bool processReaderUpdate() {
      bool atLeastOneNewTag = false;
      for (uint8_t reader=0; reader<numReaders; reader++) {
        MasterMindReader* mmReader = readers[reader];
        if (mmReader->isTagDataAvailable()) {
          atLeastOneNewTag = atLeastOneNewTag || mmReader->processTagUpdate(&mmReader->uid);
        }
      }
      return atLeastOneNewTag;
    }

    /**
     * Loops through the readers and updates the game status based on the current state of the readers.
     */
    void processReaderCurrentStatus() {

      unsigned long now = millis();

      numberFlowerPotsOnReaders = 0;
      numberCorrectFlowerPots = 0;
      
      for (int reader=0; reader<numReaders; reader++) {
        MasterMindReader* mmReader = readers[reader];
        mmReader->setTagCorrect(false);

        if ( mmReader->isTagPresent() ) {

          // Check the timeout for last update from the reader. Clear if we have exceeded the timeout.
          // This is the amount of time that the tag needs to be off the reader before we acknowledge it.
          // Need to account for random-ness of when we get updates from the card relative to how often
          // we "loop". 
          if ((now - mmReader->getTimeLastUpdated()) > 2000 ) {
            mmReader->clearCurrentTag();
            gameStatusChanged = true;
            
          } else {

            // We have a pot on a reader
            numberFlowerPotsOnReaders++;

            // Loop over all the pots and set the current pot independent of it being correct or not
            for (int ipot=0; ipot<numFlowerPots; ipot++) {
              FlowerPot* flowerPot = flowerPots[ipot];
              if (flowerPot->isKnownTag(mmReader->getCurrentTag())) {
                mmReader->setCurrentFlowerPot(flowerPot);
              }
            }

            // See if this pot is one of the answers
            MasterMindGameSolution* currentSolution = solutions[gameSolutionNumber];
            if (currentSolution->isTagInSolution(mmReader->getCurrentTag())) {
              numberCorrectFlowerPots++;
              mmReader->setTagCorrect(true);
            }
            
          }
        }
      }

    }


  public:

    unsigned long lastPrintTime = 0;

    MasterMindFlowerPotGame() {
      numReaders = 0;  
      numFlowerPots = 0;
    }

    void addReader(MasterMindReader* r) {
      if (numReaders < MAX_READERS) {
        readers[numReaders] = r;
        numReaders++;
      } else {
        Serial.println(F("Error: Maximum readers exceeded in MasterMindFlowerPotGame.addReader()"));
      }
    }

    MasterMindReader** getReaders() {
      return readers;
    }

    int getNumReaders() {
      return numReaders;
    }

    MasterMindReader* getReader(int index) {
      return readers[index];
    }

    void addFlowerPot(FlowerPot* p) {
      if (numFlowerPots < MAX_FLOWER_POTS) {
        flowerPots[numFlowerPots] = p;
        numFlowerPots++;
      } else {
        Serial.println(F("Error: Maximum flowerpots exceeded in MasterMindFlowerPotGame.addFlowerPot()"));
      }
    }

    FlowerPot** getFlowerPots() {
      return flowerPots;
    }

    int getNumFlowerPots() {
      return numFlowerPots;
    }

    FlowerPot* getFlowerPot(int index) {
      return flowerPots[index];
    }

    /**
     * This needs to be called after all the Readers and FlowerPots have been added.
     */
    void initializeGameSolutions() {
       // At some point if needed this can be done more elegantly.
       int ipot = 0;
       for (int is=0; is<NUM_GAME_SOLUTIONS; is++) {
         MasterMindGameSolution* mmSolution = new MasterMindGameSolution();
         solutions[is] = mmSolution;
         for (int ip=0; ip<NUM_POTS_PER_SOLUTION; ip++) {
           mmSolution->addSolutionFlowerPot(flowerPots[ipot]); ipot++;
           if (ipot == numFlowerPots) {
             ipot = 0;
           }
         }    
      }
    }
    
    void reset() {
      gameStatusChanged = false;
      numberFlowerPotsOnReaders = 0;
      numberCorrectFlowerPots = 0;
      initRandomSolution();
    }

    bool getGameStatusChanged() {
      return gameStatusChanged;
    }

    byte getNumberFlowerPotsOnReaders() {
      return numberFlowerPotsOnReaders;
    }

    byte getNumberCorrectFlowerPots() {
      return numberCorrectFlowerPots;
    }

    int getGameSolutionNumber() {
      return gameSolutionNumber;
    }

    bool isCompletedGuess() {
      return (numberFlowerPotsOnReaders == NUM_POTS_PER_SOLUTION);
    }

    bool isGameSolved() {
      return (numberCorrectFlowerPots == NUM_POTS_PER_SOLUTION);
    }

    /**
     * Main update method that should be called from the "loop".
     */
    void updateGameStatus() {

      // Update data that may be avaiable from the RFID cards.
      gameStatusChanged = processReaderUpdate();

      // Update to current state
      processReaderCurrentStatus();
       
    }

  
};

// This is the main game object
MasterMindFlowerPotGame* mmGameInstance;


/**
 * Arduino initialization entry point.
 */
void setup() {

  // Initialize serial communications with the PC
  Serial.begin(9600); 
  while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

#ifdef DO_DEBUG
  Serial.println("Setup started.");
#endif

  // Initialize random seed value. Using pin 0. It is assumed that this pin is not used.
  // Doing this allows the random method to return a random sequence from one run of the
  // Arduino to the next.
  randomSeed(analogRead(0));

  // Initialize mastermind
  mmGameInstance = initializeMastermind();

  // Initialize game event communications
  initializeGameEventComm(); 

  // Initialize RFID
  initializeRFID(mmGameInstance);

  // Reset all game data and other state
  reset(); 
  
  //Flash bread board LED to indicate startup completed
  pinMode(BREADBOARD_LED,OUTPUT);
  flashLED(BREADBOARD_LED, 175, 3);

#ifdef DO_DEBUG
  Serial.println("Setup finished.");
#endif

}

/**
 * Inializes the game instance objects. Should be called once from the setup() function.
 */
MasterMindFlowerPotGame* initializeMastermind() {

  // Create game instance
  MasterMindFlowerPotGame* newGame = new MasterMindFlowerPotGame();

  // Create and add a readers
  for (uint8_t reader=0; reader<NUM_MM_READERS; reader++) {
    newGame->addReader(new MasterMindReader(READER_INITIAL_RFID_NSS_PIN + reader, RST_PIN));  
  }

  // Initialize flower pots that can be part of a solution. Each pot has two RFID tags that can represent it.
  // One tag is a backup. Only one of the two tags can be active at game time.
  for (int ip=0; ip<NUM_KNOWN_FLOWER_POTS; ip++) {
    FlowerPot* fp = new FlowerPot(ip + 1);
    fp->addKnownTag(new RFIDTag(tagIdsByFlowerPot[ip][0], 4)); // 4 is the number of bytes in the RFID
    fp->addKnownTag(new RFIDTag(tagIdsByFlowerPot[ip][1], 4));
    newGame->addFlowerPot(fp);    
  }

  // Initialize the game solutions. This call needs to occur after all the pots and readers have been added to the game.
  newGame->initializeGameSolutions();

return newGame;
}

/**
 * 
 */
void initializeGameEventComm() {
#ifdef DO_GAME_EVENT_COMM
  initCommunications(MASTER_MIND_POT_GAME_NODE);  // THIS Is defined in the GameCommUtils file
  setLocalEventHandler(receiveCommEvent);
  setLocalErrorHandler(errorHandler);
  Serial.println("Game Event Communications initialized.");
#endif
#ifndef DO_GAME_EVENT_COMM
  Serial.println("Game Event Communications Is Disabled.");
#endif
}

/**
 * Initializes the RFID readers. Should be called once from setup() after the game has been initialized
 */
void initializeRFID(MasterMindFlowerPotGame* mmGame) {

#ifdef DO_DEBUG
  Serial.println("Begin RFID initialization.");
#endif

  // Init SPI bus to communicate with RFID cards
  SPI.begin();

  // Maybe?
  //delay(1000);

  for (uint8_t reader = 0; reader < mmGame->getNumReaders(); reader++) {
    MasterMindReader* mmReader = mmGame->getReader(reader);
    int pinId = mmReader->getPinId();

#ifdef DO_DEBUG
    Serial.print(F("Reader "));
    Serial.print(reader+1);
    Serial.print(F(" using pin "));
    Serial.print(pinId);
    Serial.print(F("."));
#endif

    // Initialize this mmReader's arduino card pin
    pinMode(pinId, OUTPUT);
    digitalWrite(pinId, HIGH);

    // Initialize the RFID reader card
    bool initGood = mmReader->initializeFirmwareCommunication();

#ifdef DO_DEBUG
    Serial.print(F(" Firmware init is "));
    Serial.print(initGood ? F("good.") : F("bad."));
    Serial.print(F(" Version: 0x"));
    Serial.print(mmReader->getFirmwareVersionValue(), HEX);
    Serial.print(mmReader->isExpectedFirmwareVersion() ? F(" [IsExpected] ") : F(" [IsNotExpected] "));
    mmReader->PCD_DumpVersionToSerial();
#endif
    
  }

#ifdef DO_DEBUG
  Serial.println("Finished RFID initialization.");
#endif
  
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
#ifdef DO_DEBUG
   Serial.print(F("Reset FlowerPot Game data: "));
   Serial.println(eventData.data);
#endif

   mmGameInstance->reset();

#ifdef DO_DEBUG
   Serial.print(F("  Game solution number is: "));
   Serial.println(mmGameInstance->getGameSolutionNumber());
#endif

  
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

// **********************************************************************************
// Main Loop
// **********************************************************************************
void loop() {

// Event communications
#ifdef DO_GAME_EVENT_COMM
  doComm();
#endif

  // Update the game
  mmGameInstance->updateGameStatus();

#ifdef DO_DEBUG
  if ( mmGameInstance->getGameStatusChanged() || (millis() - mmGameInstance->lastPrintTime) > 5000) {
    printCurrentGameStatus(mmGameInstance);
    mmGameInstance->lastPrintTime = millis();
  }
#endif
  
  
}

/**
 * 
 */
void printCurrentGameStatus(MasterMindFlowerPotGame* mmGame) {

  Serial.println(F("**Current game status:"));
  Serial.print(F("  Number of flower pots: "));
  Serial.println(mmGame->getNumberFlowerPotsOnReaders());

  if (mmGame->isGameSolved()) {
    Serial.println(F("  We have a WINNER!!!!!"));
  } else if (mmGame->isCompletedGuess()) {
    Serial.print(F("  Your guess has "));
    Serial.print(mmGame->getNumberCorrectFlowerPots());
    Serial.println(F(" correct flower pots. Try again."));
  }

#ifdef DO_GAME_STATUS_DETAILS
  Serial.print(F("  Solution number is "));
  Serial.println(mmGame->getGameSolutionNumber());
  
  Serial.print(F("  Number of correct flower pots: "));
  Serial.println(mmGame->getNumberCorrectFlowerPots());

  for (uint8_t reader = 0; reader < mmGame->getNumReaders(); reader++) {
    MasterMindReader* mmReader = mmGame->getReader(reader);

    Serial.print(F("  Reader "));
    Serial.print(reader);

    if (mmReader->isTagPresent()) {
      Serial.print(F(" has pot [" ));
      Serial.print(mmReader->getCurrentFlowerPot()->getId());
      Serial.print(F("] "));
      mmReader->getCurrentTag()->printIdToSerial();
      Serial.print(F(" present for (millis): "));
      Serial.print(millis() - mmReader->getTimeLastChanged());
      if (mmReader->isTagCorrect()) {
        Serial.print(F(" Pot is correct :)"));
      } else {
        Serial.print(F(" Pot is incorrect :("));
      }
    } else {
      Serial.print(F(" has no pot. :(" ));
    }

    Serial.println();

  }
#endif
  
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

void flashLED (int ledPort, int duration, int numFlashes) {
  int dur = max(2, duration);
  for (int i=0; i<numFlashes; i++) {
    digitalWrite(ledPort,HIGH);
    delay(dur);
    digitalWrite(ledPort,LOW);
    delay(dur/2);
  }
}






