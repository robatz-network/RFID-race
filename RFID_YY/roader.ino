#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include "leds.h"
#include <esp32-hal-bt.h>
#include <esp_bt.h>
#include <EEPROM.h>

#define RST_PIN     22
#define SS_PIN      21
#define VOLTAGE_PIN 33

#define RTC_RST     26
#define RTC_CLK     14
#define RTC_DATA    27

#define LED_RED     16
#define LED_GREEN   17
#define LED_BLUE    5

#define BUTTON_PIN  32

#define WIFI_RETRIES 30
#define WIFI_SSID "Roomi89"
#define WIFI_PASS "room8989"
#define AUTO_FLUSH_INTERVAL (35000UL)

#define uS_TO_S_FACTOR 1000000  //Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  30        //Time ESP32 will go to sleep (in seconds)


int analogInput = 33 ;
float vin = 0.0;
float k_divider = 4.83; // resistor divider 47k 12k
float v_ref_ADC = 1.1;
int value = 0;
int lastAddr = 0;

MFRC522 mfrc522(SS_PIN, RST_PIN);

ThreeWire myWire(RTC_DATA, RTC_CLK, RTC_RST); // IO, SCLK, CE

RtcDS1302<ThreeWire> Rtc(myWire);

struct RfidScanEvent {
  byte uid[4];
  long unsigned int time;
  int voltage;
  byte checkSum;
};

struct RfidScanEvent rfidScanEvents[200];

int nextRfidScanEventIdx = 0;
unsigned long lastFlushAttemptAt = 0;

void checkRFIDScanner(void *parameter);
void flushData(void *parameter);
String formatRfidScanEvent(const RfidScanEvent &wpt);
String formatBatteryVoltage(const uint16_t voltage);
uint16_t getBatteryVoltage();

void checkRfidScannerTask(void *parameter);
void flushDataTask(void *parameter);
void sleepStart();

void setup()
{
  SPI.begin();
  Serial.begin(115200);

  mfrc522.PCD_Init();
  mfrc522.PCD_DumpVersionToSerial();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.print("compiled: ");
  Serial.print(DATE);
  Serial.println(TIME);
  
  Rtc.Begin();
  
  EEPROM.begin(128);
  
  RtcDateTime compiled = RtcDateTime(DATE, TIME);
  Rtc.SetDateTime(compiled);
  
  WiFi.mode(WIFI_OFF);
  btStop();
  
  setupLed(LED_RED);
  setupLed(LED_GREEN);
  setupLed(LED_BLUE);
  
  pinMode(VOLTAGE_PIN, INPUT);
  
  rfidScanEvents[nextRfidScanEventIdx].voltage = analogRead(VOLTAGE_PIN);
  
  xTaskCreate(&flushDataTask, "flushDataTask", 8192, NULL, 1, NULL, 0);
  xTaskCreate(&checkRfidScannerTask, "checkRfidScannerTask", 8192, NULL, 5, NULL, 0);   //Change last arg to 1 to enable multi-core processing
  
  delay(2000);
  
  sleepStart();
}

void loop()
{
  //infinite loop
}



void checkRfidScannerTask(void *parameter)
{
  for (;;)
  {
    checkRfidScanner();
    vTaskDelay(100);
  }
  vTaskDelete(NULL);
}



void flushDataTask(void *parameter)
{
  for (;;)
  {
    if (lastFlushAttemptAt == 0 || millis() - lastFlushAttemptAt >= AUTO_FLUSH_INTERVAL)
    {
      flushData();
    }
    vTaskDelay(1000);
  }
  vTaskDelete(NULL);
}



void checkRfidScanner()
{
  for (;;)
  {
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
    {
      return;
    }
    String uidString = "";
    for (int i = 0; i < 4; i++)
    {
      rfidScanEvents[nextRfidScanEventIdx].uid[i] = mfrc522.uid.uidByte[i];
      uidString += String(rfidScanEvents[nextRfidScanEventIdx].uid[i]);
      Serial.println(rfidScanEvents[nextRfidScanEventIdx].uid[i]);
    }
    
    blinkLed(LED_GREEN, 500);
    
    if (nextRfidScanEventIdx == 0 || (nextRfidScanEventIdx > 0 && (String(rfidScanEvents[nextRfidScanEventIdx - 1].uid[0]) + String(rfidScanEvents[nextRfidScanEventIdx - 1].uid[1]) + String(rfidScanEvents[nextRfidScanEventIdx - 1].uid[2]) + String(rfidScanEvents[nextRfidScanEventIdx - 1].uid[2]))  != uidString))
    {
      RtcDateTime now = Rtc.GetDateTime();
      rfidScanEvents[nextRfidScanEventIdx].time = now.Epoch32Time();
      rfidScanEvents[nextRfidScanEventIdx].checkSum = sizeof(rfidScanEvents[nextRfidScanEventIdx].uid) + sizeof(rfidScanEvents[nextRfidScanEventIdx].voltage) + sizeof(rfidScanEvents[nextRfidScanEventIdx].time);
      EEPROM.put(Addr, rfidScanEvents[nextRfidScanEventIdx]);
      lastAddr += sizeof(RfidScanEvent);
      EEPROM.commit();
      nextRfidScanEventIdx++;
    }
if (nextRfidScanEventIdx > 0)
    {
      flushData();
    }
  }
}



bool connectWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WIFI OK");
    return true;
  }

  int timeout = 2000;

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);
    timeout -= 250;
    if (timeout <= 0)
    {
      WiFi.disconnect();
      delay(500);
      WiFi.mode(WIFI_OFF);
      return false;
    }
  }
  return true;
}



void flushData()
{
  onLed(LED_BLUE);
  lastFlushAttemptAt = millis();

  if (!connectWifi())
  {
    blinkLed(LED_BLUE, 500);
    blinkLed(LED_RED, 250, 3);
    sleepStart();
    return;
  }

  int httpResponseCode;
  HTTPClient http;
  http.begin("http://roader.herokuapp.com/api/devices/flush");
  http.addHeader("Content-Type", "text/plain");
  
  String data;
  
  RfidScanEvent MessageEvent; 
  EEPROM.get(startAddr, MessageEvent);
  int currentAddr = 0;
  if (nextRfidScanEventIdx > 0)
  { 
    while( currentAddr <= lastAddr ) {
      EEPROM.get(currentAddr, MessageEvent);
      data += formhttpMessage(MessageEvent);
      currentAddr += sizeof(RfidScanEvent);
      Serial.println(String(MessageEvent.voltage) + String(MessageEvent.time));
    }
  }
  else
  {
    data = "Voltage Only: " + String(MessageEvent.voltage);
  }
    httpResponseCode = http.POST(data);
    http.end();
  WiFi.disconnect();
  delay(500);
  
  WiFi.mode(WIFI_OFF);
  
  delay(2000);
  offLed(LED_BLUE);
  RtcDateTime now = Rtc.GetDateTime();
  
  Serial.println(httpResponseCode);
  
  if (httpResponseCode == 200)
  {
    nextRfidScanEventIdx = 0;
    blinkLed(LED_GREEN, 250, 3);
  }
  else
  {
    blinkLed(LED_RED, 250, 3);
  }
  sleepStart();
}



void sleepStart()
{
  delay(600);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, 1);
  esp_light_sleep_start();
}



String formhttpMessage(const RfidScanEvent &rfe) 
{
  return String(rfe.uid[0]) + String(rfe.uid[1]) + String(rfe.uid[2]) + String(rfe.uid[3]) + "   " + String(rfe.time) + "   " + String(rfe.voltage) + "    " + String(rfe.checkSum);
}
