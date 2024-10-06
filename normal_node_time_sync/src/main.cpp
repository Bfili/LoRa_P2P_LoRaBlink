// include libraries
#include <SPI.h>
#include <LoRa.h>

// define pins used for SPI
#define HSPI_SCK 14
#define HSPI_MISO 12
#define HSPI_MOSI 13
#define SD_CS 15

// define pins used by the LoRa module
const int csPin = 15;                                                           // LoRa radio chip select
const int resetPin = 27;                                                        // LoRa radio reset
const int irqPin = 4;                                                           // must be a hardware interrupt pin
const int ledPin = 2;                                                           // LED pin for beacon reception confimation

SPIClass *hspi = NULL;

int nodeID = 2;                                                                 // example Node ID, adjust for each node
int messageIncrement = 0;                                                       // message no., for checking PDR

// initialize variables 
unsigned long epochTime = -1;                                                   // epoch time in milliseconds
unsigned long totalSlots = -1;                                                  // no. of slots per epoch
unsigned long beaconSlots = -1;                                                 // first N_B slots for beacons
unsigned long slotTime = -1;                                                    // slot time in milliseconds

bool beaconReceived = false;                                                    // flag for beacon reception
bool messageSent = false;                                                       // flag for message sending
String message = "";                                                            // empty message variable

// function: extract data from beacon
// parameters:
// String beacon - received beacon 
void extractBeaconInformation(String beacon) {
  beaconSlots = beacon.substring(7).toInt();
  totalSlots = beacon.substring(9, 11).toInt();
  slotTime = beacon.substring(12).toInt();
}

// function: send beacon to the next layer
// parameters:
// int newLayer - value of the layer to be sent inside beacon
// int newTotalSlots - no. of slots left to be sent inside beacon
void sendBeacon(int newLayer, int newTotalSlots) {
  String newBeacon = "Layer: " + String(newLayer) + 
  " " + String(newTotalSlots) + " " + String(slotTime);
  LoRa.beginPacket();                                                           // send beacon with new information
  LoRa.print(newBeacon);
  LoRa.endPacket();
  Serial.print("Beacon sent: ");
  Serial.println(newBeacon);

  LoRa.receive();                                                               // return to receive mode after sending
}

// function: send message
void sendMessage() {
  float randomData = 22.22;                                                     // dummy data to be send, for testing purposes
  messageIncrement++;
  if (message == "") {                                                          // if message is empty, create new message
    message = "ID: " + String(nodeID) + ", DATA: " + 
    String(randomData, 2) + ", " + String(messageIncrement); 
  } 
  else if (message != "") {                                                     // | if message is not empty
    message += "| ID: " + String(nodeID) + ", DATA: " +                         // | (message to be forwarded was received), append your message
    String(randomData, 2) + ", " + String(messageIncrement);
  }

  LoRa.beginPacket();                                                           // send message
  LoRa.print(message);
  LoRa.endPacket();

  Serial.print("Message sent: ");
  Serial.println(message);
  
  messageSent = true;                                                           // reset messageSent flag
  beaconReceived = false;                                                       // reset beaconReceived flag
  message = "";                                                                 // clear message variable
  LoRa.receive();                                                               // return to receive mode
}

// function: receive message
// parameters:
// int packetSize - size of received packet
void onReceive(int packetSize) {
  if (packetSize) {
    String receivedData = "";
    while (LoRa.available()) {                                                
      receivedData += (char)LoRa.read();                                        // read received message
    }

    Serial.print("Received: ");
    Serial.println(receivedData);
    if (!beaconReceived) {                                                      // if you don't have a beacon
      extractBeaconInformation(receivedData);                                   // extract data from beacon
      beaconReceived = true;                                                    // change flag, beacon was received

      if (beaconSlots > 1) {                                                    // if you're not in the last layer
        beaconSlots -= 1;                                                       // decrement layer no. for the new beacon
        totalSlots -= 1;                                                        // decrement no. of layers left in an epoch
        sendBeacon(beaconSlots, totalSlots);                                    // send new beacon
        messageSent = false;                                                    // change flag, listen for messages in next slots
        esp_sleep_enable_timer_wakeup(beaconSlots 
                                      * slotTime * 1000);                       // put to sleep for remaining N_B slots
        Serial.println("Entering Sleep Mode");
        esp_light_sleep_start();                                                // enter light sleep mode
        Serial.println("Exiting Sleep Mode");
      }
      else if (beaconSlots == 1) {                                              // if you're in the last layer
        sendMessage();                                                          // send message
        messageSent = false;                                                    // change flag, listen for messages in next slots
      }
    } 
    else if (receivedData.substring(0, 1) != "L" && !messageSent) {             // if received a message and message wasn't send earlier
      message = receivedData;                                                   
      sendMessage();                                                            // send message
      esp_sleep_enable_timer_wakeup((totalSlots - beaconSlots)                  // put to sleet for remaining N_D slots
      * slotTime * 1000); 
      Serial.println("Entering Sleep Mode");
      esp_light_sleep_start();                                                  // enter light sleep mode
      Serial.println("Exiting Sleep Mode");
    }
    else {                                                                      // if you wait for message, but receive beacon
      Serial.println("Received another beacon, discarding...");
    }
  }
}


void setup() {
  Serial.begin(9600);
  while (!Serial);

  hspi = new SPIClass(HSPI);
  hspi->begin(HSPI_SCK, HSPI_MISO, HSPI_MOSI, SD_CS);

  LoRa.setPins(csPin, resetPin, irqPin);                                        // set pins used by LoRa
  LoRa.setSPI(*hspi);                                                           // set SPI interface to be used for communication
  
  

  if (!LoRa.begin(868E6)) {                                                     // 868E6 - LoRa frequency 868MHz
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  LoRa.setSpreadingFactor(12);                                                  // set SF, same as sink
  LoRa.setSignalBandwidth(500E3);                                               // set BW, same as sink
  LoRa.setCodingRate4(8);                                                       // set CR, same as sink

  Serial.println("LoRa Receiver Ready");
  LoRa.receive();                                                               // start in receive mode

  pinMode(ledPin, OUTPUT);                                                      // set LED pin for beacon reception confimation
}

void loop() {
  if (beaconReceived) {                                                         // change LED state, according to beaconReceived flag
    digitalWrite(ledPin, HIGH);
  }
  else {
    digitalWrite(ledPin, LOW);
  }
  int packetSize = LoRa.parsePacket();
  onReceive(packetSize);                                                        
}

