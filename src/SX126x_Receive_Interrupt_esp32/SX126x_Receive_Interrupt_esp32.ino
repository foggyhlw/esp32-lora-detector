

// include the library
#include <Arduino.h>
#include <RadioLib.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "ntp_time.h"

#include "parameters.h"
#include <EEPROM.h>
#include "Button2.h"
#include <Wire.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

unsigned long time_now1 = 0; //for getting fans number none block loop 
unsigned long time_now2 = 0;
unsigned char byteArr[PACKET_LENGTH];  //receive lora packet
char invation_records[RECORD_NUM][RECORD_LENGTH];   //used to store invation record 
int record_index=0;             //used to store invation record

WiFiManager wm;

#include "EspMQTTClient.h"
// EspMQTTClient client(
//   "ssid",
//   "wifi_password",
//   "192.168.0.100",  // MQTT Broker server ip
//   "foggy",   // Can be omitted if not needed
//   "1989228",   // Can be omitted if not needed
//   "lora_sx1268",     // Client name that uniquely identify your device
//   1883              // The MQTT port, default to 1883. this line can be omitted
// );

EspMQTTClient client(
  // "ssid",
  // "wifi_password",
  "mqqt_ip",  // MQTT Broker server ip
  1883,              // The MQTT port, default to 1883. this line can be omitted
  "mqtt_id",   // Can be omitted if not needed
  "mqtt_password",   // Can be omitted if not needed
  "lora_sx1268"     // Client name that uniquely identify your device
);
// #ifdef U8X8_HAVE_HW_SPI
// #include <SPI.h>
// #endif
// #ifdef U8X8_HAVE_HW_I2C
// #include <Wire.h>
// #endif

Button2 button = Button2(BUTTON_PIN);

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ OLED_CLK, /* data=*/ OLED_SDA);   // ESP32 Thing, HW I2C with pin remapping
// U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ OLED_CLK, /* data=*/ OLED_SDA, /* reset=*/ U8X8_PIN_NONE);

// SX1262 has the following connections:

SX1268 radio = new Module(NSS_pin, DIO1_pin, NRST_pin, BUSY_pin);

// function to get bilibili fans, directly write global var fans_num used for oled display
int fans_num = 0;
unsigned char battery_level = 0;
void get_bilibili_fans(int &fans_num){
  if (WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    http.begin(FANS_URL);
    int httpCode = http.GET();
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      StaticJsonDocument<300> json_doc; // for json deserializeJson
      DeserializationError error = deserializeJson(json_doc, payload);
      // Test if parsing succeeds.
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
      }
    fans_num = int(json_doc["data"]["follower"]);
    }
  }
}

void setup() {
  Serial.begin(9600);
  SPI.begin(18,19,23,16);
  SPI.setFrequency(4000000);
  Wire.begin();
  u8g2.begin();
  // u8g2.enableUTF8Print();		// enable UTF8 support for the Arduino print() function
  u8g2.clearBuffer();         // clear the internal memory
  // u8g2.setFont(u8g2_font_helvR10_tr); // choose a suitable font
  u8g2.setFont(defaultFont);
  // draw a frame, only the content within the frame will be updated
  // the frame is never drawn again, but will stay on the display
  // u8g2.drawBox(pixel_area_x_pos-1, pixel_area_y_pos-1, pixel_area_width+2, pixel_area_height+2);
  // u8g2.sendBuffer();          // transfer internal memory to the display
  // u8g2.setFont(u8g2_font_courB18_tr); // set the target font for the text width calculation
  // width = u8g2.getUTF8Width(text);    // calculate the pixel width of the text
  // offset = width+pixel_area_width;
  
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP 
  // wm.resetSettings();
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(60);
  if(wm.autoConnect("foggy_2G","1989Fox228")){
        Serial.println("connected...yeey :)");
    }
    else {
        Serial.println("Configportal running");
    }

  wm.startConfigPortal();
  setupDateTime();
    // wm.startWebPortal();

  button.setClickHandler(click_handler);
  button.setLongClickHandler(longClick_handler);

  // initialize SX1262 with default settings
  Serial.print(F("[SX1262] Initializing ... "));
  int state = radio.begin(434.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 22, 8, 0, false);

  // int state = radio.begin();          //remember to set tcxoVoltage = 0 in radiolib  sx1268.h
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("sx1268 init success!"));
  } else {
    Serial.print(F("sx1268 init failed, code "));
    Serial.println(state);
    // while (true);
  }

  // set the function that will be called
  // when new packet is received
  radio.setDio1Action(setFlag);

  // start listening for LoRa packets
  Serial.print(F("[SX1262] Starting to listen ... "));
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    // while (true);
  }
//   // if needed, 'listen' mode can be disabled by calling
//   // any of the following methods:
//   //
//   // radio.standby()
//   // radio.sleep()
//   // radio.transmit();
//   // radio.receive();
//   // radio.readData();
//   // radio.scanChannel();
}
/* ---------------------------setup end---------------------------*/
// flag to indicate that a packet was received
volatile bool receivedFlag = false;

// disable interrupt when it's not needed
volatile bool enableInterrupt = true;

// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  // check if the interrupt is enabled
  if(!enableInterrupt) {
    return;
  }
  // we got a packet, set the flag
  receivedFlag = true;
}
bool door_trigger = false;
bool door_trigger_flag = false;
bool door_state = false;      //store state
unsigned char temperature_h = 0;    //store temp_h
unsigned char temperature_l = 0;    //store temp_l
unsigned char humidity_h = 0;      //store humi
String temperature;
String humidity;


void loop() {
  wm.process();
  button.loop();
  client.loop(); // mqtt client loop
  // check if the flag is set
  if(receivedFlag) {
    // disable the interrupt service routine while
    // processing the data
    enableInterrupt = false;
    // reset flag
    receivedFlag = false;
    // you can read received data as an Arduino String
    // String str;
    // int state = radio.readData(str);
    // char char_str[str.length()+1];
    // str.toCharArray(char_str,str.length());
    // char * strtokIndx;

    // you can also read received data as byte array

    int state = radio.readData(byteArr, PACKET_LENGTH);

    if (state == RADIOLIB_ERR_NONE) {
      /*packet starts with 0x55 and ends with 0xaa
      packet 0x55 door_trigger temp_h temp_l humi door_status battery_level 0xaa
      cmd=1 means invader  cmd=0 means safe*/
      if (byteArr[0] == 0x55 && byteArr[PACKET_LENGTH-1] == 0xaa){
        door_trigger = byteArr[1];  // if door_state change (a door open happend),record it
        if(!door_trigger_flag){  //only renew door_trigger_flag after long_click clear it
          door_trigger_flag = door_trigger;
        }
        if(door_trigger){
          //store door open time into a array to display
          strcpy(invation_records[record_index],DateTime.format(DateFormatter::SIMPLE).c_str());
          // invation_records[record_index] = DateTime.format(DateFormatter::SIMPLE).c_str();
          record_index++;
          if (record_index>=5){
            record_index = 0;
          }
        }
        temperature_h = byteArr[2];
        temperature_l = byteArr[3];
        humidity_h = byteArr[4];

        temperature = String(temperature_h)+"."+String(temperature_l);
        humidity = String(humidity_h);
        door_state = byteArr[5];
        battery_level = byteArr[6];
        // client.publish("/foggy/esp32-lora/store-room/door/state",String(door_state));
        StaticJsonDocument<60> store_sensor;
        store_sensor["temperature"] = temperature.toFloat(); //toFloat() has precision problem, fix it
        store_sensor["humidity"] = humidity.toFloat();
        store_sensor["door"] = String(door_state);
        String serializedJson;   //to convert json to string to send via mqtt
        serializeJson(store_sensor,serializedJson);
        client.publish("/foggy/esp32-lora/store-room/sensor",serializedJson);
      }
      else{
        temperature = "NAN";
        humidity = "NAN";
      }
      Serial.print(door_state);
    //   strtokIndx = strtok(char_str, " ");
    //   temperature = String(strtokIndx);
    //   strtokIndx = strtok(NULL, " ");
    //   humidity = String(strtokIndx);
      // // packet was successfully received
      // Serial.println(F("[SX1262] Received packet!"));
      // // print data of the packet
      // Serial.print(F("[SX1262] Data:\t\t"));
      // Serial.println(str);
      // // print RSSI (Received Signal Strength Indicator)
      // Serial.print(F("[SX1262] RSSI:\t\t"));
      // Serial.print(radio.getRSSI());
      // Serial.println(F(" dBm"));
      // // print SNR (Signal-to-Noise Ratio)
      // Serial.print(F("[SX1262] SNR:\t\t"));
      // Serial.print(radio.getSNR());
      // Serial.println(F(" dB"));
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      // packet was received, but is malformed
      Serial.println(F("CRC error!"));

    } else {
      // some other error occurred
      Serial.print(F("failed, code "));
      Serial.println(state);

    }

    // put module back to listen mode
    radio.startReceive();

    // we're ready to receive more packets,
    // enable interrupt service routine
    enableInterrupt = true;

    // screen_mode = RECORD_MODE; //immediately change to record mode when a detection signal is received

  }

  if(millis() > time_now2 + OLED_UPDATE_PERIOD){
    time_now2 = millis();
    update_screen();
  }
  if(millis() > time_now1 + FANS_UPDATE_PERIOD){
    time_now1 = millis();
    // showTime();
    if (WiFi.status() == WL_CONNECTED) {
      get_bilibili_fans(fans_num);
    }
  }
  

}


void onConnectionEstablished()
{
  // Subscribe to "mytopic/test" and display received message to Serial
  client.subscribe("/esp32-lora/msg", [](const String & payload) {
    Serial.println(payload);
  });
  // Serial.println("Connected!");
  // // Subscribe to "mytopic/wildcardtest/#" and display received message to Serial
  // client.subscribe("mytopic/wildcardtest/#", [](const String & topic, const String & payload) {
  //   Serial.println("(From wildcard) topic: " + topic + ", payload: " + payload);
  // });

  // // Publish a message to "mytopic/test"
  // client.publish("mytopic/test", "This is a message"); // You can activate the retain flag by setting the third parameter to true

  // // Execute delayed instructions
  // client.executeDelayed(5 * 1000, []() {
  //   client.publish("mytopic/wildcardtest/test123", "This is a message sent 5 seconds later");
  // });
  // Serial.println("mqtt connected!");
}

void update_screen() {
  u8g2.clearBuffer();
  if (screen_mode == BILIBILI_MODE){
    u8g2.setCursor(10,17);
    u8g2.setFont(mediumFont);
    u8g2.print(DateTime.format(DateFormatter::DATE_ONLY).c_str());
    //https://github.com/olikraus/u8g2/wiki/fnticons

    if(door_trigger_flag == true){
      u8g2.setFont(u8g2_font_open_iconic_gui_2x_t);
      u8g2.drawGlyph(112,16,64);
    }
    else{
      u8g2.setFont(u8g2_font_open_iconic_www_2x_t);
      //if wifi is not connected print icon
      if(WiFi.status() == WL_CONNECTED){
        u8g2.drawGlyph(112,16,81);
      }
      else{
        u8g2.drawGlyph(112,16,74);    
      }    
    }
    // u8g2.drawXBMP(15,0,BILIBILI_BMP_width,BILIBILI_BMP_height,BILIBILI_BMP_bits);
    u8g2.setCursor(10,45);
    u8g2.setFont(largeFont);
    //DateFormatter:  https://blog.mcxiaoke.com/ESPDateTime/struct_date_formatter.html
    u8g2.println(DateTime.format(DateFormatter::TIME_ONLY).c_str());
    u8g2.setFont(defaultFont);
    // u8g2.setFontDirection(0);
    u8g2.setCursor(15,61);
    u8g2.print("foggyhlw");
    u8g2.setCursor(84,62);
    u8g2.println(fans_num);
    // u8g2.println("foggyhlw");
    u8g2.setFont(defaultFont);
  }
  if (screen_mode == RECORD_MODE){
    int line_start_y = 22;
    u8g2.setCursor(15,10);
    u8g2.setFont(smallFont);
    u8g2.print("Invation record:");
    int print_index = record_index;
    for ( int i =0; i<5; i++){
      u8g2.setCursor(0,line_start_y);
      u8g2.print(i+1);
      u8g2.setCursor(15,line_start_y);
      //to put latest record to top
      print_index--;
      if(print_index < 0){
        print_index = RECORD_NUM-1;
      }
      u8g2.print(invation_records[print_index]);
      line_start_y += 10;
    }
    // if ( !invation_records.isEmpty() ){
    //   for ( int i = 0; i<invation_records.size(); i++){
    //     u8g2.setCursor(0,line_start_y);
    //     u8g2.print(i+1);
    //     u8g2.setCursor(15,line_start_y);
    //     // const char* record = invation_records[i].date_time_record;
    //     u8g2.print(invation_records[i]);
    //     // Serial.println(i);
    //     // Serial.println(invation_records[i]);
    //     line_start_y += 10;

    //   }
    // }
    u8g2.setFont(defaultFont);
  }
  if (screen_mode == TEMP_HUMI_MODE){
    u8g2.setFont(defaultFont);
    u8g2.drawStr(5,12,"Temp: ");
    u8g2.drawStr(5,24,"Humi: ");
    u8g2.drawStr(5,36,"RSSI: ");
    u8g2.drawStr(5,48,"SNR : ");
    u8g2.drawStr(5,60,"BAT : ");

    u8g2.drawStr(95,12,"C");
    u8g2.drawStr(95,24,"%");
    u8g2.drawStr(95,36,"dBm");
    u8g2.drawStr(95,48,"dB");
    u8g2.drawStr(95,60,"%");
    u8g2.drawStr(50,12,temperature.c_str());
    u8g2.drawStr(50,24,humidity.c_str());
    u8g2.drawStr(50,36,String(radio.getRSSI(),0).c_str());
    u8g2.drawStr(50,48,String(radio.getSNR()).c_str());
    u8g2.drawStr(50,60,String(battery_level).c_str());
    u8g2.setFont(defaultFont);    
 
  }
  u8g2.sendBuffer(); 
}

void click_handler(Button2& btn){
  Serial.print("clicked!");
  if(screen_mode < MODE_NUM){
    screen_mode++;
  }
  if(screen_mode >= MODE_NUM){
    screen_mode = 1;
  }
  update_screen();
}

void longClick_handler(Button2& btn){
  //clear invation record
  if (screen_mode == RECORD_MODE){
    door_trigger_flag = false;
    for (int i = 0; i<5; i++){
      strcpy(invation_records[i],"");
    }
    record_index = 0;
  }
}
