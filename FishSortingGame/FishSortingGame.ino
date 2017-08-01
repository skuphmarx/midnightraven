#include <SPI.h>
#include <MFRC522.h>
#include <GameCommUtils.h>

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
    #include <avr/power.h>
#endif

using namespace std;

// Uncomment to enable Serial Debug Prints
#define DO_DEBUG

// Uncomment to enable event communications
#define DO_GAME_EVENT_COMM

#define SHORT_TRACK_START // If defined it skips most of the start spiel and just does a short version. for debug

// Comment this if the game requires a start event to be active
#define GAME_ACTIVATE_EVENT_NOT_REQUIRED

#define FISH_SORTING_COMM_PIN 8   //  was 8  A0 etc. don't seem to work on Mega, nor do > 13


#define READER_INITIAL_RFID_NSS_PIN   24  // was 4 RFID pin numbers are sequential starting with this value.
#define RST_PIN                      5  // RFID reset pin. Configurable but shared by all RFID readers
#define PIXEL_INITIAL_PIN            34  // was 11 Pixel pin numbers are sequential starting with this value.

#define NUM_FISH_PER_SPECIES          4  // Each species has this many individual fish

struct RGBColor {
    byte r;
    byte g;
    byte b;
};

enum fishSpeciesColorId {
    red,
    yellow,
    green,
    blue,
    NUM_FISH_SPECIES
};

RGBColor rgbColorforFishSpeciesColorId[NUM_FISH_SPECIES] = { {150,0,0}, {150,150,0}, {0,150,0}, {0,0,150} };

RGBColor alreadyUsedFishColor = {75,0,130}; //{140,40,220};
RGBColor whiteRgb = {255,255,255};


byte tagIdsForGameStartFish[NUM_FISH_PER_SPECIES][4] = {
    {0x7A, 0x31, 0x41, 0xD5}, {0xFF, 0xFF, 0xFF, 0xFF}   // game start fish ID's
};

bool startTheGame = false;
bool testModeOnly = false;

// **********************************************************************************
// Pixel LED Light String
// **********************************************************************************
class PixelLightString {
    const static int flashDuration = 30;
    
    int lightsPerString;
    int pin;
	uint32_t currentColor;  // Current color of pixel 0 used to reset if flashing
    Adafruit_NeoPixel *pixel = NULL;
    
    void init () {
        pixel = new Adafruit_NeoPixel(lightsPerString, pin, NEO_GRB + NEO_KHZ800); // Init each Pixel, unique pins
        pixel->begin();
        reset();
    }
    
    //Theatre-style crawling lights.
    void flash(uint32_t c, uint8_t wait) {
        for (int j=0; j<10; j++) {  //do 10 cycles of chasing
            for (int q=0; q < 3; q++) {
                for (uint16_t i=0; i < lightsPerString; i=i+3) {
                    pixel->setPixelColor(i+q, c);    //turn every third pixel on
                }
                pixel->show();
                
                delay(wait);
                
                for (uint16_t i=0; i < lightsPerString; i=i+3) {
                    pixel->setPixelColor(i+q, 0);        //turn every third pixel off
                }
            }
        }
    }

public:
    
    PixelLightString() {
        init();
    }
    
    PixelLightString(int numLights, int pinNumber) {
        lightsPerString = numLights;
        pin = pinNumber;
        init();
    }
    
    void flashColor(RGBColor color) {
        flash(pixel->Color(color.r, color.g, color.b), flashDuration);
    }
	
	void flashWhite() {
	   flash(pixel->Color(255,255,255),flashDuration);
	}
    
    void show() {
        pixel->show();
    }
    
    void setColor(RGBColor color) {
        for(int i=0; i<lightsPerString; i++) {
            pixel->setPixelColor(i,pixel->Color(color.r,color.g,color.b));
        }
    }
    
    void light(RGBColor color) {
        setColor(color);
        show();
    }
    
    void reset () {
        light({0,0,0});
    }
	
	void getColor() {
	   currentColor = pixel->getPixelColor(1);
	}
    
	void setFromPreviousColor() {

	    for(int i=0; i<lightsPerString; i++) {
            pixel->setPixelColor(i,currentColor);
        }
		
        show();
	}
};


// **********************************************************************************
// RFID Tag
// **********************************************************************************
class RFIDTag {
    
    const static byte MAX_ID_BYTES = 4;
    
    byte size = 0;
    byte id[MAX_ID_BYTES] = { 0, 0, 0, 0 };  // 4 max
    bool clearFlag = true;
  bool alreadyUsed = false; // When playing the game a tag can only be used once
    
public:
    
    RFIDTag() {
        clearId();
    }
    
    RFIDTag(byte b1, byte b2, byte b3, byte b4) {
        id[0] = b1;
        id[1] = b2;
        id[2] = b3;
        id[3] = b4;
        size=4;
        clearFlag = false;
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
    
    bool RFIDTag::operator==(const RFIDTag &tag) const {
#ifdef DO_DEBUG_TAGS
        Serial.print(F("Comparing tags: "));
        Serial.print(size);
        Serial.print(F("=="));
        Serial.print(tag.size);
        Serial.print(F(", "));
        Serial.print(clearFlag);
        Serial.print(F("=="));
        Serial.print(tag.clearFlag);
        Serial.print(F(","));
        printIdToSerial();
        Serial.print(F(" =="));
        tag.printIdToSerial();
        Serial.println();
#endif

        return (size == tag.size) &&
        (clearFlag == tag.clearFlag) &&
        (memcmp(id, tag.id, size) == 0);
    }
  
  void setAlreadyUsed() {
     alreadyUsed = true;
  }
  
  void clearAlreadyUsed() {
     
      alreadyUsed = false;
  }
    
  bool isAlreadyUsed() {
      return alreadyUsed;
    }
  
    void printIdToSerial() {
        for (byte i=0; i<size; i++) {
            Serial.print(id[i] < 0x10 ? " 0" : " ");
            Serial.print(id[i], HEX);
        }
    }
    
};


// **********************************************************************************
// Base Game RFID Reader. Contains non game specific methods and state of an individual RFID reader card.
// **********************************************************************************
class GameRFIDReader: public MFRC522 {
    
    const static byte EXPECTED_FIRMWARE_VERSION = 0x92;
    
    int pinId;
    byte firmwareVersionValue = 0x00;
    
public:
    
    GameRFIDReader(): MFRC522() {}
    GameRFIDReader(byte resetPowerDownPin): MFRC522(resetPowerDownPin) {}
    GameRFIDReader(byte chipSelectPin, byte resetPowerDownPin): MFRC522(chipSelectPin, resetPowerDownPin) {
        pinId = chipSelectPin;
        
        // Initialize the arduino card pin
        pinMode(pinId, OUTPUT);
        digitalWrite(pinId, HIGH);
        
        // Initialize the RFID reader card
        bool initGood = initializeFirmwareCommunication();
#ifdef DO_DEBUG
        Serial.print(F(" Firmware init is "));
        Serial.print(initGood ? F("good.") : F("bad."));
        Serial.print(F(" Version: 0x"));
        Serial.print(getFirmwareVersionValue(), HEX);
        Serial.print(isExpectedFirmwareVersion() ? F(" [IsExpected] ") : F(" [IsNotExpected] "));
        PCD_DumpVersionToSerial();
#endif
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
        PCD_Init();
        firmwareVersionValue = PCD_ReadRegister(VersionReg);
        // So far have not found a good way to determine if communication is good or bad.
        return true;
    }
};

class FishSpecies; //forward declaration of FishSpecies
// **********************************************************************************
// RFID Reader with game-specific methods and state
// **********************************************************************************
class PostReader: public GameRFIDReader {
    
    RFIDTag* currentRFIDTag = new RFIDTag();
    unsigned long timeLastChanged = millis();
    unsigned long timeLastUpdated = millis();
    bool currentTagIsCorrect = false;
    FishSpecies* currentFishSpecies = NULL;
    
public:
    
    PostReader():GameRFIDReader() {}
    
    PostReader(byte chipSelectPin, byte resetPowerDownPin): GameRFIDReader(chipSelectPin, resetPowerDownPin) {}
    
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
     * one wins.
     */
    bool processTagUpdate(MFRC522::Uid *uid) {
        bool foundNewTag = false;
        if (currentRFIDTag->isIdClear()) {
            currentRFIDTag->setId(uid);
            timeLastChanged = millis();
            timeLastUpdated = timeLastChanged;
            foundNewTag = true;
//#ifdef DO_DEBUG
//            Serial.print("processTagUpdate found:");
//            currentRFIDTag->printIdToSerial();
//#endif
        } else if (currentRFIDTag->isEqualId(uid)) {
            timeLastUpdated = millis();
        }
        return foundNewTag;
    }
    
    void clearCurrentTag() {
        currentRFIDTag->clearId();
        timeLastChanged = millis();
        timeLastUpdated = timeLastChanged;
        currentTagIsCorrect = false;
        currentFishSpecies = NULL;
    }
    
    unsigned long getTimeLastChanged() {
        return timeLastChanged;
    }
    
    unsigned long getTimeLastUpdated() {
        return timeLastUpdated;
    }
    
    void setCurrentFishSpecies(FishSpecies* f) {
        currentFishSpecies = f;
    }
    
    FishSpecies* getCurrentFishSpecies() {
        return currentFishSpecies;
    }
    
};


// **********************************************************************************
// Post
// **********************************************************************************
class Post {
    //each post contains a string of Pixel LED lights and an RFID reader
    const static int NUM_PIXELS_PER_POST=10; // Individual lights in each Pixel light string

    int id;
    PixelLightString *pixelLight = NULL;
    PostReader *rfidReader = NULL;
  RGBColor currentColor = {0,0,0};
    
public:
    Post ( int postId ) {
        id = postId;
        
#ifdef DO_DEBUG
        Serial.print(F("Post "));
        Serial.print(postId);
        Serial.print(F(" pixel pin "));
        Serial.print(postId + PIXEL_INITIAL_PIN);
        Serial.print(F(" reader pin "));
        Serial.print(postId + READER_INITIAL_RFID_NSS_PIN);
        Serial.print(F("."));
#endif
        
        rfidReader = new PostReader( postId + READER_INITIAL_RFID_NSS_PIN, RST_PIN);
        pixelLight = new PixelLightString( NUM_PIXELS_PER_POST, postId + PIXEL_INITIAL_PIN);
        pixelLight->light({150,0,150});
    }
    
    int getId() {
        return id;
    }
    
    PixelLightString* pixel() {
        return pixelLight;
    }
    
    void reset() {
        pixelLight->reset();
    currentColor = {0,0,0};
    }

    bool scanForFishAndSaveCurrent() {
        if (rfidReader->isTagDataAvailable()) {
            return rfidReader->processTagUpdate(&rfidReader->uid);
        }
        return false;
    }

    bool testReaderStatus() {
      if(rfidReader->isTagDataAvailable()) {
        return true;
      }

      return false;
    }
    
    bool isFishPresent() {
        return rfidReader->isTagPresent();
    }
    
    RFIDTag* currentFishTag() {
        return rfidReader->getCurrentTag();
    }
    
    bool currentFishMatches(RFIDTag *tag) {
#ifdef DO_DEBUG
        Serial.print("Checking post ");
        Serial.print(id);
        Serial.print(" for fish with tag");
        tag->printIdToSerial();
        Serial.print(" and found");
        rfidReader->getCurrentTag()->printIdToSerial();
        Serial.print(" match? ");
        Serial.println(tag->isEqualId(&rfidReader->uid));
#endif
        return (tag->isEqualId(&rfidReader->uid));
    }
    
    unsigned long getTimeLastUpdated() {
        return rfidReader->getTimeLastUpdated();
    }
    
    void clearCurrentTag() {
        rfidReader->clearCurrentTag();
    
    }
  
  void setCurrentColorAndLight(RGBColor color) {
     currentColor = color;
     light(color);
  }
  
  RGBColor getCurrentColor() {
     return currentColor;
  }

    
    void light(RGBColor color) {
#ifdef DO_DEBUG
        Serial.print("Lighting post ");
        Serial.print(id);
        Serial.print(" on pin ");
        Serial.print(id+PIXEL_INITIAL_PIN);
        Serial.print(" with color (");
        Serial.print(color.r);
        Serial.print(", ");
        Serial.print(color.g);
        Serial.print(", ");
        Serial.print(color.b);
        Serial.println(")");
#endif
        pixelLight->light(color);
    }
    
    void flashColor(RGBColor c) {
        pixelLight->flashColor(c);
    }
	
	void flashWhite() {
	   pixelLight->flashWhite();
	}

    void quickFlashColor(RGBColor c) {
      pixelLight->light(c);
      delay(400);
      pixelLight->reset();
    }

    void quickDoubleFlashColor(RGBColor c) {
	  pixelLight->getColor(); // This saves the current color in the pixel object
	  
      pixelLight->light(c);
      delay(200);
      pixelLight->reset();
      delay(100);
      pixelLight->light(c);
      delay(200);
    // Return to what the color was
      pixelLight->setFromPreviousColor();
    
    }

};

// **********************************************************************************
// Fish Species
// **********************************************************************************
class FishSpecies {

    int id;         // ID of this species; 0-3 or whatever.
    RGBColor color; // Color of this species.

    RFIDTag* knownTags[NUM_FISH_PER_SPECIES];
    int numberKnownTags = 0;
    
public:
    
    FishSpecies( int speciesId ) {
        id = speciesId;
        color = rgbColorforFishSpeciesColorId[speciesId];
    }
    
    int getId() {
        return id;
    }
    
    RGBColor getColor() {
        return color;
    }
    
    void addKnownTag(RFIDTag* tag) {
        if (numberKnownTags < NUM_FISH_PER_SPECIES) {
            knownTags[numberKnownTags] = tag;
            numberKnownTags++;
        } else {
            Serial.println(F("Error: Maximum tags exceeded in Fish.addKnownTag()"));
        }
    }
    
    bool isKnownTag(RFIDTag* tag) {
        for (int i=0; i<numberKnownTags; i++) {
            if (*tag == *knownTags[i]) {
                return true;
            }
        }
        return false;
    }
    
  void setNotUsed() {
          for (int i=0; i<numberKnownTags; i++) {
                 knownTags[i]->clearAlreadyUsed();
            }
  }
  
  bool setAlreadyUsedIfMatches(RFIDTag* fish) {
      bool ret = false;
      for (int i=0; i<numberKnownTags; i++) {
            if(*fish == *knownTags[i]) {
         knownTags[i]->setAlreadyUsed();
         ret = true;
         break;
      }
        }
    return ret;
  }
  
  bool isAlreadyUsed(RFIDTag* fish) {
      bool ret = false;
      for (int i=0; i<numberKnownTags; i++) {
            if(*fish == *knownTags[i] && knownTags[i]->isAlreadyUsed()) {
      Serial.println(F("*********************************ALREADY USED"));
         ret = true;
         break;
      }
        }
    return ret;
  }
  
    void printToSerial() {
        Serial.print("FishSpecies ");
        Serial.print(id);
        Serial.print(": (");
        Serial.print(color.r);
        Serial.print(", ");
        Serial.print(color.g);
        Serial.print(", ");
        Serial.print(color.b);
        Serial.print(")");
    }
  
  void printUsedTagStatus() {
      for(int i=0;i<numberKnownTags;i++) {
       Serial.print(knownTags[i]->isAlreadyUsed()); Serial.print(" ");
    }
  }
};



// **********************************************************************************
// Fish Sorting Game
// **********************************************************************************
class FishSortingGame {
    const static int NUM_POSTS = 3;
    const static int SOLUTION_SEQUENCE_LENGTH = 10;
    const static unsigned long INITIAL_GRACE_PERIOD = 5000; // TODO: (change to real value)millis to wait for a fish to be placed on post
    const static unsigned long GRACE_INCREMENT = 3000;      // num seconds longer each attempt
	const static unsigned long MAX_GRACE_PERIOD = 30000;
    unsigned long gracePeriod = INITIAL_GRACE_PERIOD;
	unsigned int numberOfAttempts = 0;  // How many times they have tried the game
    bool gameStatusChanged = false;
    bool gameActivated = false; //false until the room tells us we're ready to place via local event
    bool gameInProgress = false; //false until we detect a game start fish on post 0. Stays true until game is solved or is failed

//    RFIDTag* gameStartTag[2] = { new RFIDTag(tagIdsForGameStartFish[0], 4), new RFIDTag(tagIdsForGameStartFish[1], 4) }; // 4 bytes in each RFID tag
    RFIDTag gameStartTag[2] = { RFIDTag(tagIdsForGameStartFish[0], 4), RFIDTag(tagIdsForGameStartFish[1], 4) }; // 4 bytes in each RFID tag

  
    int postSequence[SOLUTION_SEQUENCE_LENGTH] = {0, 1, 2, 0, 1, 2, 0, 1, 2, 0};
    fishSpeciesColorId colorSequence[SOLUTION_SEQUENCE_LENGTH] = { red, yellow, green, blue, blue, green, yellow, red, red, green };
    
    int currentSequencePosition = 0;
    int lastLitSequencePosition = -1;
    unsigned long lastLightTime = 0;
    
    FishSpecies *fishSpecies[NUM_FISH_SPECIES];
    int numFishSpecies = 0; //Number of Fish known to this game
  
  void (*momentaryComm)(unsigned long);

    
    Post *posts[NUM_POSTS];

    void init() {

        //RFID Initialization
#ifdef DO_DEBUG
        Serial.println("Begin Post (Pixel+RFID) initialization.");
#endif
        for (int i=0; i<NUM_POSTS; i++) {
            posts[i] = new Post(i);
        }
#ifdef DO_DEBUG
        Serial.println("Finished Post (Pixel+RFID) initialization.");
#endif
    }

public:
    unsigned long lastPrintTime = 0;

    FishSortingGame () {
        init();
    }
    
    void reset() {
#ifdef DO_DEBUG
        Serial.println("Resetting Fish Sorting Game");
#endif
        gameActivated = false;
        gameInProgress = false; // SDK Added this. Is it correct?
        lastLitSequencePosition = -1;
        currentSequencePosition = 0;
        lastLightTime = 0;
        gameStatusChanged = false;
        for (int i=0; i<NUM_POSTS; i++) {
            posts[i]->reset();
        }
		numberOfAttempts++;
    
        clearUsedFish();
    
        initRandomGameSequence();
    }
  
  void setMomentaryComm(void (*function)(unsigned long howLong)) {
        momentaryComm = function;
    }
  
  void gameFailed() {
      for (int i=0; i<NUM_POSTS; i++) {
	  Serial.println(F("FLASHING POST"));
          posts[i]->flashColor(rgbColorforFishSpeciesColorId[fishSpeciesColorId::red]);
        }
    reset();
    activateGame();
	if(gracePeriod < MAX_GRACE_PERIOD) {
	    gracePeriod += GRACE_INCREMENT;
	}
  }
    
    void flashAllPosts(RGBColor color) {
        //Set each individual light to the same color
        for(int i=0; i<NUM_POSTS; i++) {
            posts[i]->pixel()->setColor(color);
        }
        //Show each post
        for(int i=0; i<NUM_POSTS; i++) {
            posts[i]->pixel()->show();
        }
        momentaryComm(500);
        
        //Set each individual light to black (off)
        for(int i=0; i<NUM_POSTS; i++) {
            posts[i]->pixel()->setColor({0,0,0});
        }
        for(int i=0; i<NUM_POSTS; i++) {
            posts[i]->pixel()->show();
        }
        momentaryComm(250);
    }

    void addFishSpecies(FishSpecies *newSpecies) {
        if (numFishSpecies < NUM_FISH_SPECIES) {
            fishSpecies[numFishSpecies] = newSpecies;
            numFishSpecies++;
        } else {
            Serial.println("Error: too many FishSpecies in FishSortingGame.addFishSpecies()");
        }
    }
    
    void lightNextPost () {

        if (lastLightTime == 0) {
#ifdef DO_DEBUG
            Serial.println("Lighting Game Start sequence");
#endif
            //Flash all posts to kick off the game
            for (int i=0; i<3; i++) {
                flashAllPosts(whiteRgb);
            }
            lastLightTime = millis();
            return;
        }
        
        if ((millis() - lastLightTime) > gracePeriod) {
#ifdef DO_DEBUG
            Serial.println("Timeout. No correct fish scanned within grace period.");
#endif
//      Flash the post we were waiting on once.
            posts[postSequence[currentSequencePosition]]->flashWhite();

            gameFailed();
          //  reset();
         //   momentaryComm(2000); // Two second pause before restarting the game
            return;
        }
        
        if (lastLitSequencePosition < currentSequencePosition) {
            if (lastLitSequencePosition >= 0) {
                posts[postSequence[lastLitSequencePosition]]->light({0,0,0});
            }
            lastLitSequencePosition = currentSequencePosition;
            posts[postSequence[currentSequencePosition]]->setCurrentColorAndLight(rgbColorforFishSpeciesColorId[colorSequence[currentSequencePosition]]);
        }
    }

    bool scanPostsForFish() {
        bool foundAtLeastOneNewTag = false;
        for (uint8_t i=0; i<NUM_POSTS; i++) {
            foundAtLeastOneNewTag = posts[i]->scanForFishAndSaveCurrent() || foundAtLeastOneNewTag;
        }
        return foundAtLeastOneNewTag;
    }
    
//
// TODO: Only seek on post 1 (closest to tree/porch)
    void seekStartFish() {
        unsigned long now = millis();
#ifdef DO_DEBUG
    //    Serial.println("seekStartFish");
#endif
        // Check each post for game start fish
        for (uint8_t i=0; i<NUM_POSTS; i++) {
            if(posts[i]->isFishPresent()) {
      Serial.println(F("FISH PRESENT"));
                // Check the timeout for last update from the reader. Clear if we have exceeded the timeout.
                // This is the amount of time that the tag needs to be off the reader before we acknowledge it.
                // Need to account for random-ness of when we get updates from the card relative to how often
                // we "loop".
                if ((now - posts[i]->getTimeLastUpdated()) > 1000 ) {
                    posts[i]->clearCurrentTag();
                    gameStatusChanged = true;
                } else if (!gameInProgress && (posts[i]->currentFishMatches(&gameStartTag[0]) || posts[i]->currentFishMatches(&gameStartTag[1]))) {
                        gameInProgress = true;
        #ifdef DO_DEBUG
                        Serial.println("Game Started (detected game start fish)");
        #endif
                    break;
                }
            }
        }
    }
  
  void clearUsedFish() {
  Serial.println(F("CLEARING USED FISH"));
      for(int i=0;i<numFishSpecies;i++) {
        fishSpecies[i]->setNotUsed();
    }
  }
  
  
  void foundCorrectFishOnPost(RFIDTag *foundFish) {
#ifdef DO_DEBUG
            Serial.print("FOUND CORRECT FISH!!! on post ");
            Serial.println(postSequence[currentSequencePosition]);
#endif

      // Store this fish as we cannot use it again
            setFishAlreadyUsed(foundFish);

            gameStatusChanged = true;
            currentSequencePosition++;
			
	//		delay(500);  // Delay a half second as that will indicate fish removed. What about if it's just lying there?
            lastLightTime = millis();
      
  }
  
  void setFishAlreadyUsed(RFIDTag *foundFish) {
      for(int i=0;i<numFishSpecies;i++) {
        if(fishSpecies[i]->setAlreadyUsedIfMatches(foundFish)) {
         break;
      }
    }
  }
  
  bool isFishAlreadyUsed(RFIDTag *foundFish) {
      bool ret = false;
    
      for(int i=0;i<numFishSpecies;i++) {
        if(fishSpecies[i]->isAlreadyUsed(foundFish)) {
         ret = true;
         break;
      }
    }
     
      return ret;
  }

    void gameCompleted() {
  
       posts[0]->flashColor(rgbColorforFishSpeciesColorId[fishSpeciesColorId::green]);
       posts[1]->flashColor(rgbColorforFishSpeciesColorId[fishSpeciesColorId::green]);
       posts[2]->flashColor(rgbColorforFishSpeciesColorId[fishSpeciesColorId::green]);

       reset();
       gameInProgress = false;
	   numberOfAttempts = 0;
       sendControllerImportantEvent(CE_PUZZLE_COMPLETED,CE_PUZZLE_COMPLETED_SUCCESS,FISH_SORTING_GAME_NODE);


    } 

    void updateGameStatus(bool somethingNewHappened) {
        gameStatusChanged = false;
        unsigned long now = millis();
		
//		Serial.println(F("UpdateGameStatus================================================================"));
        
        for (int p=0; p<NUM_POSTS; p++) {
            doComm();
            //skip to the next post if nothing on this post (and/or has been cleared or previsouly used)
            if(!posts[p]->isFishPresent()) {
                continue;
            }
            
            //skip to the next post if fish was removed
            // Check the timeout for last update from the reader. Clear if we have exceeded the timeout.
            // This is the amount of time that the tag needs to be off the reader before we acknowledge it.
            // Need to account for random-ness of when we get updates from the card relative to how often
            // we "loop".
			Serial.print(F("LastUpdateTime="));Serial.println(now-posts[p]->getTimeLastUpdated());
            if ((now - posts[p]->getTimeLastUpdated()) > 500 ) {
                posts[p]->clearCurrentTag();
                gameStatusChanged = true;
        Serial.println(F("================TAG HAS BEEN REMOVED"));
                continue;
            }
      
        if(somethingNewHappened) {

      // If this fish was already used then we can just ignore it
             if(isFishAlreadyUsed(posts[p]->currentFishTag())) {
                Serial.println(F("We should be skipping this fish as it's already used"));
                posts[p]->currentFishTag()->printIdToSerial();
         // The that post a quick color so you know it's a bad fish
                posts[p]->quickDoubleFlashColor(alreadyUsedFishColor);
               continue;
             }
            
            //A fish is on this post. Check if this is the post we're expecting.
              if (p != postSequence[currentSequencePosition]) {
                //A fish is present on the wrong post
#ifdef DO_DEBUG
                Serial.print("Found fish on wrong post: ");
                Serial.print(p);
                Serial.print(" instead of ");
                Serial.println(postSequence[currentSequencePosition]);
                Serial.println("FIXME: reset game here **********************************");
#endif
                //@FIXME: reset game here
                gameFailed();
                gameStatusChanged = true;
                continue;
              }
        

            
            //A fish is present on the correct post. See if it's the right species (color)
              if (!fishSpecies[colorSequence[currentSequencePosition]]->isKnownTag(posts[p]->currentFishTag())) {
#ifdef DO_DEBUG
                Serial.print("Found incorrect fish on correct post ");
                Serial.println(postSequence[currentSequencePosition]);
                Serial.println("FIXME: reset game here **********************************");
#endif
                //@FIXME: reset game here
        
                gameFailed();
                gameStatusChanged = true;
                continue;
              }
      
              foundCorrectFishOnPost(posts[p]->currentFishTag());
              break;
      } // End somethingNewHappened
        }
        
        // We have a WINNER if we go past the final sequence
        if (currentSequencePosition >= SOLUTION_SEQUENCE_LENGTH-1) { // && ((millis() - lastLightTime) > gracePeriod)) {
#ifdef DO_DEBUG
            Serial.println("WINNER, WINNER, WINNER!");
            Serial.println("FIXME: Tell the room this puzzle has been solved **********************************");
#endif
            gameCompleted();
            return;
        }
    }
    
    bool getGameStatusChanged() {
        return gameStatusChanged;
    }
 
////////////////////////////////////////////////
// Create a random solution with the following
// requirements:
// 1) The same pole is never used twice in a row
// 2) There is never more than 3 of the same fish used in the solution   
  void initRandomGameSequence() {
      int lastPostPicked = -1;
    int maxFishPerSpiecesInSolution = 3;
    unsigned int numRed = 0,numYellow=0,numBlue=0,numGreen=0;
      for(int i=0;i<SOLUTION_SEQUENCE_LENGTH;i++) {
        int pval = random(0,3);  // pick a post
      while(lastPostPicked == pval) {
               pval = random(0,3);      
      }
      lastPostPicked = pval;
      postSequence[i] = pval;
      bool pickAgain = true;
      while(pickAgain) {
         pval = random(0,4); // Pick a random color
         if((pval == 0 && numRed == maxFishPerSpiecesInSolution) ||
            (pval == 1 && numYellow == maxFishPerSpiecesInSolution) ||
            (pval == 2 && numGreen == maxFishPerSpiecesInSolution) ||
            (pval == 3 && numBlue == maxFishPerSpiecesInSolution)) {
            pickAgain = true;
         } else {
            pickAgain = false;
         }
      }
      switch(pval) {
         case 0:
            colorSequence[i] = red;
          numRed++;
         break;
         case 1:
            colorSequence[i] = yellow;
          numYellow++;
         break;
         case 2:
            colorSequence[i] = green;
          numGreen++;
         break;
         case 3:
            colorSequence[i] = blue;
          numBlue++;
         break;
      }
//      Serial.print(F("Solution: ")); Serial.print(i);Serial.print(F(" post="));Serial.print(postSequence[i]);
//      Serial.print(F(" color="));Serial.println(colorSequence[i]);
    }

  }
  
    void activateGame() {
        gameActivated = true;
    }
	
    
    void processGameLoop() {
        //Do nothing if the room has not activated this game yet
        if (!gameActivated) {
            return;
        }
        
        //Check each post for the presence of a fish (RFID tag)
        gameStatusChanged = scanPostsForFish();
        doComm();
//#ifdef DO_DEBUG
//        gameStatusChanged ? Serial.println(" Found a new fish!") : (0) ;
//#endif
        
        //If the game is activated but not yet started, check for the special game start fish (RFID tag)
        if (!gameInProgress) {
            seekStartFish();
            return;
        }
        lightNextPost();
    
//    Serial.print(F("GameStatusChanged="));Serial.println(gameStatusChanged);
      
    doComm();
//        Serial.print("----------------------------------------GAME STATUS CHANGED=");Serial.println(gameStatusChanged);
        updateGameStatus(gameStatusChanged);
    
    }


// TODO: Delete this
 int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
    void doTestMode() {
        for (uint8_t i=0; i<NUM_POSTS; i++) {
          doComm();
           if( posts[i]->testReaderStatus()) {
              // Light the post to indicate working
              posts[i]->quickDoubleFlashColor(alreadyUsedFishColor);
              Serial.println(freeRam());
           }
        }
  
    }

    void printCurrentGameStatus() {
        Serial.print(F("Fish Sorting Game ("));
        Serial.print((int)this,HEX);
        Serial.print(F("): Active? "));
        Serial.print(gameActivated);
        Serial.print(F(" InProgress? "));
        Serial.print(gameInProgress);
        Serial.print(F(" StatusChanged? "));
        Serial.print(gameStatusChanged);
        Serial.print(F(" Sequence? "));
        Serial.print(currentSequencePosition);
        Serial.print(F("/"));
        Serial.print(SOLUTION_SEQUENCE_LENGTH);
        Serial.print(F(" GracePeriod? "));
        Serial.print(millis()-lastLightTime);
        Serial.print(F("/"));
        Serial.print(gracePeriod);
        Serial.println();
    
    for(int i=0;i<NUM_FISH_SPECIES;i++) {
       fishSpecies[i]->printUsedTagStatus();
    }
    Serial.println();
    }
};


// **********************************************************************************
// Reset and other non class methods
// **********************************************************************************
FishSortingGame* fsGame; // This is the main game object

RFIDTag tagsForFishSpecies[NUM_FISH_SPECIES][NUM_FISH_PER_SPECIES] = {
    { RFIDTag(0x3B, 0x03, 0xFE, 0xC4), RFIDTag(0x6B, 0xA0, 0xFF, 0xC4), RFIDTag(0x6B, 0xBC, 0xFD, 0xC4), RFIDTag(0x5B, 0xB0, 0xFF, 0xC4) },   // red fish ID's
    { RFIDTag(0xAB, 0xB2, 0x06, 0xC5), RFIDTag(0x6B, 0xED, 0xFD, 0xC4), RFIDTag(0x8B, 0x4C, 0x08, 0xC5), RFIDTag(0x5B, 0x57, 0x21, 0xC5) },   // yellow fish ID's
    { RFIDTag(0x3B, 0x7E, 0x06, 0xC5), RFIDTag(0xDB, 0xA1, 0x06, 0xC5), RFIDTag(0xCB, 0x50, 0x21, 0xC5), RFIDTag(0xBB, 0x65, 0xFD, 0xC4) },   // green fish ID's
    { RFIDTag(0xCB, 0xD6, 0x06, 0xC5), RFIDTag(0x8B, 0xF8, 0x06, 0xC5), RFIDTag(0x5B, 0xE8, 0x06, 0xC5), RFIDTag(0x9B, 0xCF, 0x00, 0xC5) }    // blue fish ID's
};

// TODO: Delete this
 int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void resetNode(bool playResetTrack) {
    //This should reset self plus all connected game nodes (if any).

#ifdef DO_DEBUG
    Serial.print("Reset Fish Sorting Game data: ");
    Serial.println(eventData.data);
#endif
    testModeOnly = false;

  // TODO: DO we need this here? Probabaly but can remove from initialize
//    fsGame->reset();
  
  if(playResetTrack) {
       playTrack("reset50");
  }

Serial.print(F("FreeRam="));Serial.println(freeRam());
    
}

void doStartGame() {
    resetNode(false);
    sendPuzzleStartSuccess();
 //   momentaryComm(1000);
    playTrack("start50"); // If we want to hear "game started"
//  playTrack("track30");
#ifndef SHORT_TRACK_START
//  momentaryComm(60000);
  playTrack("track30a");
//  momentaryComm(50000);
  playTrack("track30b"); // This one is 23 seconds long
#endif
  fsGame->activateGame();
}

void receiveCommEvent() {
    //use eventData.whatever to get what was sent and switch
    switch (eventData.event) {
         case CE_RESET_NODE:
              resetNode(true);
          break;
          case CE_START_PUZZLE: 
          case CE_RESET_AND_START_PUZZLE:
             testModeOnly = false;
             doStartGame();   // Indicates to call all the game start stuff next loop
          break;
          case CE_TEST_MODE:
            Serial.println(F("---------------GOING INTO TEST MODE---------------"));
            Serial.println(freeRam());
             testModeOnly = true;
          break;
    }
}


 void momentaryComm(unsigned long howlong) {
  
#ifdef DO_DEBUG
// Serial.print(F("------Momentary Comm START----"));Serial.println(howlong);
#endif
    unsigned long start = millis();
    while(millis()-start < howlong) {
      doComm();
    }
  
#ifdef DO_DEBUG
 //Serial.println(F("------Momentary Comm END----"));
#endif
  }
  
///////////////////////////////////////////////////////////////////
// Initialize game
 void initializeFishSortingGame () {
#ifdef DO_DEBUG
    Serial.println("\nBegin Fish Sorting Game initialization.");
#endif
    //Spawn fish for each Species. Each species (color) has multiple fish (RFID tags).
    FishSpecies *newSpecies = NULL;
    for (int i=0; i<NUM_FISH_SPECIES; i++) {
        newSpecies = new FishSpecies(i);
#ifdef DO_DEBUG
        Serial.flush();
        Serial.print("Adding ");
        newSpecies->printToSerial();
        Serial.print(" of ");
        Serial.print(NUM_FISH_SPECIES);
        Serial.print(" fish species with ");
        Serial.print(NUM_FISH_PER_SPECIES);
        Serial.println(" fish");
#endif
        for (int j=0; j<NUM_FISH_PER_SPECIES; j++) {
//            newSpecies->addKnownTag(new RFIDTag(tagIdsForFishSpeciesColorId[i][j], 4)); // 4 bytes in each RFID tag
            newSpecies->addKnownTag(&tagsForFishSpecies[i][j]);
        }
        fsGame->addFishSpecies(newSpecies);
    }
    fsGame->reset();
#ifdef DO_DEBUG
    Serial.println("End Fish Sorting Game initialization.");
#endif
}

// **********************************************************************************
// Setup
// **********************************************************************************


void setup() {
    // Initialize serial communications with the PC
    Serial.begin(9600);
    SPI.begin(); // Init SPI bus to communicate with RFID cards
    while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
    
#ifdef DO_DEBUG
    Serial.println(F("Setup started."));
#endif

    fsGame = new FishSortingGame();
    fsGame->setMomentaryComm(momentaryComm);

    initOverrideComm(FISH_SORTING_GAME_NODE,FISH_SORTING_COMM_PIN); 
    setLocalEventHandler(receiveCommEvent);

  // Initialize random seed value. Using pin 0. It is assumed that this pin is not used.
  // Doing this allows the random method to return a random sequence from one "run" of the
  // Arduino to the next.
    randomSeed(analogRead(0));

    initializeFishSortingGame();
    resetNode(true);

#ifdef GAME_ACTIVATE_EVENT_NOT_REQUIRED
//    fsGame->activateGame();
#endif
    
#ifdef DO_DEBUG
    Serial.println("Setup complete.\n");
#endif
 //        sendIntEventToNode(GAME_CONTROLLER_NODE,CE_PING, FISH_SORTING_GAME_NODE);
}




void loop() {

#ifdef DO_GAME_EVENT_COMM
    doComm();
#endif
    
    if(!testModeOnly) {
        fsGame->processGameLoop();
      
#ifdef DO_DEBUG
      if ( fsGame->getGameStatusChanged() ) {   //|| (millis() - fsGame->lastPrintTime) > 5000) {
          fsGame->printCurrentGameStatus();
          fsGame->lastPrintTime = millis();
      }
#endif
    } else {

       fsGame->doTestMode();
    }
 
}


/***************
* Play Track. 
* Just a convenience to send a play track event
**************/
  void playTrack(String track) {
//      sendEventToController(CE_PLAY_TRACK ,rack);  
      Serial.println(F("Sending event"));

         sendEventToNode(MP3_PLAYER_NODE,CE_PLAY_TRACK, track);
      
 
  }
  
