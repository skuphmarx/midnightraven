#include <PJON.h>

// <Strategy name> bus(selected device id)
PJON<OverSampling> bus(45);

void setup() {
  pinModeFast(13, OUTPUT);
  digitalWriteFast(13, LOW); // Initialize LED 13 to be off
  bus.set_error(error_handler);
  bus.strategy.set_pins(11, 12);
  bus.set_receiver(receiver_function);

  bus.begin();

  bus.send(44, "B", 1);

  Serial.begin(115200);
};

void receiver_function(uint8_t *payload, uint16_t length, const PacketInfo &packet_info) {
 if((char)payload[0] == 'B') {
    bus.reply("B", 1);
    digitalWrite(13, HIGH);
    delay(5);
    digitalWrite(13, LOW);
  }
}

void error_handler(uint8_t code, uint8_t data) {
  if(code == CONNECTION_LOST) {
    Serial.print("Connection with device ID ");
    Serial.print(data);
    Serial.println(" is lost.");
  }
  if(code == PACKETS_BUFFER_FULL) {
    Serial.print("Packet buffer is full, has now a length of ");
    Serial.println(data, DEC);
    Serial.println("Possible wrong bus configuration!");
    Serial.println("higher MAX_PACKETS in PJON.h if necessary.");
  }
  if(code == CONTENT_TOO_LONG) {
    Serial.print("Content is too long, length: ");
    Serial.println(data);
  }
};

void loop() {
  bus.receive(20000);
  bus.update();
};
