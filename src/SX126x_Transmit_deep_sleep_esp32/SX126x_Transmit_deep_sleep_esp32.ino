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

// include the library
#include <RadioLib.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "DHTesp.h"
#include <Battery.h>

//pins for sx1268 lora module
#define  NSS_pin 17
#define  DIO1_pin 5
#define  NRST_pin 16
#define  BUSY_pin 4
// pins for I2C ssd1306 0.96 oled
#define OLED_CLK 22
#define OLED_SDA 21

#define DHT_pin 35

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10        /* Time ESP32 will go to sleep (in seconds) */

#define defaultFont u8g2_font_timR10_tr    

DHTesp dht;

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR float humidity;
RTC_DATA_ATTR float temperature;
RTC_DATA_ATTR bool last_trigger_state = true;

Battery battery = Battery(3000, 4200, 2);

void construct_packet(byte *byteArr, bool door_state = false){
  humidity = dht.getHumidity();
  temperature = dht.getTemperature();
    
    // if(temperature != NAN && humidity != NAN){
    //   String humidity_string = String(humidity,2);
    //   String temperature_string = String(temperature,2);
    //   String temp_humi = temperature_string + ' ' + humidity_string;
    //   transmissionState = radio.startTransmit(temp_humi);
    // }
  // you can also transmit byte array up to 256 bytes long
  
  byteArr[0] = 0x55;  //prefix
  byteArr[1] = 0x00;   //door trigger signal
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
  byteArr[5] = door_state;  //door state
  byteArr[6] = battery.level();  //battery level
  byteArr[7] = 0xaa;  //suffix
}


void setup() {
  SPI.begin(18,19,23,16);
  esp_sleep_wakeup_cause_t wakeup_reason;
  battery.begin(3300, 1.47, &sigmoidal);

  wakeup_reason = esp_sleep_get_wakeup_cause();
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ OLED_CLK, /* data=*/ OLED_SDA);   // ESP32 Thing, HW I2C with pin remapping
  int transmissionState = RADIOLIB_ERR_NONE;
  unsigned char byteArr[8];
  Wire.begin();
  u8g2.begin();
  ++bootCount;
  
  u8g2.clearBuffer();
  u8g2.setFont(defaultFont);
  u8g2.drawStr(5,16,"Temp: ");
  u8g2.drawStr(5,32,"Humi: ");
  u8g2.drawStr(95,16,"C");
  u8g2.drawStr(95,32,"%");
  u8g2.setCursor(50,16);
  u8g2.print(temperature);
  u8g2.setCursor(50,32);
  u8g2.print(humidity);
  u8g2.setCursor(50,48);
  u8g2.print(bootCount);
  u8g2.setCursor(50,64);
  u8g2.print(last_trigger_state);
  u8g2.setFont(defaultFont);    
  u8g2.sendBuffer(); 
 
  SX1268 radio = new Module(NSS_pin, DIO1_pin, NRST_pin, BUSY_pin);
  Serial.begin(9600);
   while(!Serial) { }

   switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
  // dht.setup(D2, DHTesp::DHT11); 
  // initialize SX1262 with default settings
  // Serial.print(F("[SX1262] Initializing ... "));
  int state = radio.begin(434.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 22, 8, 0, false);
  // if (state == RADIOLIB_ERR_NONE) {
  //   Serial.println(F("success!"));
  // } else {
  //   Serial.print(F("failed, code "));
  //   Serial.println(state);
  // }
 
if(last_trigger_state){
  construct_packet(byteArr, false);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_36, 0);
  if(wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 ){
    last_trigger_state = !last_trigger_state;
    Serial.println("++++++++++++++++++++++++");
  }
}
else{
  construct_packet(byteArr, true);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_36, 1);
    if(wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 ){
      last_trigger_state = !last_trigger_state;
      Serial.println("------------------------");
    }
}
  
  transmissionState = radio.startTransmit(byteArr, 8);
  delay(500);
  if (transmissionState == RADIOLIB_ERR_NONE) {
      // packet was successfully sent
      // Serial.println(F("transmission finished!"))
    }
 
  radio.sleep(false); 
  
  // esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
	Serial.print("Battery voltage is ");
	Serial.print(battery.voltage());
	Serial.print(" (");
	Serial.print(battery.level());
	Serial.println("%)");

  Serial.flush(); 
  esp_deep_sleep_start();
}


void loop() {
}
