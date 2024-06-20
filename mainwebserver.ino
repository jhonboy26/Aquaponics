#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

#define RST_PIN 8    // Reset pin for RFID
#define SS_PIN 53    // Slave select pin for RFID

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance
Servo myServo;                    // Create Servo object

void setup() {
  Serial.begin(9600);       // Initialize serial communications
  SPI.begin();              // Init SPI bus
  mfrc522.PCD_Init();       // Init MFRC522
  myServo.attach(9);        // Attach the servo to PWM pin 6
  myServo.write(0);         // Initial position of the servo
  Serial.println("Place your RFID tag near the reader...");
}

void loop() {
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Show UID on serial monitor
  Serial.print("UID tag: ");
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();

  // Move servo to 90 degrees
  myServo.write(90);
  delay(5000); // Wait for 5 seconds
  
  // Move servo back to 0 degrees
  myServo.write(0);
}
