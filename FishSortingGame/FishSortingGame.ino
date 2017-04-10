#include <PJON.h>
#include <GameCommUtils.h>
#include <SPI.h>
#include <MFRC522.h>

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
    #include <avr/power.h>
#endif

using namespace std;

// Uncomment to enable Serial Debug Prints
#define DO_DEBUG

// Uncomment to enable event communications
//#define DO_GAME_EVENT_COMM

// Comment this if the game requires a start event to be active
#define GAME_ACTIVATE_EVENT_NOT_REQUIRED


#define READER_INITIAL_RFID_NSS_PIN   4  // RFID pin numbers are sequential starting with this value.
#define RST_PIN                      10  // RFID reset pin. Configurable but shared by all RFID readers
#define PIXEL_INITIAL_PIN            11  // Pixel pin numbers are sequential starting with this value.

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

static const byte tagIdsForFishSpeciesColorId[NUM_FISH_SPECIES][NUM_FISH_PER_SPECIES][4] = {
    { {0x3B, 0x03, 0xFE, 0xC4}, {0x6B, 0xA0, 0xFF, 0xC4}, {0x6B, 0xBC, 0xFD, 0xC4}, {0x5B, 0xB0, 0xFF, 0xC4} },   // red fish ID's
    { {0xAB, 0xB2, 0x06, 0xC5}, {0x6B, 0xED, 0xFD, 0xC4}, {0x8B, 0x4C, 0x08, 0xC5}, {0x5B, 0x57, 0x21, 0xC5} },   // yellow fish ID's
    { {0x3B, 0x7E, 0x06, 0xC5}, {0xDB, 0xA1, 0x06, 0xC5}, {0xCB, 0x50, 0x21, 0xC5}, {0xBB, 0x65, 0xFD, 0xC4} },   // green fish ID's
    { {0xCB, 0xD6, 0x06, 0xC5}, {0x8B, 0xF8, 0x06, 0xC5}, {0x5B, 0xE8, 0x06, 0xC5}, {0x9B, 0xCF, 0x00, 0xC5} }    // blue fish ID's
};

byte tagIdsForGameStartFish[NUM_FISH_PER_SPECIES][4] = {
    {0x7A, 0x31, 0x41, 0xD5}, {0xFF, 0xFF, 0xFF, 0xFF}   // game start fish ID's
};

// **********************************************************************************
// Pixel LED Light String
// **********************************************************************************
class PixelLightString {
    const static int flashDuration = 50;
    
    int lightsPerString;
    int pin;
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
    
};


// **********************************************************************************
// RFID Tag
// **********************************************************************************
class RFIDTag {
    
    const static byte MAX_ID_BYTES = 4;
    
    byte size = 0;
    byte id[MAX_ID_BYTES] = { 0, 0, 0, 0 };  // 4 max
    bool clearFlag = true;
    
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
//#ifdef DO_DEBUG
//        Serial.print(F("Comparing tags: "));
//        Serial.print(size);
//        Serial.print(F("=="));
//        Serial.print(tag.size);
//        Serial.print(F(", "));
//        Serial.print(clearFlag);
//        Serial.print(F("=="));
//        Serial.print(tag.clearFlag);
//        Serial.print(F(","));
//        printIdToSerial();
//        Serial.print(F(" =="));
//        tag.printIdToSerial();
//        Serial.println();
//#endif

        return (size == tag.size) &&
        (clearFlag == tag.clearFlag) &&
        (memcmp(id, tag.id, size) == 0);
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
    }

    bool scanForFishAndSaveCurrent() {
        if (rfidReader->isTagDataAvailable()) {
            return rfidReader->processTagUpdate(&rfidReader->uid);
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
};

// **********************************************************************************
// Fish Sorting Game
// **********************************************************************************
class FishSortingGame {
    const static int NUM_POSTS = 3;
    const static int SOLUTION_SEQUENCE_LENGTH = 10;
    const static unsigned long int INITIAL_GRACE_PERIOD = 50000; // millis to wait for a fish to be placed on post
    
    unsigned long int gracePeriod = INITIAL_GRACE_PERIOD;
    bool gameStatusChanged = false;
    bool gameActivated = false; //false until the room tells us we're ready to place via local event
    bool gameInProgress = false; //false until we detect a game start fish on post 0. Stays true until game is solved or is failed

    RFIDTag* gameStartTag[2] = { new RFIDTag(tagIdsForGameStartFish[0], 4), new RFIDTag(tagIdsForGameStartFish[1], 4) }; // 4 bytes in each RFID tag
    
    int postSequence[SOLUTION_SEQUENCE_LENGTH] = {0, 1, 2, 0, 1, 2, 0, 1, 2, 0};
    fishSpeciesColorId colorSequence[SOLUTION_SEQUENCE_LENGTH] = { red, yellow, green, blue, blue, green, yellow, red, red, green };
    
    int currentSequencePosition = 0;
    int lastLitSequencePosition = -1;
    unsigned long lastLightTime = 0;
    
    FishSpecies *fishSpecies[NUM_FISH_SPECIES];
    int numFishSpecies = 0; //Number of Fish known to this game
    
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
        
        lastLitSequencePosition = -1;
        currentSequencePosition = 0;
        lastLightTime = 0;
        gameStatusChanged = false;
        for (int i=0; i<NUM_POSTS; i++) {
            posts[i]->reset();
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
        delay(500);
        
        //Set each individual light to black (off)
        for(int i=0; i<NUM_POSTS; i++) {
            posts[i]->pixel()->setColor({0,0,0});
        }
        for(int i=0; i<NUM_POSTS; i++) {
            posts[i]->pixel()->show();
        }
        delay(250);
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
                flashAllPosts(rgbColorforFishSpeciesColorId[green]);
            }
            lastLightTime = millis();
            return;
        }
        
        if ((millis() - lastLightTime) > gracePeriod) {
#ifdef DO_DEBUG
            Serial.println("Timeout. No correct fish scanned within grace period.");
#endif
            posts[postSequence[currentSequencePosition]]->flashColor(rgbColorforFishSpeciesColorId[red]);
            reset();
            delay(2000); // Two second pause before restarting the game
            return;
        }
        
        if (lastLitSequencePosition < currentSequencePosition) {
            if (lastLitSequencePosition >= 0) {
                posts[postSequence[lastLitSequencePosition]]->light({0,0,0});
            }
            lastLitSequencePosition = currentSequencePosition;
            posts[postSequence[currentSequencePosition]]->light(rgbColorforFishSpeciesColorId[colorSequence[currentSequencePosition]]);
        }
    }

    bool scanPostsForFish() {
        bool foundAtLeastOneNewTag = false;
        for (uint8_t i=0; i<NUM_POSTS; i++) {
            foundAtLeastOneNewTag = posts[i]->scanForFishAndSaveCurrent() || foundAtLeastOneNewTag;
        }
        return foundAtLeastOneNewTag;
    }
    
    void seekStartFish() {
        unsigned long now = millis();
#ifdef DO_DEBUG
        //Serial.println("seekStartFish");
#endif
        // Check each post for game start fish
        for (uint8_t i=0; i<NUM_POSTS; i++) {
            if(posts[i]->isFishPresent()) {
                // Check the timeout for last update from the reader. Clear if we have exceeded the timeout.
                // This is the amount of time that the tag needs to be off the reader before we acknowledge it.
                // Need to account for random-ness of when we get updates from the card relative to how often
                // we "loop".
                if ((now - posts[i]->getTimeLastUpdated()) > 500 ) {
                    posts[i]->clearCurrentTag();
                    gameStatusChanged = true;
                } else if (!gameInProgress && (posts[i]->currentFishMatches(gameStartTag[0]) || posts[i]->currentFishMatches(gameStartTag[1]))) {
                        gameInProgress = true;
        #ifdef DO_DEBUG
                        Serial.println("Game Started (detected game start fish)");
        #endif
                    break;
                }
            }
        }
    }

    void updateGameStatus() {
        gameStatusChanged = false;
        unsigned long now = millis();
        
        for (int p=0; p<NUM_POSTS; p++) {
            //skip to the next post if nothing on this post (and/or has been cleared)
            if(!posts[p]->isFishPresent()) {
                continue;
            }
            
            //skip to the next post if fish was removed
            // Check the timeout for last update from the reader. Clear if we have exceeded the timeout.
            // This is the amount of time that the tag needs to be off the reader before we acknowledge it.
            // Need to account for random-ness of when we get updates from the card relative to how often
            // we "loop".
            if ((now - posts[p]->getTimeLastUpdated()) > 500 ) {
                posts[p]->clearCurrentTag();
                gameStatusChanged = true;
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
                gameStatusChanged = true;
                continue;
            }
#ifdef DO_DEBUG
            Serial.print("FOUND CORRECT FISH!!! on post ");
            Serial.println(postSequence[currentSequencePosition]);
#endif
            gameStatusChanged = true;
            currentSequencePosition++;
            lastLightTime = millis();
            break;
        }
        
        // We have a WINNER if we go past the final sequence
        if (currentSequencePosition >= SOLUTION_SEQUENCE_LENGTH-1) { // && ((millis() - lastLightTime) > gracePeriod)) {
#ifdef DO_DEBUG
            Serial.println("WINNER, WINNER, WINNER!");
            Serial.println("FIXME: Tell the room this puzzle has been solved **********************************");
#endif
            //@FIXME: Tell the room this game has been solved
            posts[0]->flashColor(rgbColorforFishSpeciesColorId[fishSpeciesColorId::green]);
            posts[1]->flashColor(rgbColorforFishSpeciesColorId[fishSpeciesColorId::green]);
            posts[2]->flashColor(rgbColorforFishSpeciesColorId[fishSpeciesColorId::green]);

            reset();
            gameInProgress = false;
            return;
        }
    }
    
    bool getGameStatusChanged() {
        return gameStatusChanged;
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
//#ifdef DO_DEBUG
//        gameStatusChanged ? Serial.println(" Found a new fish!") : (0) ;
//#endif
        
        //If the game is activated but not yet started, check for the special game start fish (RFID tag)
        if (!gameInProgress) {
            seekStartFish();
            return;
        }
        lightNextPost();
        updateGameStatus();
    }

    void printCurrentGameStatus() {
        Serial.print("Fish Sorting Game (");
        Serial.print((int)this,HEX);
        Serial.print("): Active? ");
        Serial.print(gameActivated);
        Serial.print(" InProgress? ");
        Serial.print(gameInProgress);
        Serial.print(" StatusChanged? ");
        Serial.print(gameStatusChanged);
        Serial.print(" Sequence? ");
        Serial.print(currentSequencePosition);
        Serial.print("/");
        Serial.print(SOLUTION_SEQUENCE_LENGTH);
        Serial.print(" GracePeriod? ");
        Serial.print(millis()-lastLightTime);
        Serial.print("/");
        Serial.print(gracePeriod);
        Serial.println();
    }
};
FishSortingGame* fsGame; // This is the main game object

// **********************************************************************************
// Reset
// **********************************************************************************
void reset() {
    //This should reset self plus all connected game nodes (if any).

#ifdef DO_DEBUG
    Serial.print("Reset Fish Sorting Game data: ");
    Serial.println(eventData.data);
#endif
    
    fsGame->reset();
    
}

// **********************************************************************************
// Comm Routine
// **********************************************************************************
void receiveCommEvent() {
    //use eventData.whatever to get what was sent and switch
    switch (eventData.event) {
        case CE_RESET_NODE:
            reset();
            break;
        default:
            ;
    }
}

// **********************************************************************************
// Sample Error Handler
// **********************************************************************************
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
            newSpecies->addKnownTag(new RFIDTag(tagIdsForFishSpeciesColorId[i][j], 4)); // 4 bytes in each RFID tag
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

    initializeFishSortingGame();

#ifdef GAME_ACTIVATE_EVENT_NOT_REQUIRED
    fsGame->activateGame();
#endif
    
#ifdef DO_DEBUG
    Serial.println("Setup complete.\n");
#endif
}

// **********************************************************************************
// Main Loop
// **********************************************************************************
void loop() {
#ifdef DO_GAME_EVENT_COMM
    doComm();
#endif
    
    fsGame->processGameLoop();
    
#ifdef DO_DEBUG
    if ( fsGame->getGameStatusChanged() || (millis() - fsGame->lastPrintTime) > 5000) {
        fsGame->printCurrentGameStatus();
        fsGame->lastPrintTime = millis();
    }
#endif
    
}

// **********************************************************************************
// Helper routine to dump a byte array as hex values to Serial.
// **********************************************************************************
void dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
}
