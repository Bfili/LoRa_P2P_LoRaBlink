// include libraries
#include <SPI.h>
#include <LoRa.h>

// define pins used for SPI
#define HSPI_SCK 14                                                             
#define HSPI_MISO 12
#define HSPI_MOSI 13
#define SD_CS 15

// define pins used by the LoRa module
const int csPin = 15;                                            // LoRa radio chip select
const int resetPin = 27;                                         // LoRa radio reset
const int irqPin = 4;                                            // must be a hardware interrupt pin

SPIClass * hspi = NULL;

unsigned long epochTime = 10000;                                 // epoch time in milliseconds
unsigned long totalSlots = 5;                                   // no. of slots per epoch
unsigned long beaconSlots = 2;                                   // first N_B slots for beacons
unsigned long slotTime = 2000;                                   // slot time in milliseconds
unsigned long epochDelay = 1000;                                 // 1 seconds delay between epochs

void setup() {
  Serial.begin(9600);
  while (!Serial);

  hspi = new SPIClass(HSPI);
  hspi->begin(HSPI_SCK, HSPI_MISO, HSPI_MOSI, SD_CS);

  LoRa.setPins(csPin, resetPin, irqPin);                         // set pins used by LoRa
  LoRa.setSPI(*hspi);                                            // set SPI interface to be used for communication

  if (!LoRa.begin(868E6)) {                                      // 868E6 - LoRa frequency 868MHz
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  LoRa.setSpreadingFactor(12);                                   // set SF, common values: 7, 8, 9, 10, 11, 12
  LoRa.setSignalBandwidth(500E3);                                // set BW, common values: 125MHz, 250MHz, 500MHz
  LoRa.setCodingRate4(8);                                        // set CR, equation 4/(4+CR), common values: 5, 6, 7, 8

  Serial.println("Sink Node Starting...");
}

void loop() {
  Serial.println("Sending Beacon...");                           // | send beacon with synchronization information:
  LoRa.beginPacket();                                            // | beaconSlots, totalSlots, slotTime
  LoRa.print("Layer: ");                                         // | beacon structure example:
  LoRa.print(beaconSlots);                                       // | Layer: 2 30 2000
  LoRa.print(" ");
  LoRa.print(totalSlots);
  LoRa.print(" ");
  LoRa.print(slotTime);
  LoRa.endPacket();

  unsigned long startTime = millis();                            // start internal timer
  while (millis() - startTime < epochTime) {                     // do instructions for epochTime duration
    int packetSize = LoRa.parsePacket();                        
    if (packetSize) {
      String receivedData = "";
      while (LoRa.available()) {
      receivedData += (char)LoRa.read();                         // read data in received packet 
      }

      if (receivedData.substring(0, 1) != "L") {                 // | if received message isn't a beacon
        Serial.print("Received packet: ");                       // | print message with information about
        Serial.print(receivedData);                              // | RSSI and SNR values 
        Serial.print(" RSSI: ");
        Serial.print(LoRa.packetRssi());
        Serial.print(" SNR: ");
        Serial.println(LoRa.packetSnr());
      }
    }
  }

  Serial.println("Epoch ended, waiting 1 seconds...");
  delay(epochDelay);
}
