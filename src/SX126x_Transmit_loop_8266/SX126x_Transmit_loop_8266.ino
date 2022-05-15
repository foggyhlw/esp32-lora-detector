/*
   RadioLib SX126x Transmit with Interrupts Example

   This example transmits LoRa packets with one second delays
   between them. Each packet contains up to 256 bytes
   of data, in the form of:
    - Arduino String
    - null-terminated char array (C-string)
    - arbitrary binary data (byte array)

   Other modules from SX126x family can also be used.

   For default module settings, see the wiki page
   https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx126x---lora-modem

   For full API reference, see the GitHub Pages
   https://jgromes.github.io/RadioLib/
*/
#define UPDATE_PERIOD 10000
unsigned long time_now2 = 0; //for getting fans number none block loop 


// include the library
#include <RadioLib.h>
#include <SPI.h>
#include "DHTesp.h"
#include <Bounce2.h>
#include <Battery.h>
Battery battery = Battery(3000, 4200, A0);
Bounce b = Bounce();

DHTesp dht;

// SX1268 has the following connections:
// NSS pin:   D4
// DIO1 pin:  D8
// NRST pin:  D3
// BUSY pin:  D0

SX1268 radio = new Module(D4, D8, D3, D0);

// or using RadioShield
// https://github.com/jgromes/RadioShield
//SX1262 radio = RadioShield.ModuleA;

// or using CubeCell
//SX1262 radio = new Module(RADIOLIB_ONBOARD_MODULE);

// save transmission state between loops
int transmissionState = RADIOLIB_ERR_NONE;
volatile boolean interrupt_flag = false;
unsigned char byteArr[8];

void construct_packet(byte *byteArr, bool door_trigger = false){
  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();
    
    // if(temperature != NAN && humidity != NAN){
    //   String humidity_string = String(humidity,2);
    //   String temperature_string = String(temperature,2);
    //   String temp_humi = temperature_string + ' ' + humidity_string;
    //   transmissionState = radio.startTransmit(temp_humi);
    // }
  // you can also transmit byte array up to 256 bytes long
  
  byteArr[0] = 0x55;  //prefix
  byteArr[1] = door_trigger;   //door open state
  if(temperature != NAN && humidity != NAN){
    byteArr[2] = ((int)temperature)%256;  //temp_h
    byteArr[3] = ((int)(temperature*10))%10;  //temp_l
    byteArr[4] = ((int)humidity)%256; //humidity
  }
  else{
    byteArr[2] = 0;
    byteArr[3] = 0;
    byteArr[4] = 0;
  }
  byteArr[5] = digitalRead(D1);  //door state
  byteArr[6] = battery.level();  //battery level
  byteArr[7] = 0xaa;  //suffix
}




IRAM_ATTR void invation_report(){
  interrupt_flag = true;
}

void setup() {
  b.attach(D1,INPUT_PULLUP); // Attach the debouncer to a pin with INPUT_PULLUP mode
  b.interval(100); // Use a debounce interval of 25 milliseconds
  battery.begin(3300, 1.47, &sigmoidal);
  Serial.begin(9600);
  dht.setup(D2, DHTesp::DHT11); 
  // initialize SX1262 with default settings
  Serial.print(F("[SX1262] Initializing ... "));
  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    // while (true);
  }
  // pinMode(D1, INPUT_PULLUP);
  // attachInterrupt(digitalPinToInterrupt(D1), invation_report, RISING);
}


void loop() {
  b.update(); // Update the Bounce instance
  if ( b.rose() ) {  // Call code if button transitions from HIGH to LOW
    // interrupt_flag = false;
    construct_packet(byteArr, true);
    int transmissionState =  radio.transmit(byteArr, 8);
    delay(100);
    if (transmissionState == RADIOLIB_ERR_NONE) {
      // the packet was successfully transmitted
      Serial.println(F("success!"));

    } else if (transmissionState == RADIOLIB_ERR_PACKET_TOO_LONG) {
      // the supplied packet was longer than 256 bytes
      Serial.println(F("too long!"));

    } else if (transmissionState == RADIOLIB_ERR_TX_TIMEOUT) {
      // timeout occured while transmitting packet
      Serial.println(F("timeout!"));   

    } else {
      // some other error occurred
      Serial.print(F("failed, code "));
      Serial.println(transmissionState);

    }
    Serial.println("interrupt!");
  }
  if(millis() > time_now2 + UPDATE_PERIOD){
    time_now2 = millis();
    construct_packet(byteArr,false);
    int transmissionState =  radio.transmit(byteArr, 8);
    delay(300);
    if (transmissionState == RADIOLIB_ERR_NONE) {
      // the packet was successfully transmitted
      Serial.println(F("success!"));

    } else if (transmissionState == RADIOLIB_ERR_PACKET_TOO_LONG) {
      // the supplied packet was longer than 256 bytes
      Serial.println(F("too long!"));

    } else if (transmissionState == RADIOLIB_ERR_TX_TIMEOUT) {
      // timeout occured while transmitting packet
      Serial.println(F("timeout!"));

    } else {
      // some other error occurred
      Serial.print(F("failed, code "));
      Serial.println(transmissionState);
    }
    Serial.print("Battery voltage is ");
    Serial.print(battery.voltage());
    Serial.print(" (");
    Serial.print(battery.level());
    Serial.println("%)");
  }
  // if(interrupt_flag){
  //   interrupt_flag = false;
  //   construct_packet(byteArr, true);
  //   int transmissionState =  radio.transmit(byteArr, 8);
  //   delay(100);
  //   if (transmissionState == RADIOLIB_ERR_NONE) {
  //     // the packet was successfully transmitted
  //     Serial.println(F("success!"));

  //   } else if (transmissionState == RADIOLIB_ERR_PACKET_TOO_LONG) {
  //     // the supplied packet was longer than 256 bytes
  //     Serial.println(F("too long!"));

  //   } else if (transmissionState == RADIOLIB_ERR_TX_TIMEOUT) {
  //     // timeout occured while transmitting packet
  //     Serial.println(F("timeout!"));

  //   } else {
  //     // some other error occurred
  //     Serial.print(F("failed, code "));
  //     Serial.println(transmissionState);

  //   }
  //   Serial.println("interrupt!");
    
  // }
}