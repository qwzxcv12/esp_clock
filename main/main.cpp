#include <Arduino.h>
#include "arduino_secrets.h"

// This is how many color levels the display shows - the more the slower the update.
//#define PxMATRIX_COLOR_DEPTH 4

// Defines the speed of the SPI bus (reducing this may help if you experience noisy images).
#define PxMATRIX_SPI_FREQUENCY 10000000

// Creates a second buffer for backround drawing (doubles the required RAM).
//#define PxMATRIX_double_buffer true

//----------------------------------------Including Libraries.
#include <PxMatrix.h>
#include "RTClib.h"
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include "PageIndex.h"
#include "am-duong-lich.h"
#include <TimeLib.h>
#include <time.h>
//----------------------------------------
// Forward declarations
void handleRoot();
void handleSettings();
void get_All_Saved_Settings();
uint16_t getTextWidth(const char* text);
void run_Scrolling_Text(uint8_t speed, byte y_pos, const char* st_Text, uint16_t color);
void display_Data();
void handleNotFound();
void show_Text_Center(const char* text);
void wifi_Connect();
void time_Update();

//----------------------------------------Pins for LED MATRIX.
#define P_LAT 5
#define P_A   19
#define P_B   23
#define P_C   18
#define P_OE  4
//----------------------------------------

// Defines the width and height of the panel in pixels.
#define matrix_width  32
#define matrix_height 16

// Timer setup.
// Create a hardware timer  of ESP32.
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// This defines the 'on' time of the display is us.
// The larger this number, the brighter the display. If too large the ESP will crash.
uint8_t display_draw_time = 30; // 30-70 is usually fine.

// Declaring the "PxMATRIX" object as a "display" and its settings.
PxMATRIX display(matrix_width, matrix_height, P_LAT, P_OE, P_A, P_B, P_C);

//----------------------------------------Variable for some colors.
uint16_t myRED      = display.color565(255, 0, 0);
uint16_t myGREEN    = display.color565(0, 255, 0);
uint16_t myBLUE     = display.color565(0, 0, 255);
uint16_t myYELLOW   = display.color565(255, 255, 0);
uint16_t myCYAN     = display.color565(0, 255, 255);
uint16_t myFUCHSIA  = display.color565(255, 0, 255);
uint16_t myWHITE    = display.color565(255, 255, 255);
uint16_t myBLACK    = display.color565(0, 0, 0);

uint16_t myCOLOR_ARRAY[7] = {myRED, myGREEN, myBLUE, myYELLOW, myCYAN, myFUCHSIA, myWHITE};
int cnt_Color = 0;
int myCOLOR_ARRAY_Length = sizeof(myCOLOR_ARRAY) / sizeof(myCOLOR_ARRAY[0]);
//----------------------------------------

// Timer/millis variables for scrolling text.
unsigned long prevMill_Scroll_Text = 0;

// Variables used to scroll text.
int scrolling_Y_Pos = 0;
long scrolling_X_Pos;
long scrolling_X_Pos_CT;
uint16_t scrolling_Text_Color;
uint16_t text_Color;
char text_Scrolling_Text[151];
uint16_t text_Length_In_Pixel;
bool set_up_Scrolling_Text_Length = true;
bool start_Scroll_Text = false;
byte scrolling_text_Display_Order = 0;
bool reset_Scrolling_Text = false;

// Timer/millis variable to update time data.
unsigned long prevMill_Update_Time = 0;
const long interval_Update_Time = 1000;

// Timer/millis variable to display time (hours and minutes).
unsigned long prevMill_Show_Clock = 0;
const long interval_Show_Clock = 500;

// Variables to hold date and time data.
char daysOfTheWeek[7][10] = {"CHU NHAT", "THU 2", "THU 3", "THU 4", "THU 5", "THU 6", "THU 7"};
char chr_t_Minute[4];
byte minute_Val, last_minute_Val;
char chr_t_Hour[4];
char day_and_date_Text[100];
bool blink_Colon = false;
uint16_t clock_Color;
uint16_t day_and_date_Text_Color;

int d_Year;
byte d_Month, d_Day;
byte t_Hour, t_Minute, t_Second;
byte input_Display_Mode, input_Brightness, input_Scrolling_Speed;
int Color_Clock_R, Color_Clock_G, Color_Clock_B;
int Color_Date_R, Color_Date_G, Color_Date_B;
int Color_Text_R, Color_Text_G, Color_Text_B;
char input_Scrolling_Text[151];

int Text_Color_R = 255;
int Text_Color_G = 255;
int Text_Color_B = 255;

bool has_synced_today = false;
uint8_t current_brightness = 0; // Tracks actual display brightness to avoid redundant calls
//----------------------------------------Variable declaration for your network credentials.
const char* ssid = "Ze";  //--> Your wifi name.
const char* password = "thien1991"; //--> Your wifi password.
//----------------------------------------

//----------------------------------------Defining the key.
// The "key" works like a password. To control the "P10 RGB 32x16", users must know the â€œkeyâ€ .
// You can change it to another word.
#define key_Txt "123456789"
//----------------------------------------

// Declare the â€œRTC_DS3231â€  object as â€œrtcâ€ .
RTC_DS3231 rtc;

// Declaring the "Preferences" object as "preferences".
Preferences preferences;

// Server on port 80.
WebServer server(80);

// FreeRTOS Task Handle for Web Server
TaskHandle_t webServerTaskHandle;

void webServerTask(void * pvParameters) {
  for(;;) {
    server.handleClient();
    vTaskDelay(10 / portTICK_PERIOD_MS); // Yield 10ms to watchdog
  }
}






//________________________________________________________________________________ IRAM_ATTR display_updater()
// Interrupt handler for Timer.
void IRAM_ATTR display_updater(){
  // Increment the counter and set the time of ISR.
  portENTER_CRITICAL_ISR(&timerMux);
  display.display(display_draw_time);
  portEXIT_CRITICAL_ISR(&timerMux);
}
//________________________________________________________________________________ 

void display_update_enable(bool is_enable) {
  if (is_enable) {
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &display_updater, true);
    timerAlarmWrite(timer, 4000, true);
    timerAlarmEnable(timer);
  } else {
    timerDetachInterrupt(timer);
    timerAlarmDisable(timer);
  }
}
//________________________________________________________________________________ 






//________________________________________________________________________________ connecting_To_WiFi()
void connecting_To_WiFi() {
  //----------------------------------------Set Wifi to STA mode.
  Serial.println();
  Serial.println("-------------WIFI mode");
  Serial.println("WIFI mode : STA");
  WiFi.mode(WIFI_STA);
  Serial.println("-------------");
  delay(1000);
  //---------------------------------------- 

  //----------------------------------------Connect to Wi-Fi (STA).
  Serial.println();
  Serial.println("-------------Connection");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  //:::::::::::::::::: The process of connecting ESP32 with WiFi Hotspot / WiFi Router.
  // The process timeout of connecting ESP32 with WiFi Hotspot / WiFi Router is 20 seconds.
  // If within 20 seconds the ESP32 has not been successfully connected to WiFi, the ESP32 will restart.
  // I made this condition because on my ESP32, there are times when it seems like it can't connect to WiFi, so it needs to be restarted to be able to connect to WiFi.
  
  int connecting_process_timed_out = 20; //--> 20 = 20 seconds.
  connecting_process_timed_out = connecting_process_timed_out * 2;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    if(connecting_process_timed_out > 0) connecting_process_timed_out--;
    if(connecting_process_timed_out == 0) {
      Serial.println();
      Serial.println("Failed to connect to WiFi. The ESP32 will be restarted.");
      Serial.println("-------------");
      delay(1000);
      ESP.restart();
    }
  }
  
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("Successfully connected to : ");
  Serial.println(ssid);
  Serial.println("-------------");
  //:::::::::::::::::: 
  delay(1000);
  //---------------------------------------- 
}
//________________________________________________________________________________ 






//________________________________________________________________________________ prepare_and_start_The_Server()
void prepare_and_start_The_Server() {
  //----------------------------------------Setting the server.
  server.on("/", handleRoot); 
  server.on("/settings", handleSettings);
  delay(500);
  //----------------------------------------

  //----------------------------------------Start server.
  server.begin(); 
  Serial.println();
  Serial.println("HTTP server started");
  delay(500);
  //----------------------------------------

  //----------------------------------------Open the IP address in your browser to open the interface page.
  // Make sure that your computer / mobile device and ESP32 are connected to the same WiFi network.
  Serial.println();
  Serial.print("IP address : ");
  Serial.println(WiFi.localIP());
  Serial.println("Open the IP address in your browser to open the interface page.");
  Serial.println("Make sure that your computer / mobile device and");
  Serial.println("ESP32 are connected to the same WiFi network.");
  delay(500);
  //----------------------------------------
}
//________________________________________________________________________________ 






//________________________________________________________________________________ handleRoot()
// This routine is executed when you open ESP32 IP Address in browser.
void handleRoot() {
  server.send(200, "text/html", MAIN_page); //--> Send web page.
}
//________________________________________________________________________________ handleSettings().
// Subroutines for handling handle settings from clients.
// OPTIMIZED: Removed display interrupt toggling and unnecessary delays.
// NVS (Preferences) is thread-safe and does not require disabling the display.
void handleSettings() {
  String incoming_Settings;
  
  incoming_Settings = server.arg("key");
  
  Serial.println();
  Serial.println("-------------Settings");
  Serial.print("Key : ");
  Serial.println(incoming_Settings);

  // Conditions for checking keys.
  if (incoming_Settings == key_Txt) {
    incoming_Settings = server.arg("sta");

    // Conditions for setting the Clock and Date.
    if (incoming_Settings == "setTimeDate") {
      incoming_Settings = server.arg("d_Year");
      d_Year = incoming_Settings.toInt();
      incoming_Settings = server.arg("d_Month");
      d_Month = incoming_Settings.toInt();
      incoming_Settings = server.arg("d_Day");
      d_Day = incoming_Settings.toInt();
      incoming_Settings = server.arg("t_Hour");
      t_Hour = incoming_Settings.toInt();
      incoming_Settings = server.arg("t_Minute");
      t_Minute = incoming_Settings.toInt();
      incoming_Settings = server.arg("t_Second");
      t_Second = incoming_Settings.toInt();

      char TD[40];
      snprintf(TD, sizeof(TD), "Date : %d-%d-%d | Time : %d-%d-%d", d_Day, d_Month, d_Year, t_Hour, t_Minute, t_Second);
      
      Serial.println();
      Serial.println("Set Time and Date.");
      Serial.print("DateTime : ");
      Serial.println(TD);

      Serial.println("Set Time and Date...");
      rtc.adjust(DateTime(d_Year, d_Month, d_Day, t_Hour, t_Minute, t_Second));
      Serial.println("Setting the Time and Date has been completed.");
    }

    // Conditions for setting Display Mode.
    if (incoming_Settings == "setDisplayMode") {
      incoming_Settings = server.arg("input_Display_Mode");
      byte new_Display_Mode = incoming_Settings.toInt();
      
      Serial.println();
      Serial.println("Set Display Mode.");
      Serial.print("Display Mode : ");
      Serial.println(new_Display_Mode);

      // Check-before-write: Only write to Flash if value changed
      if (new_Display_Mode != input_Display_Mode) {
        input_Display_Mode = new_Display_Mode;
        preferences.begin("mySettings", false);
        preferences.putInt("input_DM", input_Display_Mode);
        preferences.end();
      }

      if (input_Display_Mode == 1) {
        clock_Color = display.color565(Color_Clock_R, Color_Clock_G, Color_Clock_B);
        day_and_date_Text_Color = display.color565(Color_Date_R, Color_Date_G, Color_Date_B);
        text_Color = display.color565(Color_Text_R, Color_Text_G, Color_Text_B);
      }

      display.clearDisplay();
      reset_Scrolling_Text = true;
      scrolling_text_Display_Order = 0;
      
      Serial.println("Set and save Display Mode is complete.");
    }

    // Conditions for setting Brightness.
    if (incoming_Settings == "setBrightness") {
      incoming_Settings = server.arg("input_Brightness");
      int temp_Bright = incoming_Settings.toInt();
      if (temp_Bright > 255) temp_Bright = 255;
      if (temp_Bright < 0)   temp_Bright = 0;
      
      Serial.println();
      Serial.println("Set Brightness.");
      Serial.print("Brightness : ");
      Serial.println(temp_Bright);

      // Check-before-write: Only write to Flash if value changed
      if ((byte)temp_Bright != input_Brightness) {
        input_Brightness = temp_Bright;
        preferences.begin("mySettings", false);
        preferences.putInt("input_BRT", input_Brightness);
        preferences.end();
      }

      display.setBrightness(input_Brightness);
      
      Serial.println("Set and save Brightness is complete.");
    }

    // Conditions for setting Clock Color.
    if (incoming_Settings == "setColorClock") {
      if (input_Display_Mode == 2) {
        server.send(200, "text/plane", "+ERR_DM");
        Serial.println("-------------");
        return;
      }
      
      int new_R = server.arg("Color_Clock_R").toInt();
      int new_G = server.arg("Color_Clock_G").toInt();
      int new_B = server.arg("Color_Clock_B").toInt();
      
      Serial.println();
      Serial.println("Set Clock Color.");
      Serial.print("Clock Color (RGB) : ");
      Serial.print(new_R);Serial.print(",");Serial.print(new_G);Serial.print(",");Serial.println(new_B);

      // Check-before-write: Only write to Flash if any value changed
      if (new_R != Color_Clock_R || new_G != Color_Clock_G || new_B != Color_Clock_B) {
        Color_Clock_R = new_R;
        Color_Clock_G = new_G;
        Color_Clock_B = new_B;
        preferences.begin("mySettings", false);
        preferences.putInt("CC_R", Color_Clock_R);
        preferences.putInt("CC_G", Color_Clock_G);
        preferences.putInt("CC_B", Color_Clock_B);
        preferences.end();
      }

      clock_Color = display.color565(Color_Clock_R, Color_Clock_G, Color_Clock_B);
      
      Serial.println("Set and save Clock Color is complete.");
    }

    // Conditions for setting Date Color.
    if (incoming_Settings == "setColorDate") {
      if (input_Display_Mode == 2) {
        server.send(200, "text/plane", "+ERR_DM");
        Serial.println("-------------");
        return;
      }

      int new_R = server.arg("Color_Date_R").toInt();
      int new_G = server.arg("Color_Date_G").toInt();
      int new_B = server.arg("Color_Date_B").toInt();
      
      Serial.println();
      Serial.println("Set Date Color.");
      Serial.print("Date Color (RGB) : ");
      Serial.print(new_R);Serial.print(",");Serial.print(new_G);Serial.print(",");Serial.println(new_B);

      // Check-before-write
      if (new_R != Color_Date_R || new_G != Color_Date_G || new_B != Color_Date_B) {
        Color_Date_R = new_R;
        Color_Date_G = new_G;
        Color_Date_B = new_B;
        preferences.begin("mySettings", false);
        preferences.putInt("DC_R", Color_Date_R);
        preferences.putInt("DC_G", Color_Date_G);
        preferences.putInt("DC_B", Color_Date_B);
        preferences.end();
      }

      display.clearDisplay();
      reset_Scrolling_Text = true;
      scrolling_text_Display_Order = 0;
      day_and_date_Text_Color = display.color565(Color_Date_R, Color_Date_G, Color_Date_B);
      
      Serial.println("Set and save Date Color is complete.");
    }

    // Conditions for setting text on Scrolling Text.
    if (incoming_Settings == "setScrollingText") {
      incoming_Settings = server.arg("input_Scrolling_Text");
      incoming_Settings.toCharArray(input_Scrolling_Text, incoming_Settings.length() + 1);
      
      Serial.println();
      Serial.println("Set Scrolling Text.");
      Serial.print("Text : ");
      Serial.println(input_Scrolling_Text);

      preferences.begin("mySettings", false);
      preferences.putString("input_ST", input_Scrolling_Text);
      preferences.end();

      display.clearDisplay();
      reset_Scrolling_Text = true;
      scrolling_text_Display_Order = 1;
      
      Serial.println("Set and save Scrolling Text is complete.");
    }

    // Conditions for setting Text Color.
    if (incoming_Settings == "setTextColor") {
      if (input_Display_Mode == 2) {
        server.send(200, "text/plane", "+ERR_DM");
        Serial.println("-------------");
        return;
      }
      
      int new_R = server.arg("Color_Text_R").toInt();
      int new_G = server.arg("Color_Text_G").toInt();
      int new_B = server.arg("Color_Text_B").toInt();
      
      Serial.println();
      Serial.println("Set Text Color.");
      Serial.print("Text Color (RGB) : ");
      Serial.print(new_R);Serial.print(",");Serial.print(new_G);Serial.print(",");Serial.println(new_B);

      // Check-before-write
      if (new_R != Color_Text_R || new_G != Color_Text_G || new_B != Color_Text_B) {
        Color_Text_R = new_R;
        Color_Text_G = new_G;
        Color_Text_B = new_B;
        preferences.begin("mySettings", false);
        preferences.putInt("TC_R", Color_Text_R);
        preferences.putInt("TC_G", Color_Text_G);
        preferences.putInt("TC_B", Color_Text_B);
        preferences.end();
      }

      display.clearDisplay();
      reset_Scrolling_Text = true;
      scrolling_text_Display_Order = 1;
      text_Color = display.color565(Color_Text_R, Color_Text_G, Color_Text_B);
      
      Serial.println("Set and save Text Color is complete.");
    }

    // Conditions for setting Scrolling Speed.
    if (incoming_Settings == "setScrollingSpeed") {
      incoming_Settings = server.arg("input_Scrolling_Speed");
      byte new_Speed = incoming_Settings.toInt();
      
      Serial.println();
      Serial.println("Set Scrolling Speed.");
      Serial.print("Scrolling Speed : ");
      Serial.println(new_Speed);

      // Check-before-write
      if (new_Speed != input_Scrolling_Speed) {
        input_Scrolling_Speed = new_Speed;
        preferences.begin("mySettings", false);
        preferences.putInt("input_SS", input_Scrolling_Speed);
        preferences.end();
      }
      
      Serial.println("Set and save Scrolling Speed is complete.");
    }

    // Sends settings stored in flash memory to the client.
    if (incoming_Settings == "getSettings") {
      Serial.println();
      Serial.println("Get Settings.");

      get_All_Saved_Settings();
      
      char send_Settings[200];
      snprintf(send_Settings, sizeof(send_Settings), "%d|%d|%d|%d|%d|%d|%d|%d|%s|%d|%d|%d|%d", input_Display_Mode, input_Brightness, 
                                                                       Color_Clock_R, Color_Clock_G, Color_Clock_B,
                                                                       Color_Date_R, Color_Date_G, Color_Date_B,
                                                                       input_Scrolling_Text,
                                                                       Color_Text_R, Color_Text_G, Color_Text_B,
                                                                       input_Scrolling_Speed);

      Serial.print("Settings Data :");
      Serial.println(send_Settings);

      server.send(200, "text/plane", send_Settings);
    }
    Serial.println("-------------");

    server.send(200, "text/plane", "+OK");
  } else {
    Serial.println();
    Serial.println("Wrong Key Text !");
    Serial.println("Please enter the correct Key Text.");
    Serial.println("-------------");
    
    server.send(200, "text/plane", "+ERR");
  }
}
//________________________________________________________________________________






//________________________________________________________________________________ get_All_Saved_Settings()
void get_All_Saved_Settings() {
  // Open Preferences (read-only mode).
  preferences.begin("mySettings", true);

  input_Display_Mode = preferences.getInt("input_DM", 1);
  input_Brightness = preferences.getInt("input_BRT", 125);
  Color_Clock_R = preferences.getInt("CC_R", 255);
  Color_Clock_G = preferences.getInt("CC_G", 255);
  Color_Clock_B = preferences.getInt("CC_B", 255);
  clock_Color = display.color565(Color_Clock_R, Color_Clock_G, Color_Clock_B);
  Color_Date_R = preferences.getInt("DC_R", 255);
  Color_Date_G = preferences.getInt("DC_G", 255);
  Color_Date_B = preferences.getInt("DC_B", 255);
  day_and_date_Text_Color = display.color565(Color_Date_R, Color_Date_G, Color_Date_B);
  String my_Scrolling_Text = preferences.getString("input_ST", "");
  int my_Scrolling_Text_Length = my_Scrolling_Text.length() + 1;
  my_Scrolling_Text.toCharArray(input_Scrolling_Text, my_Scrolling_Text_Length);
  Color_Text_R = preferences.getInt("TC_R", 255);
  Color_Text_G = preferences.getInt("TC_G", 255);
  Color_Text_B = preferences.getInt("TC_B", 255);
  text_Color = display.color565(Color_Text_R, Color_Text_G, Color_Text_B);
  input_Scrolling_Speed = preferences.getInt("input_SS", 35);

  // Close the Preferences.
  preferences.end();

  Serial.println("-------------");
  Serial.println("All Saved Settings.");
  Serial.print("Display Mode : ");
  Serial.println(input_Display_Mode);
  Serial.print("Brightness : ");
  Serial.println(input_Brightness);
  Serial.print("Clock Color (RGB) : ");
  Serial.print(Color_Clock_R);Serial.print(",");Serial.print(Color_Clock_G);Serial.print(",");Serial.println(Color_Clock_B);
  Serial.print("Date Color (RGB) : ");
  Serial.print(Color_Date_R);Serial.print(",");Serial.print(Color_Date_G);Serial.print(",");Serial.println(Color_Date_B);
  Serial.print("Scrolling Text : ");
  Serial.println(input_Scrolling_Text);
  Serial.print("Text Color (RGB) : ");
  Serial.print(Color_Text_R);Serial.print(",");Serial.print(Color_Text_G);Serial.print(",");Serial.println(Color_Text_B);
  Serial.print("Scrolling Speed : ");
  Serial.println(input_Scrolling_Speed);
  Serial.println("-------------");
}
//________________________________________________________________________________






//________________________________________________________________________________ run_Scrolling_Text()
void run_Scrolling_Text(uint8_t st_Y_Pos, byte st_Speed, const char* st_Text, uint16_t st_Color) {
  if (start_Scroll_Text == true && set_up_Scrolling_Text_Length == true) {
    if (strlen(st_Text) > 0) {
      text_Length_In_Pixel = getTextWidth(st_Text);
      scrolling_X_Pos = matrix_width;
      
      set_up_Scrolling_Text_Length = false;
    } else {
      start_Scroll_Text = false;
      return;
    }
  }

  if (reset_Scrolling_Text == true) {
    set_up_Scrolling_Text_Length = true;
    start_Scroll_Text = false;
    reset_Scrolling_Text = false;
    
    return;
  }

  unsigned long currentMillis_Scroll_Text = millis();
  if (currentMillis_Scroll_Text - prevMill_Scroll_Text >= st_Speed) {
    prevMill_Scroll_Text = currentMillis_Scroll_Text;

    scrolling_X_Pos--;
    if (scrolling_X_Pos < -(matrix_width + text_Length_In_Pixel)) {
      set_up_Scrolling_Text_Length = true;
      start_Scroll_Text = false;
      
      return;
    }

    scrolling_X_Pos_CT = scrolling_X_Pos + 1;
    
    display.setTextColor(myBLACK);
    display.setCursor(scrolling_X_Pos_CT, st_Y_Pos);
    display.print(st_Text);
    
    display.setTextColor(st_Color);
    display.setCursor(scrolling_X_Pos, st_Y_Pos);
    display.print(st_Text);
  }
}
//________________________________________________________________________________ 






//________________________________________________________________________________ getTextWidth()
// Subroutine to get the length of text in pixels.
uint16_t getTextWidth(const char* text) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return w;
}
//________________________________________________________________________________ 






//________________________________________________________________________________ drawColon()
void drawColon(int16_t x, int16_t y, uint16_t colonColor) {
  display.drawPixel(x, y, colonColor);
  display.drawPixel(x+1, y, colonColor);
  display.drawPixel(x, y+1, colonColor);
  display.drawPixel(x+1, y+1, colonColor);

  display.drawPixel(x, y+3, colonColor);
  display.drawPixel(x+1, y+3, colonColor);
  display.drawPixel(x, y+4, colonColor);
  display.drawPixel(x+1, y+4, colonColor);
}
//________________________________________________________________________________ 






//________________________________________________________________________________ get_Time()
void get_Time() {
  DateTime now = rtc.now();

  minute_Val = now.minute();
  
  snprintf(chr_t_Hour, sizeof(chr_t_Hour), "%02d", now.hour());
  snprintf(chr_t_Minute, sizeof(chr_t_Minute), "%02d", now.minute());

  // Smart Auto-Brightness (Dimming at night 22:00 -> 06:00)
  // Only update brightness when value actually changes to prevent micro-flickers
  uint8_t target_brightness = (now.hour() >= 22 || now.hour() < 6) ? 15 : input_Brightness;
  if (target_brightness != current_brightness) {
    current_brightness = target_brightness;
    display.setBrightness(current_brightness);
  }

  // Daily NTP Sync at 3 AM
  if (now.hour() == 3 && !has_synced_today) {
    if (WiFi.status() == WL_CONNECTED) {
      configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 10000)) {
        rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
      }
    }
    has_synced_today = true;
  } else if (now.hour() == 4) {
    has_synced_today = false;
  }
}
//________________________________________________________________________________ 






//________________________________________________________________________________ get_Date()
void get_Date() {
  // Láº¥y thá» i gian tá»« RTC
  DateTime now = rtc.now();

  // Ä á»‹nh dáº¡ng ngÃ y thÃ¡ng vÃ  tÃªn ngÃ y trong tuáº§n
  snprintf(day_and_date_Text, sizeof(day_and_date_Text), "%s, %02d-%02d-%d", 
          daysOfTheWeek[now.dayOfTheWeek()], // Láº¥y tÃªn ngÃ y trong tuáº§n
          now.day(),                         // Láº¥y ngÃ y
          now.month(),                       // Láº¥y thÃ¡ng
          now.year());                       // Láº¥y nÄƒm
  // Chuyá»n Äá»i ngÃ y dÆ°Æ¡ng sang Ã¢m lá»ch
  double timeZone = 7.0; // MÃºi giá» Viá»t Nam
  std::array<int, 4> lunarDate = convertToLunarCalendar(now.day(), now.month(), now.year(), timeZone);

  // ThÃªm thÃ´ng tin Ã¢m lá»ch vÃ o biáº¿n day_and_date_Text
  char lunarText[50];
  snprintf(lunarText, sizeof(lunarText), " | AL: %02d-%02d-%d", lunarDate[0], lunarDate[1], lunarDate[2]);
  
  // Ná»i thÃ´ng tin Ã¢m lá»ch vÃ o day_and_date_Text
  strcat(day_and_date_Text, lunarText);
  
  // In ra thÃ´ng tin ngÃ y dÆ°Æ¡ng vÃ  Ã¢m lá»ch
  Serial.println(day_and_date_Text);
}

//________________________________________________________________________________ 






//________________________________________________________________________________ VOID SETUP()
void setup() {
  // put your setup code here, to run once:

  delay(1000);
  Serial.begin(115200);
  Serial.println();

  get_All_Saved_Settings();

  //----------------------------------------Starting and setting up the DS3231 RTC module.
  Serial.println();
  Serial.println("------------");
  Serial.println("Starting the DS3231 RTC module.");
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  Serial.println("Successfully started the DS3231 RTC module.");
  Serial.println("------------");
  //----------------------------------------

  // Display initialization.
  display.begin(8); //--> Value 8 for 1/8 row scan panel.
  delay(100);

  // Enable Timer Interrupts.
  display_update_enable(true);
  delay(100);

  display.clearDisplay();
  delay(1000);

  display.setBrightness(input_Brightness); //--> Range from 0 to 255.
  delay(100);

  display.fillScreen(myRED);
  delay(1000);
  display.fillScreen(myGREEN);
  delay(1000);
  display.fillScreen(myBLUE);
  delay(1000);
  display.fillScreen(myWHITE);
  delay(1000);

  display.clearDisplay();
  delay(1000);

  display.setTextWrap(false);
  display.setTextSize(1);
  display.setRotation(0);
  delay(100);

  start_Scroll_Text = true;
  while(true) {run_Scrolling_Text(4, 35, "Connecting to WiFi.", myBLUE); delay(1); if (start_Scroll_Text == false) break;}
  delay(500);

  start_Scroll_Text = true;
  while(true) {run_Scrolling_Text(4, 35, "Please Wait...", myRED); delay(1); if (start_Scroll_Text == false) break;}
  delay(500);

  // Disable Timer Interrupts.
  display_update_enable(false);
  delay(1000);

  connecting_To_WiFi();
  delay(1000);

  prepare_and_start_The_Server();
  delay(1000);

  // Enable Timer Interrupts.
  display_update_enable(true);
  delay(1000);

  start_Scroll_Text = true;
  while(true) {run_Scrolling_Text(4, 35, "Successfully connected to WiFi.", myGREEN); delay(1); if (start_Scroll_Text == false) break;}
  delay(500);

  char IP_Add[30];
  snprintf(IP_Add, sizeof(IP_Add), "IP Address : %s", WiFi.localIP().toString().c_str());
  start_Scroll_Text = true;
  while(true) {run_Scrolling_Text(4, 35, IP_Add, myWHITE); delay(1); if (start_Scroll_Text == false) break;}
  delay(500);

  //----------------------------------------NTP Time Sync
  start_Scroll_Text = true;
  while(true) {run_Scrolling_Text(4, 35, "Syncing time via NTP...", myYELLOW); delay(1); if (start_Scroll_Text == false) break;}

  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // UTC+7 for Vietnam
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10000)) { // Wait up to 10 seconds for time sync
    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    start_Scroll_Text = true;
    while(true) {run_Scrolling_Text(4, 35, "NTP Sync Success!", myGREEN); delay(1); if (start_Scroll_Text == false) break;}
  } else {
    start_Scroll_Text = true;
    while(true) {run_Scrolling_Text(4, 35, "NTP Sync Failed!", myRED); delay(1); if (start_Scroll_Text == false) break;}
  }
  delay(500);
  //----------------------------------------

  // Start FreeRTOS Web Server Task on Core 0
  xTaskCreatePinnedToCore(
    webServerTask,
    "WebServerTask",
    4096,
    NULL,
    1,
    &webServerTaskHandle,
    0
  );
}
//________________________________________________________________________________ 






//________________________________________________________________________________ VOID LOOP()
void loop() {
  // put your main code here, to run repeatedly:





  //----------------------------------------input_Display_Mode = 1.
  if (input_Display_Mode == 1) {
    //::::::::::::::::::Timer/Millis to update clock data.
    unsigned long currentMillis_Update_Time = millis();
    if (currentMillis_Update_Time - prevMill_Update_Time >= interval_Update_Time) {
      prevMill_Update_Time = currentMillis_Update_Time;

      get_Time();
      blink_Colon = !blink_Colon;
    }
    //::::::::::::::::::

    //::::::::::::::::::Timer/Millis to display hours and minutes.
    unsigned long currentMillis_Show_Clock = millis();
    if (currentMillis_Show_Clock - prevMill_Show_Clock >= interval_Show_Clock) {
      prevMill_Show_Clock = currentMillis_Show_Clock;

      display.setTextSize(1);

      //clock_Color = myRED;

      if (last_minute_Val != minute_Val) display.fillRect(1, 0, 11, 7, myBLACK);
      display.setTextColor(clock_Color);
      display.setCursor(1, 0);
      display.print(chr_t_Hour);

      if (blink_Colon == true) {
        drawColon(15, 1, clock_Color);
      } else {
        drawColon(15, 1, myBLACK);
      }

      if (last_minute_Val != minute_Val) display.fillRect(20, 0, 11, 7, myBLACK);
      display.setTextColor(clock_Color);
      display.setCursor(20, 0);
      display.print(chr_t_Minute);

      last_minute_Val = minute_Val;
    }
    //::::::::::::::::::

    //::::::::::::::::::Conditions for setting and preparing scrolling text.
    // "start_Scroll_Text = false" means scrolling has not been executed.
    // After the settings and preparations for scrolling text below are complete, "start_Scroll_Text = true" to start scrolling text.
    // "start_Scroll_Text" will return "false" if scrolling the text is complete or the scrolled text is empty.
    if (start_Scroll_Text == false) {
      scrolling_text_Display_Order++;
      if (scrolling_text_Display_Order > 2) scrolling_text_Display_Order = 1;

      // Conditions for scrolling text containing the name of the day and date.
      if (scrolling_text_Display_Order == 1) {
        get_Date();
        display.setTextSize(1);
        scrolling_Y_Pos = 8;  //--> Y position settings for scrolling text.
        //input_Scrolling_Speed = 45; //--> Speed ââsettings for scrolling text.
        //day_and_date_Text_Color = myGREEN;
        scrolling_Text_Color = day_and_date_Text_Color; //--> Color settings for scrolling text. You can also use: scrolling_Text_Color = display.color565(255, 0, 0);
        strcpy(text_Scrolling_Text, day_and_date_Text);  //--> Sets the displayed text on scrolling text.
      }

      // Conditions for scrolling the text you want.
      if (scrolling_text_Display_Order == 2) {
        display.setTextSize(1);
        scrolling_Y_Pos = 8;  //--> Y position settings for scrolling text.
        //input_Scrolling_Speed = 45; //--> Speed ââsettings for scrolling text.
        //text_Color = myBLUE;
        scrolling_Text_Color = text_Color;  //--> Color settings for scrolling text. You can also use: scrolling_Text_Color = display.color565(255, 0, 0);
        strcpy(text_Scrolling_Text, input_Scrolling_Text);  //--> Sets the displayed text on scrolling text.
      }

      start_Scroll_Text = true;
    }
    //::::::::::::::::::
  }
  //----------------------------------------
  



  //----------------------------------------input_Display_Mode = 2.
  if (input_Display_Mode == 2) {
    //::::::::::::::::::
    unsigned long currentMillis_Update_Time = millis();
    if (currentMillis_Update_Time - prevMill_Update_Time >= interval_Update_Time) {
      prevMill_Update_Time = currentMillis_Update_Time;

      get_Time();
      blink_Colon = !blink_Colon;
    }
    //::::::::::::::::::

    //::::::::::::::::::
    unsigned long currentMillis_Show_Clock = millis();
    if (currentMillis_Show_Clock - prevMill_Show_Clock >= interval_Show_Clock) {
      prevMill_Show_Clock = currentMillis_Show_Clock;

      display.setTextSize(1);

      clock_Color = myCOLOR_ARRAY[cnt_Color];

      if (last_minute_Val != minute_Val) display.fillRect(1, 0, 11, 7, myBLACK);
      display.setTextColor(clock_Color);
      display.setCursor(1, 0);
      display.print(chr_t_Hour);

      if (blink_Colon == true) {
        drawColon(15, 1, clock_Color);
      } else {
        drawColon(15, 1, myBLACK);
      }

      if (last_minute_Val != minute_Val) display.fillRect(20, 0, 11, 7, myBLACK);
      display.setTextColor(clock_Color);
      display.setCursor(20, 0);
      display.print(chr_t_Minute);

      last_minute_Val = minute_Val;
    }
    //::::::::::::::::::

    //::::::::::::::::::
    if (start_Scroll_Text == false) {
      scrolling_text_Display_Order++;
      if (scrolling_text_Display_Order > 3) scrolling_text_Display_Order = 1;

      if (scrolling_text_Display_Order == 1) {
        get_Date();
        display.setTextSize(1);
        scrolling_Y_Pos = 8;  //--> Y position settings for scrolling text.
        //input_Scrolling_Speed = 45; //--> Speed ââsettings for scrolling text.

        int next_cnt_Color = cnt_Color + 1;
        if (next_cnt_Color > (myCOLOR_ARRAY_Length - 1)) next_cnt_Color = cnt_Color - (myCOLOR_ARRAY_Length - 1);
        day_and_date_Text_Color = myCOLOR_ARRAY[next_cnt_Color];
      
        scrolling_Text_Color = day_and_date_Text_Color; //--> Color settings for scrolling text. You can also use: scrolling_Text_Color = display.color565(255, 0, 0);
        strcpy(text_Scrolling_Text, day_and_date_Text);  //--> Sets the displayed text on scrolling text.
      }

      if (scrolling_text_Display_Order == 2) {
        display.setTextSize(1);
        scrolling_Y_Pos = 8;  //--> Y position settings for scrolling text.
        //input_Scrolling_Speed = 45; //--> Speed ââsettings for scrolling text.

        int next_cnt_Color = cnt_Color + 2;
        if (next_cnt_Color > (myCOLOR_ARRAY_Length - 1)) next_cnt_Color = cnt_Color - (myCOLOR_ARRAY_Length - 2);
        text_Color = myCOLOR_ARRAY[next_cnt_Color];
        
        scrolling_Text_Color = text_Color; //--> Color settings for scrolling text. You can also use: scrolling_Text_Color = display.color565(255, 0, 0);      
        strcpy(text_Scrolling_Text, input_Scrolling_Text);  //--> Sets the displayed text on scrolling text.
      }

      // Conditions for changing color.
      if (scrolling_text_Display_Order == 3) {
        cnt_Color++;
        if (cnt_Color > (myCOLOR_ARRAY_Length - 1)) cnt_Color = 0;

        strcpy(text_Scrolling_Text, "");
      }

      start_Scroll_Text = true;
    }
    //::::::::::::::::::
  }
  //----------------------------------------
  


  
  if (start_Scroll_Text == true) {
    run_Scrolling_Text(scrolling_Y_Pos, input_Scrolling_Speed, text_Scrolling_Text, scrolling_Text_Color);
  }
}
//________________________________________________________________________________ 
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
