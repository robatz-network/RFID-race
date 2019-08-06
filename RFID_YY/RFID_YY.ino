#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <TaskScheduler.h>

#define RST_PIN 22
#define SS_PIN 21
#define VOLTAGE_PIN 4

#define LED_RED 16
#define LED_GREEN 17
#define LED_BLUE 5

#define WIFI_SSID "Room89"
#define WIFI_PASS "room8989"

Scheduler rfidScheduler;
MFRC522 mfrc522(SS_PIN, RST_PIN);

struct WaypointScan {
    String uid;
    unsigned long time;
};

struct WaypointScan waypoints[200];
int nextWaypointIdx = 0;
void checkRFIDScanner();
void flushData();
String formatWaypoint(const WaypointScan &wpt);
String formatBatteryVoltage(const uint16_t voltage);
void blinkLed(const uint8_t, const uint32_t);
uint16_t getBatteryVoltage();

Task taskCheckRFID(TASK_SECOND * 0.1, TASK_FOREVER, &checkRFIDScanner);
Task taskFlushData(TASK_SECOND * 30, TASK_FOREVER, &flushData);

void setup()
{
    SPI.begin();
    Serial.begin(115200);
    mfrc522.PCD_Init();
    mfrc522.PCD_DumpVersionToSerial();

    rfidScheduler.addTask(taskCheckRFID);
    rfidScheduler.addTask(taskFlushData);

    taskFlushData.enable();
    taskCheckRFID.enable();

    WiFi.mode(WIFI_STA);

    pinMode(LED_RED, INPUT_PULLUP);
    pinMode(LED_GREEN, INPUT_PULLUP);
    pinMode(LED_BLUE, INPUT_PULLUP);
    pinMode(VOLTAGE_PIN, INPUT);
}

void loop()
{
    rfidScheduler.execute();
}

void checkRFIDScanner()
{
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        return;
    }

    String uidString;

    for (int i = 0; i < 4; i++) {
        uidString += String(mfrc522.uid.uidByte[i]);
    }

    if (nextWaypointIdx == 0 || (nextWaypointIdx > 0 && waypoints[nextWaypointIdx - 1].uid != uidString)) {
        waypoints[nextWaypointIdx].uid = uidString;
        waypoints[nextWaypointIdx].time = millis();
        nextWaypointIdx++;
        flushData();
    }

    blinkLed(LED_GREEN, 100);
}

bool connectWifi()
{
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int counter = 0;

    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        counter++;
        if (counter >= 8) {
            WiFi.disconnect();
            return false;
        }
    }

    return true;
}

void flushData()
{
    if (!connectWifi()) {
        return;
    }

    int httpResponseCode;
    HTTPClient http;
    http.begin("http://roader.herokuapp.com/api/devices/flush");
    http.addHeader("Content-Type", "text/plain");
    String data;

    for (int i = 0; i < nextWaypointIdx; i++) {
        data += formatWaypoint(waypoints[i]) + "\n";
    }

    data += formatBatteryVoltage(getBatteryVoltage()) + "\n";
    httpResponseCode = http.POST(data);
    http.end();
    WiFi.disconnect();

    if (httpResponseCode == 200) {
        nextWaypointIdx = 0;
        blinkLed(LED_BLUE, 150);
    } else {
        blinkLed(LED_RED, 1000);
    }
}

uint16_t getBatteryVoltage()
{
    return analogRead(VOLTAGE_PIN);
}

void blinkLed(const uint8_t pin, const uint32_t ms)
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delay(ms);
    digitalWrite(pin, HIGH);
    pinMode(pin, INPUT_PULLUP);
}

String formatWaypoint(const WaypointScan &wpt)
{
    return "rfid_scanned\t" + WiFi.macAddress() + "\t" + wpt.uid + "\t" + wpt.time;
}

String formatBatteryVoltage(const uint16_t voltage)
{
    return "voltage\t" + WiFi.macAddress() + "\t" + voltage + "\t" + millis();
}
