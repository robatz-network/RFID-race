#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <TaskScheduler.h>

#define ARRAYSIZE 200
#define RST_PIN 22
#define SS_PIN 21

#define LED_Red 16
#define LED_Green 17
#define LED_Blue 5

#define Voltage_Pin 4

#define ssid "Room89"
#define password "room8989"

Scheduler rfidScheduler;
MFRC522 mfrc522(SS_PIN, RST_PIN);

struct result {
    String uid;
    unsigned long Time;
};

struct result raceInfo[ARRAYSIZE];

int httpResponseCode;
int Number_of_RFID_marks = 0;

String localuid;
String message;

bool Connected_to_WiFi();
void Send_information(bool Full_clear, bool Send_last_only);
void Check_RFID_scaner();
String Race_info_message();
float Get_Battery_Voltage();

void decor_1()
{
    Send_information(false, true);
}

void decor_2()
{
    Send_information(true, false);
}

Task taskCheckRFID(TASK_SECOND * 0.10, TASK_FOREVER, &Check_RFID_scaner);
Task taskShortSend(TASK_SECOND * 5, TASK_FOREVER, &decor_1);
Task taskGlobalSend(TASK_SECOND * 27, TASK_FOREVER, &decor_2);

void setup()
{

    SPI.begin();
    Serial.begin(115200);
    mfrc522.PCD_Init();
    mfrc522.PCD_DumpVersionToSerial();

    rfidScheduler.addTask(taskCheckRFID);
    rfidScheduler.addTask(taskShortSend);
    rfidScheduler.addTask(taskGlobalSend);

    taskGlobalSend.enable();
    taskShortSend.enable();
    taskCheckRFID.enable();

    WiFi.mode(WIFI_STA);

    pinMode(LED_Red, INPUT_PULLUP);
    pinMode(LED_Green, INPUT_PULLUP);
    pinMode(LED_Blue, INPUT_PULLUP);
    pinMode(Voltage_Pin, INPUT);
}

void loop()
{
    rfidScheduler.execute();
}

void Check_RFID_scaner()
{

    if (!mfrc522.PICC_IsNewCardPresent()) {
        return;
    }
    if (!mfrc522.PICC_ReadCardSerial()) {
        return;
    }
    String uidString;

    for (int k = 0; k < 4; k++) {
        uidString += String(mfrc522.uid.uidByte[k]); // Create String with UID
    }
    pinMode(LED_Green, OUTPUT);
    digitalWrite(LED_Green, LOW);
    Serial.println("Detected:");
    Serial.println(uidString);

    if (!(localuid == uidString)) {
        raceInfo[Number_of_RFID_marks].uid = uidString;
        raceInfo[Number_of_RFID_marks].Time = millis();
        localuid = uidString;
        Number_of_RFID_marks++;
    }
    Serial.println(Number_of_RFID_marks);
    delay(100);
    digitalWrite(LED_Green, HIGH);
    pinMode(LED_Green, INPUT_PULLUP);
}

bool Connected_to_WiFi()
{
    WiFi.begin(ssid, password);
    delay(1250);
    Serial.print(" Wifi status ");
    Serial.println(WiFi.status());
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi didn't connect");
        return false;
    } else
        return true;
}

void Send_information(bool Full_clear, bool Send_last_only)
{
    if (!Connected_to_WiFi()) {
        return;
    }

    HTTPClient http;
    http.begin("http://roader.herokuapp.com/api/devices/flush");
    http.addHeader("Content-Type", "text/plain");
    if (!Send_last_only) {
        Serial.println("Sendling all list message");
        message += Race_info_message(!Send_last_only);
        Serial.print(message);
        httpResponseCode = http.POST(message);
        message = "";
    } else {
        if (Number_of_RFID_marks > 0) {
            message += Race_info_message(!Send_last_only);
            Serial.print(message);
            httpResponseCode = http.POST(message);
            message = "";
        } else
            return;
    }
    if (Number_of_RFID_marks > 0) {
        pinMode(LED_Blue, OUTPUT);
        digitalWrite(LED_Blue, LOW);

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.print("http Response Code");
            Serial.print(httpResponseCode);
            Serial.println(response);
            http.end();
            WiFi.disconnect();
            Serial.println("WiFi.disconnect()\n");
            if (Full_clear) {
                for (int j = 0; j < Number_of_RFID_marks; j++) {
                    raceInfo[j].uid = ""; // Clear the array of raceInfo
                    raceInfo[j].Time = 0;
                }
                Number_of_RFID_marks = 0;
                Serial.println("Full Clear was end");
            }
            delay(150);
            digitalWrite(LED_Blue, HIGH);
            pinMode(LED_Blue, INPUT_PULLUP);
            return;
        }

        else {
            Serial.print("Error on sending POST: ");
            Serial.println(httpResponseCode);
            http.end();
            WiFi.disconnect();
            pinMode(LED_Red, OUTPUT);
            digitalWrite(LED_Red, LOW);
            delay(1000);
            digitalWrite(LED_Red, HIGH);
            pinMode(LED_Red, INPUT_PULLUP);
            return;
        }
    }
}

String Race_info_message(bool Send_All)
{
    String msg = "";
    if (Send_All) {
        msg += ("General post is consist of " + String(Number_of_RFID_marks) + " marks\n");
        for (int j = 0; j < Number_of_RFID_marks; j++) { // Make message and send it to ESP32 with server on board
            msg += WiFi.macAddress();
            msg += "\t";
            msg += raceInfo[j].uid;
            msg += "\t";
            msg += raceInfo[j].Time;
            msg += "\n";
        }
        msg += String(Get_Battery_Voltage());
        msg += "\n";
    } else {
        msg += "Last registered mark\n";
        msg += WiFi.macAddress();
        msg += "\t";
        msg += raceInfo[Number_of_RFID_marks - 1].uid;
        msg += "\t";
        msg += raceInfo[Number_of_RFID_marks - 1].Time;
        msg += "\n";
    }
    return (msg);
}

float Get_Battery_Voltage()
{
    int Val = 0;
    float Voltage_In = 0;
    Val = analogRead(Voltage_Pin); // "Analog" value of selected pin
    Voltage_In = (Val) / 1023;     // Conversion to voltage
    return Voltage_In;
}
