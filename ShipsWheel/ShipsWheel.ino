#include <RotaryEncoder.h>
#include <GameCommUtils.h>

/*
 * Ships Wheel Puzzle
 */
#define LED1_1 10
#define LED1_2 11
#define LED2_1 12
#define LED2_2 13

 RotaryEncoder* wheel;
 int lastValue=0;

 void setup() {
  wheel = new RotaryEncoder(8,9,0);    // clk=pin 6, dt=pin 7, sw not used
  pinMode(LED1_1,OUTPUT);
  pinMode(LED1_2,OUTPUT);
  pinMode(LED2_1,OUTPUT);
  pinMode(LED2_2, OUTPUT);

  Serial.begin(9600);
  Serial.println(F("READY"));
}

void loop() {
  // put your main code here, to run repeatedly:
  int currentValue = wheel->getClickValue();
  if(lastValue != currentValue) {
    Serial.print(F("Current value="));
    Serial.println(currentValue);
    lastValue=currentValue;
  }

}
