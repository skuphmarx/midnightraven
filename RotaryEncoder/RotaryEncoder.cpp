
#include "RotaryEncoder.h"

static int MAX_COUNT=20;   // When it reaches this value it will roll over to 0

RotaryEncoder :: RotaryEncoder ( uint8_t theClkPin, uint8_t theDtPin, uint8_t theSwitchPin ) {

	this->clkPin = theClkPin;
	this->dtPin = theDtPin;
	this->swPin = theSwitchPin;
	
	pinMode(this->dtPin,INPUT);	// TODO: Do we need pullups? CHECK
	pinMode(this->clkPin,INPUT);	// TODO: Do we need pullups? CHECK
	pinMode(this->swPin,INPUT);	// TODO: Do we need pullups? CHECK
	
	this->encoderPosCount = 0;
	this->clicksInDirection = 0;

}

void RotaryEncoder :: reset() {
  this->encoderPosCount = 0 ;
  this->clicksInDirection = 0;
  this->lastTurnClockwise = true;
}

void RotaryEncoder :: setDebounce(int val) {
    debounce = val;
}

void RotaryEncoder :: setClicksTillRollover(int val) {
    clicksTillRollover = val;
}

bool RotaryEncoder :: hasValueChanged() {
 bool ret = false;
 unsigned long checkTime = millis();
  
    int aVal = digitalRead(this->clkPin);

    if ((aVal != this->clkPinLast)&&(aVal==LOW) ) { 
  
        int bVal = digitalRead(this->dtPin);
        if(  ( (bVal != aVal && !this->lastTurnClockwise) || (bVal == aVal && this->lastTurnClockwise) ) && ((checkTime - lastTurnTime) < debounce) ) {
          Serial.println("TOO FAST");
          bVal = !bVal;
        }

//          Serial.print("time=");
//          Serial.println((checkTime-lastTurnTime));
          if(bVal != aVal){
		     this->updateClicks(true);
             this->lastTurnClockwise=true;
			 if(++this->encoderPosCount > MAX_COUNT) {
			    this->encoderPosCount = 0 ;
			 }
          } 
          else {
		     this->updateClicks(false);
			 
             if(--this->encoderPosCount < -MAX_COUNT) {
                this->encoderPosCount = 0;            
             }
			 this->lastTurnClockwise=false;

          }
    
          this->lastTurnTime = checkTime;
		  
		  ret = true;
      
    }
    this->clkPinLast = aVal; // Don’t forget this 
	
	return ret;

}

int RotaryEncoder :: getValue() {

   return this->encoderPosCount;
}

int RotaryEncoder :: getClickValue() {
   return this->clicksInDirection;
}

void RotaryEncoder :: updateClicks(bool isCurrentDirectionClockwise) {
      if(this->lastTurnClockwise && isCurrentDirectionClockwise) {
			this->clicksInDirection++;
      } else if(this->lastTurnClockwise && !isCurrentDirectionClockwise) {
	        this->clicksInDirection = -1;  // we have switched directions
	  } else if(!this->lastTurnClockwise && !isCurrentDirectionClockwise) {
	        this->clicksInDirection--;
	  } else if(!this->lastTurnClockwise && isCurrentDirectionClockwise) {
	         this->clicksInDirection = 1; // switched directions 
	  }
}

bool RotaryEncoder :: isLastTurnClockwise() {
 return this->lastTurnClockwise;
}





