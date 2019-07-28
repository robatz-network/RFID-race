#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>

#define   ARRAYSIZE   200
#define   buttonPin   13 
#define   RST_PIN     22 
#define   SS_PIN      21

#define   LED_Red     4
#define   LED_Green   17
#define   LED_Blue    5
 
#define   ssid        "cyberspot"
#define   password    "cyberspot19"

int httpResponseCode;
String localuid; 
String message;
int Number_of_RFID_marks = 0;
unsigned long Last_send_time;

struct result{
  String uid;
  unsigned long Time;
};

struct result raceInfo[ARRAYSIZE] ;

MFRC522 mfrc522(SS_PIN, RST_PIN);

bool Connected_to_WiFi();
bool Send_information();
void Check_RFID_scaner();
String Race_info_message();


void setup() {
 
    Serial.begin(115200);
    SPI.begin(); 
    WiFi.mode(WIFI_STA);
    pinMode (LED_Red, OUTPUT);
    pinMode (LED_Green, OUTPUT);
    pinMode (LED_Blue, OUTPUT);
    digitalWrite(LED_Red, HIGH);
    digitalWrite(LED_Green, HIGH);
    digitalWrite(LED_Blue, HIGH); 
    pinMode(buttonPin,INPUT_PULLUP);
  
    mfrc522.PCD_Init(); 
    mfrc522.PCD_DumpVersionToSerial(); 

    Last_send_time = millis();
}
 
void loop() {
 Check_RFID_scaner();
 if ( (millis() - Last_send_time >= 27000) && (Connected_to_WiFi()) ) {
   Send_information();   
 }
}

void Check_RFID_scaner(){

  if ( ! mfrc522.PICC_IsNewCardPresent()) {
        return;
      }
    if ( ! mfrc522.PICC_ReadCardSerial()) {
      return;
    }
 
    String uidString = String(mfrc522.uid.uidByte[0])+String(mfrc522.uid.uidByte[1])+String(mfrc522.uid.uidByte[2])+String(mfrc522.uid.uidByte[3]); //Create String with UID
    
    digitalWrite(LED_Green, LOW);
    Serial.println("Detected:");
    Serial.println(uidString);
    
    if ( !(localuid == uidString) ){
      raceInfo[Number_of_RFID_marks].uid = uidString;
      raceInfo[Number_of_RFID_marks].Time = millis();
      localuid = uidString;
      Number_of_RFID_marks++;
    }  
    Serial.println(Number_of_RFID_marks);
    delay(100);
    digitalWrite(LED_Green, HIGH);
}


bool Connected_to_WiFi(){
  WiFi.begin(ssid, password);
  delay(3000);
  Serial.print(" Wifi status " ); Serial.println(WiFi.status());
  Last_send_time = millis();
  if (WiFi.status() != WL_CONNECTED) { 
    Serial.println("WiFi didn't connect");
    return false;
  }
  else 
    return true;
}

bool Send_information() {


   HTTPClient http;   
   http.begin("http://roader.herokuapp.com/api/devices/flush");  
   http.addHeader("Content-Type", "text/plain"); 
   Race_info_message();            
   Serial.println(message);
   
   if (Number_of_RFID_marks > 0) {
    
    digitalWrite(LED_Blue, LOW);
    httpResponseCode = http.POST(message);  
    Number_of_RFID_marks = 0;
       
    if(httpResponseCode>0) {
      String response = http.getString();                       
      Serial.print("http Response Code");Serial.print(httpResponseCode); Serial.println(response);           
      http.end(); 
      WiFi.disconnect();
      Serial.println("WiFi.disconnect()");
      message = "";
      digitalWrite(LED_Blue, HIGH); 
      return true;
    }
    
    else {
      Serial.print("Error on sending POST: ");Serial.println(httpResponseCode);
      http.end();  
      WiFi.disconnect();
      digitalWrite(LED_Blue, HIGH);
      digitalWrite(LED_Red, LOW);
      delay(1000);
      digitalWrite(LED_Red, HIGH);
      return false;
    }
   }
} 

String Race_info_message() {

  for ( int j = 0; j < Number_of_RFID_marks; j++ ) {   //Make message and send it to ESP32 with server on board
    message += WiFi.macAddress();
    message += "\t";
    message += raceInfo[j].uid;
    message += "\t";
    message += raceInfo[j].Time;
    message += "\n";
    raceInfo[j].uid = ""; //Clear the array of raceInfo
    raceInfo[j].Time = 0;
  }
  return("pp");
}
