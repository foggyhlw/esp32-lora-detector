#define RECORD_NUM 5       //record number displayed on invation page
#define RECORD_LENGTH 20   //datetime string lenght

#define PACKET_LENGTH 8   //bytes a packet contains, (prefix) 0x55 door temp_h temp_l humi res res (sufix) 0xaa 

#define FANS_UPDATE_PERIOD 60000   
#define OLED_UPDATE_PERIOD 1000

// default font for u8g2
//
#define defaultFont u8g2_font_7x13B_tr     
#define smallFont u8g2_font_t0_12_tf       
#define mediumFont u8g2_font_t0_17b_me       
#define largeFont u8g2_font_fur20_tn      
#define chineseFont u8g2_font_unifont_t_chinese2

// pins for I2C ssd1306 0.96 oled
#define OLED_CLK 22
#define OLED_SDA 21
//pins for sx1268 lora module
#define  NSS_pin 17
#define  DIO1_pin 5
#define  NRST_pin 16
#define  BUSY_pin 4

//pins for rotary and button
// #define ROTARY_PIN1  25
// #define ROTARY_PIN2 26
#define BUTTON_PIN  15
// #define CLICKS_PER_STEP   4    // this number depends on your rotary encoder 

//EEPROM
// #define BRIGHTNESS_SET_ADDR 0
// #define MONITOR_AUTO_CONTROL_ADDR 4
// #define EEPROM_SIZE 6

// used for getting bilibili fans number
// tutorial: https://www.bilibili.com/video/BV17V411q7oG?p=2
String FANS_URL = "http://api.bilibili.com/x/relation/stat?vmid=your_uid";


#define MODE_NUM 4   //number of modes
#define TEMP_HUMI_MODE 3
#define BILIBILI_MODE 1
#define RECORD_MODE 2
#define CHANGE_MOD 0
uint8_t screen_mode = BILIBILI_MODE;
