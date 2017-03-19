#ifndef __ROTARYENCODER__

#define __ROTARYENCODER__

#include "Arduino.h"

class RotaryEncoder {

public:
	
	RotaryEncoder ( uint8_t theClkPin, uint8_t theDtPin, uint8_t theSwitchPin);
	bool hasValueChanged ( );
	int getValue();
	int getClickValue();
	void reset();
	void updateClicks(bool isCurrentDirectionClockwise);
	bool isLastTurnClockwise();
	void setDebounce(int val);
	void setClicksTillRollover(int val);

	
private:
	uint8_t	clkPin,				// pin definitions for the encoder
			dtPin,
			swPin;	

	int encoderPosCount,
	    clkPinLast,
		clicksInDirection;   // This is how many clicks in which direction. e.g. 1 means 1 clockwise, 4 means 4 counterclockwise
	int debounce = 25;       // If a change in direction occurs within this time then it's an anomoly so consider it a debounce
	int clicksTillRollover = 20;  // Once we hit 20 we start at 0 again
		
	bool lastTurnClockwise;	
	
	unsigned long lastTurnTime;
};

#endif