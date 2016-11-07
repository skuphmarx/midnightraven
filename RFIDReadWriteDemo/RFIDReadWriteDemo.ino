    /* Front door Knocker
    ------Wiring------
    Pin 0: Record A New Knock button.
    Pin 1: (uses the built in LED)
    Pin 2 (Analog 1): A piezo element for beeping and sensing knocks.
    Pin 3: Connects to a transistor that opens a solenoid lock when HIGH.

  * Read a card using a mfrc522 reader on your SPI interface  
  * Pin layout should be as follows (on Arduino Uno):  
  * 3V -> VCC  NOTE 3V NOT 5V
  * GND -> GND
  * Pin 10 -> NSS
  * Pin 11 -> MOSI / ICSP-4  
  * Pin 12 -> MISO / ICSP-1  
  * Pin 13 -> SCK / ISCP-3    
  * Pin 5 -> RST
  */

    #include <SPI.h>
    #include <RFID.h>

  
    
    /* RFID Related setup */
     #define SS_PIN 10  
     #define RST_PIN 5  
     RFID rfid(SS_PIN,RST_PIN); 
     int serNum[5];  
     int itemOne[5] ={164,66,162,167,227};  /* Define RFID tag numbers we are looking for */
     int itemTwo[5] = {18,169,128,100,95};
     
   
    /*
     * Setup this node
     */
    void setup() {
     
   
      Serial.begin(9600);
      
   
/* RFID setup */
      SPI.begin();
      rfid.init();
      
      Serial.println("after rfid setup");
      
    }

   
    
  

     /*********************************************************
      * Basic node loop
      */
    void loop() {

     checkForRFID();
	  
    }
    

    /*********************************************************
     * Check for any RFID reads
     */
     void checkForRFID() {
      // TODO - If we are shown the ID then play the proper track. Puts us in game state 3
       if(rfid.isCard()){  
         Serial.println("IS CARD");
         if(rfid.readCardSerial()) {  
           Serial.println("Read Card"); 
           if(checkWhichCard(rfid,itemOne)) {

           } // End Item 1 is present
         } // End if readCardSerial
       } // End if isCard
       
     } // End checkforRFID
     
     /*******************************************************
     * See if the indicated card is present on the reader
     */
     bool checkWhichCard(RFID rfid, int item[]) {
          bool ret = false;
   
     Serial.print("RFID Tag number: ");
     Serial.print(rfid.serNum[0]);
     Serial.print(" ");
     Serial.print(rfid.serNum[1]);
     Serial.print(" ");
     Serial.print(rfid.serNum[2]);
     Serial.print(" ");
     Serial.print(rfid.serNum[3]);
     Serial.print(" ");
     Serial.println(rfid.serNum[4]);
   
//         if(rfid.serNum[0] == item[0] &&
//            rfid.serNum[1] == item[1] &&
//            rfid.serNum[2] == item[2] &&
//            rfid.serNum[3] == item[3] &&
//            rfid.serNum[4] == item[4]) {
//              ret = true; 
//         }
   
       return ret;
    } // End checkWhichCard 


