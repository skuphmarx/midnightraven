
typedef byte pot_pos_t;

// Uncomment for potentiometer stream values
//#define DEBUG_POT_STREAM

// Potentiometer Pins
#define POT_PIN_A 2
#define POT_PIN_B 3

// This is the number of potentiometer "clicks" per dial position.
#define NUM_POT_DELTA 10

#define LOCK_BUTTON A0
#define RESET_BUTTON A1

// TEMP
bool pFirst = false;
unsigned long timeLastDebug = millis();
pot_pos_t oldEncPos = 0; //

int freeRam();

/**
 * Represents the raw potentiometer. No application specific logic.
 * 
 * The way that the overall logic works is to look at the stream of interrupt data for specific series of events to determine a change of
 * state. Much of the stream of data is ignored in some cases. The MAX_DEBUNK_MILLIS is used to narrow the set of stream data that is used
 * to determine state change. MAX_DEBUNK_MILLIS should also help with "turning too fast."
 * 
 * There are 3 specific data stream series. The series must occur with MAX_DEBUNK_MILLIS.
 * 
 * 1) Decreasing:
 *    PinB - B LOW, A LOW
 *    PinA - A HIGH, B HIGH
 *    
 * 2) Increasing:
 *    PinA - A HIGH, B HIGH
 *    PinB - B HIGH, A HIGH
 *    
 * 3) Increasing - this series overrides series 1 by removing the decrease and replacing with increase.:
 *    PinB - B LOW, A LOW
 *    PinA - A HIGH, B HIGH
 *    PinB - B HIGH, A HIGH
 */
class Potentiometer {

    int MAX_DEBUNK_MILLIS = 100;   // Noise filter

    pot_pos_t currentPosition = 0;
    pot_pos_t previousPosition = 0;
    bool increasing = true;

    int encoderPinA;
    int encoderPinB;

    byte pinAFlag = 0; // When 1, we are expecting pinA - decreasing
    byte pinBFlag = 0; // When 1, we are expecting pinB - increasing

    long pinAExcpetedMillis = 0; // The timestamp when pinA expected, pinAFlag, was set.
    long pinBExcpetedMillis = 0; // The timestamp when pinB expected, pinBFlag, was set.

    boolean pinBMaybe = false; // PinB increasing can look like a PinA decreasing. True tells us to look for that possibility.
    long pinBMaybeMillis = 0;  // The timestamp of when the pinBMaybe flag was set true.

    byte increaseCount = 0;
    byte decreaseCount = 0;
    byte series3Increment = 0;

  public:

    /**
     * Pot pins input.
     */
    Potentiometer(int pinA, int pinB) {
      this->encoderPinA = pinA;
      this->encoderPinB = pinB;

      pinMode(pinA, INPUT_PULLUP); // set pinA as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)
      pinMode(pinB, INPUT_PULLUP); // set pinB as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)

      digitalWrite(pinA, HIGH);  // turn on pull-up resistor
      digitalWrite(pinB, HIGH);  // turn on pull-up resistor

    }

    pot_pos_t getCurrentPosition() {
      return currentPosition;
    }

    pot_pos_t getPreviousPosition() {
      return previousPosition;
    }

    /**
     * Returns true when turning one direction, and false when turning the other. As coded, true/increasing means counter clockwise, 
     * false/decreasing is clockwise.
     * The increasing/decreasing refers to the potentiometer position: 0-MaxValue. MaxValue is determined by the type of the variable currentPosition.
     * The currentPosition value rolls over to 0 once the MaxValue+1 is reached. Likewise the currentPosition value rolls over to MaxValue once
     * 0-1 is reached.
     */
    bool isIncreasing() {
      return increasing;
    }

    /**
     * PinA interupt processing. Completes Series 1. Starts Series 2 and Series 3.
     */
    void processInteruptPinA() {

      long now = millis();
      boolean A_set = digitalRead(encoderPinA) == HIGH;
      boolean B_set = digitalRead(encoderPinA) == HIGH;

 #ifdef DEBUG_POT_STREAM
     Serial.print(F("A B "));
      Serial.print(A_set);
      Serial.print(F(" "));
      Serial.print(B_set);
      Serial.print(F(" "));
      Serial.println(now);
#endif

      // Series 1
      if ((A_set && B_set) && pinAFlag == 1 && ((now - pinAExcpetedMillis) < MAX_DEBUNK_MILLIS)) {

        series3Increment = 0;
        decreaseCount++;
        if (decreaseCount >= 2) {
          
          decreaseCount = 2;
          increaseCount = 0;
        
          previousPosition = currentPosition;
          currentPosition--;
          series3Increment = 1;
          pinAFlag = 0;
          pinBFlag = 0;
          increasing = false;
          pinAExcpetedMillis = 0;
          pinBExcpetedMillis = 0;

          Serial.println(F("SET Decreasing"));

        }

        // Look out it might be series 3
        pinBMaybe = true;
        pinBMaybeMillis = now;
        

      // Starting series 2
      } else if (A_set && B_set && pinBFlag == 0) {
        pinBFlag = 1;
        pinBExcpetedMillis = now;
      }

    }


    /**
     * PinB interupt processing. Completes Series 2 and Series 3. Starts Series 1.
     */
    void processInteruptPinB() {

      long now = millis();
      boolean A_set = digitalRead(encoderPinA) == HIGH;
      boolean B_set = digitalRead(encoderPinA) == HIGH;

#ifdef DEBUG_POT_STREAM
      Serial.print(F("B A "));
      Serial.print(B_set);
      Serial.print(F(" "));
      Serial.print(A_set);
      Serial.print(F(" "));
      Serial.println(now);
#endif

      // Series 2
      if ((B_set && A_set) && pinBFlag == 1 && ((now - pinBExcpetedMillis) < MAX_DEBUNK_MILLIS)) {

        increaseCount++;
        if (increaseCount > 1) {

          increaseCount = 2;
          decreaseCount = 0;
        
          previousPosition = currentPosition;
          currentPosition++;
          pinAFlag = 0;
          pinBFlag = 0;
          increasing = true;
          pinAExcpetedMillis = 0;
          pinBExcpetedMillis = 0;

          pinBMaybe = false;
          pinBMaybeMillis = 0;

          Serial.println(F("SET Increasing"));

          
        }


      // Series 3
      } else if ((B_set && A_set) && pinBMaybe && ((now - pinBMaybeMillis) < MAX_DEBUNK_MILLIS)) {


        increaseCount++;
        if (increaseCount > 1) {

          increaseCount = 2;
          previousPosition = currentPosition;
        
          // Up two because we did a decrease but it turned out to be series 3.
          currentPosition = currentPosition + series3Increment;
          
          currentPosition++;
        
          pinAFlag = 0;
          pinBFlag = 0;
          increasing = true;
          pinAExcpetedMillis = 0;
          pinBExcpetedMillis = 0;

          pinBMaybe = false;
          pinBMaybeMillis = 0;


          Serial.println(F("SET Increasing in maybe"));

          
        }

      // Starting series 1  
      } else if (!B_set && !A_set && pinAFlag == 0) {
        pinAFlag = 1;
        pinAExcpetedMillis = now;
      }

    }

};


class Dial {

    // Need some way to account for a fast spin and we don't get every "click".
    // This saves us if things get really out of whack.
    //uint8_t magicNumber = 254;

    Potentiometer* pot;
    uint8_t numberPositions;
    pot_pos_t numberPotDeltaPerPosition;
    uint8_t dialPosition = 1;
    pot_pos_t dialPotPositionValue = 0;

    bool checkInBoundsIncrease(pot_pos_t potCurrPos) {
      pot_pos_t potCurrPosTemp = potCurrPos;
      pot_pos_t nextExpPos = dialPotPositionValue + numberPotDeltaPerPosition;
      for (pot_pos_t ip = 0; ip < (numberPotDeltaPerPosition * 2); ip++) {
        potCurrPosTemp++;
        if (potCurrPosTemp == nextExpPos) {
          return true;
        }
      }
      return false;
    }

    bool checkInBoundsDecrease(pot_pos_t potCurrPos) {
      pot_pos_t potCurrPosTemp = potCurrPos;
      pot_pos_t nextExpPos = dialPotPositionValue - numberPotDeltaPerPosition;
      for (pot_pos_t ip = 0; ip < (numberPotDeltaPerPosition * 2); ip++) {
        potCurrPosTemp--;
        if (potCurrPosTemp == nextExpPos) {
          return true;
        }
      }
      return false;
    }


  public:

    Dial(Potentiometer* p, uint8_t numDialPositions, pot_pos_t numPotDelta) {
      pot = p;
      numberPositions = numDialPositions;
      numberPotDeltaPerPosition = numPotDelta;
    }

    Potentiometer* getPotentiometer() {
      return pot;
    }

    uint8_t getNumberPositions() {
      return numberPositions;
    }

    pot_pos_t getNumberPotDeltaPerPosition() {
      return numberPotDeltaPerPosition;
    }

    uint8_t getDialPosition() {
      return dialPosition;
    }

    pot_pos_t getDialPotPosition() {
      return dialPotPositionValue;
    }

    void updateDial() {

      pot_pos_t potCurrPos = pot->getCurrentPosition();
      bool increasing = pot->isIncreasing();

      if (dialPotPositionValue != potCurrPos) {

        // Increasing potentiometer values is counter-clockwise - is this a wiring issue?
        if (increasing) {
          pot_pos_t nextExpectedPos = dialPotPositionValue + numberPotDeltaPerPosition;
          if (potCurrPos == nextExpectedPos) {
            dialPotPositionValue = potCurrPos;

            moveDialCounterClockwise();

            Serial.print(F("CounterClockwised position to "));
            Serial.print(dialPosition);
            Serial.print(F(", DialPotPos "));
            Serial.print(dialPotPositionValue);
            Serial.print(F(", NextExpected "));
            Serial.print(nextExpectedPos);
            Serial.print(F(", Pot position is "));
            Serial.println(potCurrPos);

            // What's happening? Spinning like crazy. Just set things to LED 1 and wait for the spinner to settle down.
          } else if (!checkInBoundsIncrease(potCurrPos)) {
            dialPotPositionValue = potCurrPos;
            dialPosition = 1;
          }

        // decreasing
        } else {

          pot_pos_t nextExpectedPos = dialPotPositionValue - numberPotDeltaPerPosition;
          if (potCurrPos == nextExpectedPos) {
            dialPotPositionValue = potCurrPos;

            moveDialClockwise();

            Serial.print(F("Clockwised position to "));
            Serial.print(dialPosition);
            Serial.print(F(", Pot position is "));
            Serial.println(potCurrPos);

            // What's happening? Spinning like crazy. Just set things to LED 1 and wait for the spinner to settle down.
          } else if (!checkInBoundsDecrease(potCurrPos)) {
            dialPotPositionValue = potCurrPos;
            dialPosition = 1;
          }
        }
      }

    }

    uint8_t moveDialClockwise() {
      dialPosition++;
      if (dialPosition > numberPositions) {
        dialPosition = 1;
      }
      return dialPosition;
    }

    uint8_t moveDialCounterClockwise() {
      dialPosition--;
      if (dialPosition == 0) {
        dialPosition = numberPositions;
      }
      return dialPosition;
    }
};

class PushButton {

    uint8_t pinId;
    bool pressed = false;
    bool statusChanged = false;

  public:

    PushButton(uint8_t pId) {
      pinId = pId;
      pinMode(pinId, INPUT);
    }

    bool isStatusChanged() {
      return statusChanged;
    }

    bool isPressed() {
      return (HIGH == digitalRead(pinId));
    }

    void updateStatus() {
      statusChanged = false;
      int pushStatus = digitalRead(pinId);
      if (pressed && (pushStatus == LOW)) {
        pressed = false;
        statusChanged = true;
      } else if (!pressed && (pushStatus == HIGH)) {
        pressed = true;
        statusChanged = true;
      }
    }

};

class LightEmittingDiode {

    uint8_t pinId;

  public:

    LightEmittingDiode(uint8_t pId) {
      pinId = pId;
      pinMode(pinId, OUTPUT);
    }

    uint8_t getPinId() {
      return pinId;
    }

    void turnOn() {
      digitalWrite(pinId, HIGH);
    }

    void turnOff() {
      digitalWrite(pinId, LOW);
    }

    void flash(int delayMs) {
      turnOn();
      delay(delayMs);
      turnOff();
    }


};

class LEDPair {

    LightEmittingDiode* led1;
    LightEmittingDiode* led2;

  public:

    LEDPair(LightEmittingDiode* diode1, LightEmittingDiode* diode2) {
      led1 = diode1;
      led2 = diode2;
    }

    void turnOnLed1() {
      led1->turnOn();
    }

    void turnOffLed1() {
      led1->turnOff();
    }

    void turnOnLed2() {
      led2->turnOn();
    }

    void turnOffLed2() {
      led2->turnOff();
    }

    void turnOn() {
      turnOnLed1();
      turnOnLed2();
    }

    void turnOff() {
      turnOffLed1();
      turnOffLed2();
    }

    void flash() {
      turnOn();
      delay(500);
      turnOff();
    }

};

class ShipWheelGame {

    const static uint8_t MAX_LED_PAIRS = 8;

    LEDPair* ledPairs[MAX_LED_PAIRS];
    uint8_t numLedPairs = 0;

    Dial* gameDial;
    PushButton* lockButton;
    PushButton* resetButton;

    byte dialPos = 99;

    void updateGameDialStatus() {
      gameDial->updateDial();
      if (dialPos != gameDial->getDialPosition()) {
        turnOffAllLed1();
        dialPos = gameDial->getDialPosition();
        if (dialPos > 0 && dialPos <= numLedPairs) {
          Serial.print(F("Dial position "));
          Serial.println(dialPos);
          turnOnLed1(dialPos - 1);
        } else {
          dialPos = 99;
        }
      }
    }

    void updateButtonStatus() {
      lockButton->updateStatus();
      resetButton->updateStatus();

      if (lockButton->isPressed() && lockButton->isStatusChanged()) {
        Serial.println(F("Lock button pressed."));

        if (dialPos > 0 && dialPos <= numLedPairs) {
          turnOnLed2(dialPos - 1);
        }

        
      }

      if (resetButton->isPressed() && resetButton->isStatusChanged()) {
        Serial.println(F("Reset button pressed."));
        flashAllLed2();
      }

    }

  public:

    ShipWheelGame(Dial* dial, PushButton* lockB, PushButton* resetB) {
      gameDial = dial;
      lockButton = lockB;
      resetButton = resetB;

    }

    Dial* getDial() {
      return gameDial;
    }

    void addLEDPair(LEDPair* p) {
      if (numLedPairs < MAX_LED_PAIRS) {
        ledPairs[numLedPairs] = p;
        numLedPairs++;
      } else {
        Serial.println(F("SW Err 1"));
      }
    }

    void updateStatus() {
      updateGameDialStatus();
      updateButtonStatus();
    }

    LEDPair** getLEDPairs() {
      return ledPairs;
    }

    uint8_t getNumLEDPairs() {
      return numLedPairs;
    }

    //    LEDPair* getLEDPair(uint8_t index) {
    //      return ledPairs[index];
    //    }

    void flashAll() {
      for (uint8_t i = 0; i < numLedPairs; i++) {
        ledPairs[i]->flash();
      }
    }

    void turnOffAllLed1() {
      for (uint8_t i = 0; i < numLedPairs; i++) {
        ledPairs[i]->turnOffLed1();
      }
    }

    void turnOnAllLed2() {
      for (uint8_t i = 0; i < numLedPairs; i++) {
        ledPairs[i]->turnOnLed2();
      }
    }

    void turnOffAllLed2() {
      for (uint8_t i = 0; i < numLedPairs; i++) {
        ledPairs[i]->turnOffLed2();
      }
    }

    void flashAllLed2() {
      turnOnAllLed2();
      delay(500);
      turnOffAllLed2();
    }

    void turnOffAll() {
      for (uint8_t i = 0; i < numLedPairs; i++) {
        ledPairs[i]->turnOff();
      }
    }

    void turnOnLed1(uint8_t index) {
      if (index >= 0 && index < numLedPairs) {
        ledPairs[index]->turnOnLed1();
      }
    }

    void turnOnLed2(uint8_t index) {
      if (index >= 0 && index < numLedPairs) {
        ledPairs[index]->turnOnLed2();
      }
    }

};


//Potentiometer* potent;
ShipWheelGame* swGameInstance;


void setup() {

  Serial.begin(115200); // start the serial monitor link
  while (!Serial);

  Potentiometer* potent = initiaizePotentiometer();

  swGameInstance = initializeShipWheelGame(potent);

  // Flash all the LEDs at startup
  swGameInstance->flashAll();

}

int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}


Potentiometer* initiaizePotentiometer() {

  Potentiometer* newPot = new Potentiometer(POT_PIN_A, POT_PIN_B);

  //pinMode(POT_PIN_A, INPUT_PULLUP); // set pinA as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)
  //pinMode(POT_PIN_B, INPUT_PULLUP); // set pinB as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)

  //digitalWrite(POT_PIN_A, HIGH);  // turn on pull-up resistor
  //digitalWrite(POT_PIN_B, HIGH);  // turn on pull-up resistor

  attachInterrupt(0, potentPinAInteruptHandler, RISING); // set an interrupt on PinA, looking for a rising edge signal and executing the "PinA" Interrupt Service Routine.
  attachInterrupt(1, potentPinBInteruptHandler, RISING); // set an interrupt on PinB, looking for a rising edge signal and executing the "PinB" Interrupt Service Routine.

  return newPot;
}

void potentPinAInteruptHandler() {
  cli(); //stop interrupts happening before we read pin values
  swGameInstance->getDial()->getPotentiometer()->processInteruptPinA();
  sei(); //restart interrupts
}

void potentPinBInteruptHandler() {
  cli(); //stop interrupts happening before we read pin values
  swGameInstance->getDial()->getPotentiometer()->processInteruptPinB();
  sei(); //restart interrupts
}


ShipWheelGame* initializeShipWheelGame(Potentiometer* p) {

  int numberLEDPairs = 6;

  ShipWheelGame* game = new ShipWheelGame( new Dial(p, numberLEDPairs, NUM_POT_DELTA),
      new PushButton(LOCK_BUTTON),
      new PushButton(RESET_BUTTON) );

  // LED Pair 1
  game->addLEDPair(new LEDPair(new LightEmittingDiode(22), new LightEmittingDiode(23)));

  // LED Pair 2
  game->addLEDPair(new LEDPair(new LightEmittingDiode(24), new LightEmittingDiode(25)));

  // LED Pair 3
  game->addLEDPair(new LEDPair(new LightEmittingDiode(26), new LightEmittingDiode(27)));

  // LED Pair 4
  game->addLEDPair(new LEDPair(new LightEmittingDiode(28), new LightEmittingDiode(29)));

  // LED Pair 5
  game->addLEDPair(new LEDPair(new LightEmittingDiode(30), new LightEmittingDiode(31)));

  // LED Pair 6
  game->addLEDPair(new LEDPair(new LightEmittingDiode(32), new LightEmittingDiode(33)));



  // Check setup
  if (game->getNumLEDPairs() != numberLEDPairs) {
    Serial.println(F("Err: LED pair count is incorrect!"));
  }

  return game;
}

/**
   ---------------------------------------------------------------------------
   MAIN LOOP
   ---------------------------------------------------------------------------
*/
void loop() {

  swGameInstance->updateStatus();

  if (oldEncPos != swGameInstance->getDial()->getPotentiometer()->getCurrentPosition()) {
    Serial.print(F("Pr, Pc "));
    Serial.print(swGameInstance->getDial()->getPotentiometer()->getPreviousPosition());
    Serial.print(F(" "));
    Serial.print(swGameInstance->getDial()->getPotentiometer()->getCurrentPosition());
    //    Serial.print(swGameInstance->getDial()->getPotentiometer()->getCurrentPosition());
    Serial.print(F(" "));
    Serial.println(swGameInstance->getDial()->getPotentiometer()->isIncreasing() ? F("increasing.") : F ("decreasing."));
    oldEncPos = swGameInstance->getDial()->getPotentiometer()->getCurrentPosition();
  }

  if (!pFirst) {
    Serial.print(F("Initial pot pos "));
    Serial.print(swGameInstance->getDial()->getPotentiometer()->getCurrentPosition());
    Serial.print(F(" "));
    Serial.println(swGameInstance->getDial()->getPotentiometer()->isIncreasing() ? F("increasing.") : F ("decreasing."));
    pFirst = true;
  }

  if (millis() - timeLastDebug > 5000) {
    Serial.println(F("***********************"));

    //      Serial.print(F("*** Dial position local "));
    //      Serial.println(dialPos);
    //      Serial.print(F("*** Game Dial position "));
    //      Serial.println(gameDial->getDialPosition());
    //      Serial.print(F("*** Game Dial pot position "));
    //      Serial.println(gameDial->getDialPotPosition());
    //      Serial.print(F("Free ram: "));
    //      Serial.println(freeRam());

    timeLastDebug = millis();
  }


}
