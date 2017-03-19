/**
 * Mastermind Flower Pot Game Node.
 */

// Wires to RFID Readers, right to left: red, brown, black, blue, orange/purple, yellow, green/white. Last pin is empty.

// Wire to arduino:
// yellow: 13                         SCK
// blue:   12                         MISO
// orange/purple: 11                  MOSI
// brown:  10                         RESET
// Greens/white: 7, 8, 9
// Black: GND 1 opposite side         GND 
// red: 3 Volt                        VCC

// ICSP Pins
//
// MISO    VCC
// SCK     MOSI
// RESET   GND
//


//#include <SoftwareSerial.h>

//#include <PJON.h>
#include <SPI.h>
#include <MFRC522.h>
#include <GameCommUtils.h>
#include <string.h>

using namespace std;

// Which nodes do we communicate to. 
#define CONTROLLER_NODE GAME_CONTROLLER_NODE
#define AUDIO_NODE      GAME_CONTROLLER_NODE

// Uncomment to enable event communications
#define DO_GAME_EVENT_COMM

// Logging *******************************************

// Uncomment to enable Info level Serial Prints - this is the minimal logging other than none.
#define DO_INFO

// Uncommment this to get the logging feedback for playing the game via the Serial port.
#define DO_PLAY_GAME_LOGGING

// Uncomment to enable Debug level Serial Prints
//#define DO_DEBUG

// Uncomment to enable Debug level Serial Prints related to RFID
#define DO_DEBUG_RFID

// Uncomment to enable printing game status update
//#define DO_GAME_STATUS_UPDATE

// Uncomment to enable Debug level Serial Prints related to CommUtils
#define DO_DEBUG_GAME_COMM


// Board locations *******************

// PIN for comm
#define GAME_COMM_PIN 12

// RFID reset pin. Configurable but shared by all RFID readers
#define RST_PIN         10 

// Reader pin numbers are assumed sequential starting with this value.
// These are the slave select lines of the RFID readers.
#define READER_INITIAL_RFID_NSS_PIN    7
#define NUM_MM_READERS 3

// This is the number of flower pots that are known to the logic
#define NUM_KNOWN_FLOWER_POTS 10

// Comment this if the game requires a start event to be active 
//#define GAME_START_EVENT_NOT_REQUIRED

// Active Flower Pots that can be part of a solution.
// Each pot has 2 tags, one is a backup. Only one of the two tags can be
// active in any of the pots at game time.
// The RFID has 4 bytes.
static byte tagIdsByFlowerPot[NUM_KNOWN_FLOWER_POTS][2][4] = {
  { {0xC2, 0x66, 0x7F, 0x64}, {0x2B, 0xEE, 0xFD, 0xC4} },   // 1.1, 1.2
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

// Audio Tracks
#define BASE_AUDIO_TRACK_ID              0
#define AUDIO_ZERO_CORRECT_GUESS         BASE_AUDIO_TRACK_ID
#define AUDIO_ONE_CORRECT_GUESS          BASE_AUDIO_TRACK_ID + 1
#define AUDIO_TWO_CORRECT_GUESS          BASE_AUDIO_TRACK_ID + 2
#define AUDIO_ALL_CORRECT_GUESS          BASE_AUDIO_TRACK_ID + 3
#define AUDIO_NO_ACTION                  BASE_AUDIO_TRACK_ID + 4

// Local functions
void printBegin(String s);
void printEnd(String s);
void processReceivedStartGame();
void processSendEvents();


/**
 * RFID Tag.
 */
class RFIDTag {

    const static byte MAX_ID_BYTES = 4;

    byte size = 0;
    byte id[MAX_ID_BYTES] = { 0, 0, 0, 0 };  // 4 max
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
      memset(id, 0, MAX_ID_BYTES);
      clearFlag = true;
    }

    void setId(MFRC522::Uid *uid) {
      setId(uid->uidByte, uid->size);
    }

    void setId(byte *uidBytes, byte inSize) {
      if (inSize <= MAX_ID_BYTES) {
        memcpy(id, uidBytes, inSize);
        size = inSize;
        clearFlag = false;
      } else {
        Serial.println(F("RFIDTag Err 1"));
      }
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

    const static byte MAX_TAGS_PER_POT = 2;

    RFIDTag* knownTags[MAX_TAGS_PER_POT];
    byte numberKnownTags = 0;

    byte id;  // ID of the pot; 1-10 or whatever.

  public:

    FlowerPot( byte potId ) {
      id = potId;
    }

    byte getId() {
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
        Serial.println(F("FP Err 1"));
      }
    }

    /**
     * Returns true if the input tag is one of the tags that is associated with this pot.
     */
    bool isKnownTag(RFIDTag* tag) {
      for (uint8_t i=0; i<numberKnownTags; i++) {
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

    byte pinId;
    byte firmwareVersionValue = 0x00;

  public:

    GameRFIDReader(): MFRC522() {}
    GameRFIDReader(byte resetPowerDownPin): MFRC522(resetPowerDownPin) {}
    GameRFIDReader(byte chipSelectPin, byte resetPowerDownPin): MFRC522(chipSelectPin, resetPowerDownPin) {
      pinId = chipSelectPin;
    }
    
    byte getPinId() {
      return pinId;
    }

    bool isTagDataAvailable()
    {
      //PICC_IsNewCardPresent();
      //PICC_ReadCardSerial();
      return PICC_IsNewCardPresent() && PICC_ReadCardSerial();
    //return false;
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
      for (uint8_t bb=0; bb<1; bb++) {
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
     * one wins. If this update was a new tag then true is returned, otherwise false is returned.
     */
    boolean processTagUpdate() {
      bool updateWasNewTag = false;
      if (isTagDataAvailable()) {
        if (currentRFIDTag->isIdClear()) {
          currentRFIDTag->setId(&this->uid);
          timeLastChanged = millis();
          timeLastUpdated = timeLastChanged;
          updateWasNewTag = true;
        } else if (currentRFIDTag->isEqualId(&this->uid)) {
          timeLastUpdated = millis();
        }
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

    const static uint8_t MAX_POTS_PER_SOLUTION = 3;

    FlowerPot* solutionPots[MAX_POTS_PER_SOLUTION];
    uint8_t numberSolutionPots = 0;

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
        Serial.println(F("Sol Err1"));
      }
    }

    /**
     * Returns true if the input tag is part of the solution, false otherwise.
     */
    bool isTagInSolution(RFIDTag* tag) {
      for (uint8_t ip=0; ip<numberSolutionPots; ip++) {
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

    const static uint8_t MAX_READERS = 5;
    const static uint8_t MAX_FLOWER_POTS = 12;
    const static uint8_t NUM_GAME_SOLUTIONS = 3;
    const static uint8_t NUM_POTS_PER_SOLUTION = 3;

    MasterMindReader* readers[MAX_READERS];
    uint8_t numReaders = 0;

    FlowerPot* flowerPots[MAX_FLOWER_POTS];
    uint8_t numFlowerPots = 0;

    MasterMindGameSolution* solutions[NUM_GAME_SOLUTIONS];

    bool gameIsStarted = false;
    
    bool gameStatusChanged = false;
    byte numberFlowerPotsOnReaders = 0;
    byte numberCorrectFlowerPots = 0;

    uint8_t gameSolutionNumber = 0;

    long gameStartTimeMillis = 0;
    long firstPotTimeMillis = 0;
    long firstGuessTimeMillis = 0;
    long mostRecentGuessTimeMillis = 0;
    long solutionFoundTimeMillis = 0;

    /**
     * Chooses a new random solution from one of the possible solutions and returns the index
     * of the choosen solution.
     */
    uint8_t initRandomSolution() {
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
        if (mmReader->processTagUpdate()) {
          atLeastOneNewTag = true;
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
      
      for (uint8_t reader=0; reader<numReaders; reader++) {
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
            for (uint8_t ipot=0; ipot<numFlowerPots; ipot++) {
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

    void processStatisticsUpdate() {
      unsigned long now = millis();

      // Is this the first flower pot in this game
      if ((firstPotTimeMillis == 0) && (numberFlowerPotsOnReaders > 0)) {
        firstPotTimeMillis = now;

Serial.print("SET firstPotTimeMillis ");
Serial.println(firstPotTimeMillis);
        
      }

      // Is the first guess
      if ((firstGuessTimeMillis == 0) && isCompletedGuess()) {
        firstGuessTimeMillis = now;

Serial.print("SET firstGuessTimeMillis ");
Serial.println(firstGuessTimeMillis);
      }

      // Most recent guess
//      if (isCompletedGuess()) {
//        mostRecentGuessTimeMillis = now;

//Serial.print("SET mostRecentGuessTimeMillis ");
//Serial.println(mostRecentGuessTimeMillis);
//      }

      // Is this a winner
      if ((solutionFoundTimeMillis == 0) && isGameSolved()) {
        solutionFoundTimeMillis = now;

Serial.print("SET solutionFoundTimeMillis ");
Serial.println(solutionFoundTimeMillis);
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
        Serial.println(F("MM Err 1"));
      }
    }

    MasterMindReader** getReaders() {
      return readers;
    }

    uint8_t getNumReaders() {
      return numReaders;
    }

    MasterMindReader* getReader(uint8_t index) {
      return readers[index];
    }

    void addFlowerPot(FlowerPot* p) {
      if (numFlowerPots < MAX_FLOWER_POTS) {
        flowerPots[numFlowerPots] = p;
        numFlowerPots++;
      } else {
        Serial.println(F("MM Err 2"));
      }
    }

    FlowerPot** getFlowerPots() {
      return flowerPots;
    }

    uint8_t getNumFlowerPots() {
      return numFlowerPots;
    }

    FlowerPot* getFlowerPot(uint8_t index) {
      return flowerPots[index];
    }

    /**
     * This needs to be called after all the Readers and FlowerPots have been added.
     */
    void initializeGameSolutions() {
       // At some point if needed this can be done more elegantly.
       uint8_t ipot = 0;
       for (uint8_t is=0; is<NUM_GAME_SOLUTIONS; is++) {
         MasterMindGameSolution* mmSolution = new MasterMindGameSolution();
         solutions[is] = mmSolution;
         for (uint8_t ip=0; ip<NUM_POTS_PER_SOLUTION; ip++) {
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
      gameStartTimeMillis = 0;
      firstPotTimeMillis = 0;
      firstGuessTimeMillis = 0;
      mostRecentGuessTimeMillis = 0;
      solutionFoundTimeMillis = 0;
      initRandomSolution();
    }

    void processStartGame() {
      if (!isGameStarted()) {
        reset();
        setGameStartTime();
        setGameStarted(true);
      }
    }

    bool getGameStatusChanged() {
      return gameStatusChanged;
    }

    bool isGameStarted() {
      return gameIsStarted;
    }

    void setGameStarted(bool val) {
      gameIsStarted = val;
    }

    uint8_t getNumberFlowerPotsOnReaders() {
      return numberFlowerPotsOnReaders;
    }

    uint8_t getNumberCorrectFlowerPots() {
      return numberCorrectFlowerPots;
    }

    uint8_t getGameSolutionNumber() {
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
      // Game status changed is true if one of the readers has a new tag, false otherwise.
      gameStatusChanged = processReaderUpdate();

      // Update to current state
      processReaderCurrentStatus();

      // Update Statistics
      if (gameStatusChanged) {
        processStatisticsUpdate();
      }
       
    }

    void setGameStartTime() {
      gameStartTimeMillis = millis();
    }

    long getGameStartTimeMillis() {
      return gameStartTimeMillis;
    }

    long getFirstPotTimeMillis() {
      return firstPotTimeMillis;
    }

    long getFirstGuessTimeMillis() {
      return firstGuessTimeMillis;
    }

    long getMostRecentGuessTimeMillis() {
      return mostRecentGuessTimeMillis;
    }

    long getSolutionFoundTimeMillis() {
      return solutionFoundTimeMillis;
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

#ifdef DO_INFO
  printBegin(F("Setup"));
#endif

  // Initialize random seed value. Using pin 0. It is assumed that this pin is not used.
  // Doing this allows the random method to return a random sequence from one "run" of the
  // Arduino to the next.
  randomSeed(analogRead(0));

  // Initialize mastermind
  mmGameInstance = initializeMastermind();

  // Initialize RFID
  initializeRFID(mmGameInstance);

  // Initialize game event communications
  initializeGameEventComm();

#ifdef GAME_START_EVENT_NOT_REQUIRED
  mmGameInstance->processStartGame();
#endif
  
#ifdef DO_INFO
  printEnd(F("Setup"));
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
  // Node ID is in GameCommUtil.h

//#ifdef DO_DEBUG
  printBegin(F("CommNodeInit"));
  Serial.print(F(" "));
  Serial.print(MASTER_MIND_POT_GAME_NODE);
  Serial.print(F(", Pin: "));
  Serial.println(GAME_COMM_PIN);
//#endif

  //setCommUtilsReceiveTimeout(2000);
  
  initOverrideComm(MASTER_MIND_POT_GAME_NODE, GAME_COMM_PIN);
  setLocalEventHandler(receiveCommEvent);
  setLocalErrorHandler(errorHandler);
//#ifdef DO_DEBUG  
  printEnd(F("CommNodeInit"));
//#endif
#endif

#ifndef DO_GAME_EVENT_COMM
#ifdef DO_DEBUG
  Serial.println(F("GameComm disabled."));
#endif
#endif
}

 /**
  * Initializes the RFID readers. Should be called once from setup() after the game has been initialized
  */
 void initializeRFID(MasterMindFlowerPotGame* mmGame) {

#ifdef DO_DEBUG_RFID
  printBegin(F("RFIDinit"));
#endif

  // Init SPI bus to communicate with RFID cards
  SPI.begin();
    
  for (uint8_t reader = 0; reader < mmGame->getNumReaders(); reader++) {
    MasterMindReader* mmReader = mmGame->getReader(reader);
    int pinId = mmReader->getPinId();

#ifdef DO_DEBUG_RFID
    Serial.print(F("Reader "));
    Serial.print(reader+1);
    Serial.print(F(", pin "));
    Serial.print(pinId);
#endif

    // Initialize this mmReader's arduino card pin
    pinMode(pinId, OUTPUT);
    digitalWrite(pinId, HIGH);

    // Initialize the RFID reader card
    bool initGood = mmReader->initializeFirmwareCommunication();

#ifdef DO_DEBUG_RFID
    Serial.print(F(" Firmware init is "));
    Serial.print(initGood ? F("good.") : F("bad."));
    Serial.print(F(" V: 0x"));
    Serial.print(mmReader->getFirmwareVersionValue(), HEX);
    Serial.print(mmReader->isExpectedFirmwareVersion() ? F(" :) ") : F(" :( "));
    mmReader->PCD_DumpVersionToSerial();
#endif
    
  }

#ifdef DO_DEBUG_RFID
  printEnd("RFIDinit");
#endif
  
}



void receiveCommEvent() {

  //use eventData.whatever to get what was sent and switch
  switch (eventData.event) {
    case CE_RESET_NODE:
      reset();
//      respondAckToSender();
      break;
    case CE_START_PUZZLE:
      processReceivedStartGame();
//      respondAckToSender();
      break;
    case CE_PING:
#ifdef DO_DEBUG_GAME_COMM    
      Serial.print(F("Rcv PING: ["));
      Serial.print(eventData.data);
      Serial.println(F("]"));
#endif
      sendEventToNode(CONTROLLER_NODE, CE_PONG, "pong");
      break;
    case CE_ACK:
      Serial.print(F("Rcv ACK. ["));
      Serial.print(eventData.data);
      Serial.println(F("]"));
     break;
    default:
      ;
  }
}

void reset() {
  mmGameInstance->reset();  
}

void processReceivedStartGame() {
  mmGameInstance->processStartGame();
}

//Sample Error Handler
void errorHandler(uint8_t code, uint8_t data) {
  if(code == CONNECTION_LOST) {
    Serial.println(F("CMErr1"));
    //Serial.print("MM Connection with device ID ");
    //Serial.print(data);
    //Serial.println(" is lost.");
  } else if(code == PACKETS_BUFFER_FULL) {
    Serial.println(F("CMErr2"));
//    Serial.print("MM Packet buffer is full, has now a length of ");
//    Serial.println(data, DEC);
//    Serial.println("Possible wrong bus configuration!");
//    Serial.println("higher MAX_PACKETS in PJON.h if necessary.");
  } else if(code == CONTENT_TOO_LONG) {
    Serial.println(F("CMErr3"));
//    Serial.print("MM Content is too long, length: ");
//    Serial.println(data);
  } else {
    Serial.println(F("CMErr4"));
//    Serial.print("MM Unknown error code received: ");
//    Serial.println(code);
//    Serial.print("With data: ");
//    Serial.println(data);
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
  processGameLoopIteration();
  
}

/**
 * Main loop processing of the game.
 */
void processGameLoopIteration() {
  if (mmGameInstance->isGameStarted()) {
    mmGameInstance->updateGameStatus();
    processSendEvents(mmGameInstance);
    
    if (mmGameInstance->getGameStatusChanged() || (millis() - mmGameInstance->lastPrintTime) > 15000) {
      printCurrentGameStatus(mmGameInstance);
      mmGameInstance->lastPrintTime = millis();
    }

  }
}

/**
 * 
 */
void printCurrentGameStatus(MasterMindFlowerPotGame* mmGame) {

#ifdef DO_PLAY_GAME_LOGGING
  if (mmGame->isGameSolved()) {
    Serial.print(F("-- "));
    Serial.print(F("You Win! Duration(ms): "));
    Serial.println(mmGame->getSolutionFoundTimeMillis() - mmGame->getFirstPotTimeMillis());
  } else if (mmGame->isCompletedGuess()) {
    Serial.print(F("-- "));
    Serial.print(mmGame->getNumberCorrectFlowerPots());
    Serial.println(F(" correct. Try again."));
  }
#endif

#ifdef DO_GAME_STATUS_UPDATE
  Serial.print(F("***:"));
  Serial.print(F(" #pots: "));
  Serial.print(mmGame->getNumberFlowerPotsOnReaders());

  Serial.print(F(", Sol# "));
  Serial.print(mmGame->getGameSolutionNumber());
  
  Serial.print(F(", #Correct: "));
  Serial.println(mmGame->getNumberCorrectFlowerPots());

  for (uint8_t reader = 0; reader < mmGame->getNumReaders(); reader++) {
    MasterMindReader* mmReader = mmGame->getReader(reader);

    Serial.print(F(" Rdr "));
    Serial.print(reader);

    if (mmReader->isTagPresent()) {
      Serial.print(F(": pot# " ));
      Serial.print(mmReader->getCurrentFlowerPot()->getId());
      Serial.print(F(", "));
      mmReader->getCurrentTag()->printIdToSerial();
      Serial.print(F(", millis: "));
      Serial.print(millis() - mmReader->getTimeLastChanged());
      if (mmReader->isTagCorrect()) {
        Serial.print(F(", correct."));
      } else {
        Serial.print(F(", incorrect."));
      }
    } else {
      Serial.print(F(", no pot." ));
    }

    Serial.println();

  }
#endif
  
}

void processSendEvents(MasterMindFlowerPotGame* mmGame) {

  if (mmGame->getGameStatusChanged()) {
    if (mmGame->isGameSolved()) {
      sendAudioEvent(AUDIO_ALL_CORRECT_GUESS);
    } else if (mmGame->isCompletedGuess()) {
      if (mmGame->getNumberCorrectFlowerPots() == 0) {
        sendAudioEvent(AUDIO_ZERO_CORRECT_GUESS);
      } else if (mmGame->getNumberCorrectFlowerPots() == 1) {
        sendAudioEvent(AUDIO_ONE_CORRECT_GUESS);
      } else if (mmGame->getNumberCorrectFlowerPots() == 2) {
        sendAudioEvent(AUDIO_TWO_CORRECT_GUESS);
      }
    }
  }  
  
}

void sendAudioEvent(int audioId) {
#ifdef DO_DEBUG_GAME_COMM    
  Serial.print(F("Send Audio: "));
  Serial.println(audioId);
#endif  
  sendIntEventToNode(AUDIO_NODE, CE_PLAY_TRACK, audioId);
}


void printBegin(String s) {
  Serial.print(F("B "));
  Serial.println(s);
}

void printEnd(String s) {
  Serial.print(F("E "));
  Serial.println(s);
}




