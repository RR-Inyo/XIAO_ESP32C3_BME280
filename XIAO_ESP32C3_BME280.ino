//----------------------------------------------------------------------
// XIAO_ESP32C3_BME280.ino
// A temperature/humidity/pressure monitor by Seeed XIAO ESP32C3
// (c) 2025 @RR_Inyo
// Released under the MIT license
// https://opensource.org/licenses/mit-license.php
//----------------------------------------------------------------------

#include <Wire.h>
#include <Adafruit_BME280.h>
#include "ST7032.hpp"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

//--------------------------------------------------
// Handler for BME280
//--------------------------------------------------
Adafruit_BME280 bme;

//--------------------------------------------------
// Handler for ST7032 character LCD
//--------------------------------------------------
ST7032 lcd;

//--------------------------------------------------
// Global constants and variables
//--------------------------------------------------

// Pins and I2C device settings
const int GPIO_SW1              = 20;
const int GPIO_SW2              = 5;
const int BME280_ADDR           = 0x76;
const int CONTRAST              = 44;

// WiFi settings
const int N_SSID                = 3;
const char* SSID[N_SSID]        = {"Your SSID 1", "Your SSID 2", "Your SSID 3"};
const char* PASSWORD[N_SSID]    = {"Your password 1", "Your password 2", "Your password 3"};
const int T_DELAY               = 1500;
const uint32_t T_WIFI_TIMEOUT   = 15000;
bool online;

// Constants and variables for NTP servers, date-time structure, and string
const char *server1             = "ntp.nict.jp";
const char *server2             = "time.google.com";
const char *server3             = "ntp.jst.mfeed.ad.jp";
const long JST                  = 3600L * 9;
const int summertime            = 0;
struct tm tm;
int tm_min_old                  = 0;
const char *months[]            = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char *dayofweek[]         = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *dayofweek_short[]   = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
char datebuf[32];
int tm_sec_old = 0;
unsigned long t_millis;

// Global variables to store latest measurement results
float t, h, p;

// Global variables to store tactile switch conditions
volatile bool sw1Pushed         = false;
volatile bool sw2Pushed         = false;
int scrMode                     = 0;

// Constants and variables for Google Spreadsheet API constants and variables
TaskHandle_t taskHandle_GSS;
const char* apiURL              = "Your Google Spreadsheet API URL";
int httpCode;
HTTPClient http;
bool postingNow = false;

// ----------------------------------------------------------------
// Function for WiFi connection
// ----------------------------------------------------------------
void connectWiFi() {
  for (int i = 0; i < N_SSID; i++) {
    // Wait for connection
    uint32_t tWiFi = millis();
    WiFi.begin(SSID[i], PASSWORD[i]);
    lcd.setCursor(0x00);
    lcd.putString("WiFi:");
    lcd.putString(SSID[i]);
    lcd.setCursor(0x40);
    lcd.putString("Connecting");

    while (WiFi.status() != WL_CONNECTED) {
      delay(T_DELAY * 2);
      lcd.putString(".");
      if (millis() - tWiFi > T_WIFI_TIMEOUT) {
        lcd.setCursor(0x40);
        lcd.putString("Failed!         ");
        delay(T_DELAY);
        lcd.clear();
        break;
      }
    }
    
    // Confirm connectionm, exit loop if connected
    if (WiFi.status() == WL_CONNECTED) {
      online = true;
      lcd.putString("OK!");
      delay(T_DELAY);
      
      // Show IP address
      uint32_t ip = WiFi.localIP();        
      int ip_octet_1 = (ip & 0x000000ff);        
      int ip_octet_2 = (ip & 0x0000ff00) >> 8;
      int ip_octet_3 = (ip & 0x00ff0000) >> 16;        
      int ip_octet_4 = (ip & 0xff000000) >> 24;
      char buf[32];
      sprintf(buf, "%d.%d.%d.%d", ip_octet_1, ip_octet_2, ip_octet_3, ip_octet_4);
      lcd.clear();
      lcd.setCursor(0x00);
      lcd.putString("IP:");
      lcd.putString(buf);
      lcd.setCursor(0x40);
      lcd.putString("RSSI:");
      lcd.putString(String(WiFi.RSSI()) + " dBm");
      delay(T_DELAY);
      lcd.clear();
      break;
    }
  }     
}

//--------------------------------------------------
// Task to post to Google Spreadsheet API
//--------------------------------------------------
void postGSS(void *pvParameters) {
  uint32_t ulNotifiedValue;
  char pubMessage[128];
  while (true) {
    // Wait for notification
    xTaskNotifyWait(0, 0, &ulNotifiedValue, portMAX_DELAY);

    // Flag up
    postingNow = true;
 
    // Create JSON message
    StaticJsonDocument<500> doc;
    JsonObject object = doc.to<JsonObject>();

    // Stuff data to JSON message and serialize it
    object["datetime"] = datebuf;
    object["temp"] = t;
    object["humid"] = h;
    object["press"] = p;
    serializeJson(doc, pubMessage);

    // Post to Google Spreadsheet API
    http.begin(apiURL);
    httpCode = http.POST(pubMessage);

    // Flag down
    postingNow = false;

    // Wait
    delay(100);
  }
}

//--------------------------------------------------
// ISR for SW1
//--------------------------------------------------
void IRAM_ATTR onSW1() {
  sw1Pushed = true;
}

//--------------------------------------------------
// ISR for SW2
//--------------------------------------------------
void IRAM_ATTR onSW2() {
  sw2Pushed = true;
}

//--------------------------------------------------
// The setup function
//--------------------------------------------------
void setup() {
  // Set up serial
  Serial.begin(115200);

  // Set pins connected to tactile switches
  pinMode(GPIO_SW1, INPUT_PULLUP);
  pinMode(GPIO_SW2, INPUT_PULLUP);
  pinMode(GPIO_NUM_8, INPUT_PULLUP);

  // Delay, magic
  delay(T_DELAY);

  // Initialize I2C bus and character LCD
  Wire.begin();

  // Show message on serial
  Serial.printf("Seeed XIAO ESP32C3 Air Monitor\n");

  // Set up BME280
  bool status = bme.begin(BME280_ADDR, &Wire);
  if (!status) {
    delay(10);
  }
  bme.init();

  lcd.begin();
  lcd.setContrast(CONTRAST);
  lcd.clear();

  // Show message
  lcd.setCursor(0x00);
  lcd.putString("XIAO ESP32C3    ");
  lcd.setCursor(0x40);
  lcd.putString("     Air Monitor");
  lcd.setContrast(CONTRAST);
  delay(1000);
  lcd.clear();

  // Connect to WiFi
  connectWiFi();

  // Synchronize real-time cllock to NTP servers
  if (online) {
    configTime(JST, summertime, server1, server2, server3);
  }

  // Attach ISR for push switches
  attachInterrupt(GPIO_SW1, onSW1, FALLING);    
  attachInterrupt(GPIO_SW2, onSW2, FALLING);

  // Create task to post to Google Spreadsheet
  if (online) {    
    xTaskCreateUniversal(
      postGSS,              // Function to be run as a task
      "postGSS",            // Task name
      8192,                 // Stack memory
      NULL,                 // Parameter
      1,                    // Priority
      &taskHandle_GSS,      // Task handler
      PRO_CPU_NUM           // Core to run the task
    );
  }
}

//--------------------------------------------------
// The loop function
//--------------------------------------------------
void loop() {
  // Get current time, if online
  if (online) {
    if (getLocalTime(&tm)) {
      // Store millis when second changes
      if (tm.tm_sec != tm_sec_old) {
        t_millis = millis();
      }
      tm_sec_old = tm.tm_sec;

      // Create string to post to Google Spreadsheet
      sprintf(datebuf, "%d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    }
  }

  // Toggle display mode if switch pushed, clear LCD
  if (sw1Pushed) {
    // Toggle only if current GPIO_SW1 status is HIGH
    if (digitalRead(GPIO_SW1) == HIGH) {
      sw1Pushed = false;
      lcd.clear();
      scrMode = (scrMode + 1) % 4;
    }
  }

  if (sw2Pushed) {
    // Toggle only if current GPIO_SW1 status is HIGH
    // Reset!
    if (digitalRead(GPIO_SW2) == HIGH) {
      sw2Pushed = false;
      ESP.restart();
    }
  }

  // Read from BME280, recording into global variables
  t = bme.readTemperature();
  h = bme.readHumidity();
  p = bme.readPressure() / 100.0;

  // Send data to serial
  Serial.printf("T: %4.2f degC, H: %4.2f%%, P: %6.2f hPa\n", t, h, p);
  if (!digitalRead(GPIO_SW1)) {
    Serial.printf("SW1 pushed!, ");
  }
  else {
    Serial.printf("SW1 released., ");
  }
    if (!digitalRead(GPIO_SW2)) {
    Serial.printf("SW2 pushed!\n");
  }
  else {
    Serial.printf("SW2 released, ");
  }

  if (postingNow) {
    Serial.printf("Posting to Google Spreadsheet.\n");
  }
  else {
    Serial.printf("\n");
  }

  // Show on LCD
  switch (scrMode) {

    // Show air conditions
    case 0:
    // Show on LCD, temperature
      lcd.setCursor(0x00);
      lcd.putString(String(t, 2));
      lcd.putChar((char) 0xdf);
      lcd.putString("C ");

      // Show in LCD, humidity
      lcd.setCursor(0x0a);
      lcd.putString(String(h, 2) + "% ");

      // Show in LCD, pressure
      lcd.setCursor(0x40);
      lcd.putString(String(p, 2) + " hPa ");

      break;

    // Show clock
    case 1:
      if (online) {
        char buf[20];
        sprintf(buf, "%d-%02d-%02d %s", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, dayofweek[tm.tm_wday]);
        lcd.setCursor(0x00);
        lcd.putString(buf);

        // Show hour, minute, and second
        sprintf(buf, "%02d:%02d:%02d JST", tm.tm_hour, tm.tm_min, tm.tm_sec);
        lcd.setCursor(0x40);
        lcd.putString(buf);
      }
      else {
        lcd.setCursor(0x00);
        lcd.putString("Clock:");
        lcd.setCursor(0x40);
        lcd.putString("Disconnected.");
      }

      break;

    // Show IP address
    case 2:
      lcd.setCursor(0x00);
      lcd.putString("IP:");

      if (WiFi.status() == WL_CONNECTED) {
        // Convert IP address to string
        uint32_t ip = WiFi.localIP();        
        int ip_octet_1 = (ip & 0x000000ff);        
        int ip_octet_2 = (ip & 0x0000ff00) >> 8;
        int ip_octet_3 = (ip & 0x00ff0000) >> 16;        
        int ip_octet_4 = (ip & 0xff000000) >> 24;
        char buf[20];
        sprintf(buf, "%d.%d.%d.%d", ip_octet_1, ip_octet_2, ip_octet_3, ip_octet_4);
        lcd.putString(buf);
        lcd.setCursor(0x40);
        lcd.putString("RSSI:");
        lcd.putString(String(WiFi.RSSI()) + " dBm");
      }
      else {
        lcd.setCursor(0x40);
        lcd.putString("Disconnected.");
      }

      break;
    
    // Show date, time, and air conditions
    case 3:
      // Show date and time
      if (online) {
        char buf[20];
        sprintf(buf, "%s-%02d %s ", months[tm.tm_mon], tm.tm_mday, dayofweek[tm.tm_wday]);
        lcd.setCursor(0x00);
        lcd.putString(buf);

        // Show hour, minute, and second
        char colon;
        if (millis() - t_millis < 500) {
          colon = ':';
        }
        else {
          colon = ' ';
        }
        sprintf(buf, "%02d%c%02d", tm.tm_hour, colon, tm.tm_min);
        lcd.putString(buf);
      }
      else {
        lcd.setCursor(0x00);
        lcd.putString("Clock: Discon'd.");
      }

      // Show air conditions
      lcd.setCursor(0x40);
      lcd.putString(String(t, 1));
      lcd.setCursor(0x46);
      lcd.putString(String(h, 0));
      lcd.setCursor(0x4a);
      lcd.putString(String(p, 1));
  }

  // Post to Google Spreadsheet every minute
  if (online) {
    if (tm.tm_min != tm_min_old) {
      // Notify task to post to GSS
      xTaskNotify(taskHandle_GSS, 0, eIncrement);

      // Preserve minute value
      tm_min_old = tm.tm_min;
    }
  }

  // Show mark on LCD when posting to Google Spreadsheet
  if (scrMode != 3) {
    lcd.setCursor(0x4f);
    if (postingNow) {
      lcd.putChar('G');  // Up arrow
    }
    else {
      lcd.putChar(' ');   // Space
    }
  }

  // Reconnection to WiFi if disconnected
  if (online && WiFi.status() != WL_CONNECTED) {
    online = false;
    connectWiFi();
  }

  // Wait
  delay(25);
}
