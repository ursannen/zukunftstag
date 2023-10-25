/*
Projekt:      Suessigkeitenautomat 
Autor:        Vorname Nachname
Datum:        09.11.2023
Steuerung:    ESP32C3 Dev Module
Beschreibung: Diese Software steuert den Suessigkeitenautomaten, welcher im Rahmen des Zukunftstages 2023 bei der Helbling Technik in Wil gebaut wurde.
              
              Zur Funktionalität:
              Der ESP32 baut ein WiFi-Netz mit der IP-Adresse 192.168.4.1. auf. Verbindet man sich mit dieser IP via einem Web-Browser, 
              kann man mit dem Suessigkeitenautomaten interagieren.
              
              Tipps für die Entwicklung:
              Die Webseite wurde mit einem Online HTML Editor (Link: https://www.tutorialspoint.com/online_html_editor.php) entwickelt und anschliessend 
              mittels eines HTML-Compress-Tool (Link: https://www.textfixer.com/html/compress-html-compression.php) zum String konvertiert. 
*/


// Libraries
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <WebSocketsServer.h>  // by Markus Sattler - search for "Arduino Websockets"
#include <ArduinoJson.h>       // by Benoit Blanchon


// Globale Variablen
const uint8_t Acceleration = 100;
const uint8_t SpeedLevel = 255;


// Typen-Definition
enum class LedColor {
  NO,
  RED,
  GREEN,
  BLUE,
  WHITE
};

enum class PusherPosition : bool {
  COLLECTING_POSITION = 0,
  DISPENSING_POSITION = 1
};


// Konfiguration der Pins
const uint8_t RandomSignalPin = 0;
const uint8_t MotorPin = 4;
const uint8_t LedPin = 8;
const uint8_t SwitchPin = 10;


// Konfiguration von RGB-LED
const uint8_t NumberOfLeds = 1;
Adafruit_NeoPixel rgbLed = Adafruit_NeoPixel(NumberOfLeds, LedPin);


// Konfiguration WiFi
const char* Ssid = "Suessigkeitenautomat_Name";
const char* Password = "";


// Konfiguration von Webserver und Websocket
const uint8_t WebServerPort = 80;
const uint8_t WebSocketPort = 81;
WebServer server(WebServerPort);
WebSocketsServer webSocket = WebSocketsServer(WebSocketPort);


// Konfiguration vom Payload
const uint8_t MaxJsonDocumentSize = 250;
StaticJsonDocument<MaxJsonDocumentSize> jsonDocument;
String webpage = "<p> &nbsp;</p><h1>Lisa's Suessigkeitenautomat</h1><table> <thead> <tr> <td> </td> </tr> </thead> <tbody> <tr> <td style='text-align: center;'> <p><strong><span id='sum1'>-</span></strong></p> </td> <td style='text-align: center;'> <p><strong>+</strong></p> </td> <td style='text-align: center;'> <p><strong><span id='sum2'>-</span></strong></p> </td> <td style='text-align: center;'> <p><strong>=</strong></p> </td> <td> <td><input type='number' name='lsgAdd' id='lsgAdd' max='9999' min='-9999' step='1'></td> </td> </tr> </tbody></table><p><button id='BTN_CHECK' type='button'>Check result </button></p><p><button id='BTN_SEND_BACK' type='button' hidden>Continue </button></p><script> var Socket; var obj; var CorrectAnswers; document.getElementById('BTN_CHECK').addEventListener('click', button_check_result); document.getElementById('BTN_SEND_BACK').addEventListener('click', button_send_back); function init() { Socket = new WebSocket('ws://' + window.location.hostname + ':81/'); Socket.onmessage = function(event) { processCommand(event); }; } function button_check_result() { CorrectAnswers = 0; document.getElementById('lsgAdd').setAttribute('disabled', ''); if (document.getElementById('lsgAdd').value == obj.LSGadd) { CorrectAnswers += 1; document.getElementById('lsgAdd').style.color = 'green'; } else { document.getElementById('lsgAdd').style.color = 'red'; } document.getElementById('BTN_CHECK').setAttribute('hidden', 'hidden'); document.getElementById('BTN_SEND_BACK').removeAttribute('hidden'); console.log(CorrectAnswers); var loesungen = { count: CorrectAnswers, }; console.log(JSON.stringify(loesungen)); Socket.send(JSON.stringify(loesungen)); } function button_send_back() { ResetCounter = 0; document.getElementById('lsgAdd').value = ''; document.getElementById('lsgAdd').style.color = 'black'; document.getElementById('BTN_CHECK').removeAttribute('hidden'); document.getElementById('BTN_SEND_BACK').setAttribute('hidden', 'hidden'); } function processCommand(event) { obj = JSON.parse(event.data); document.getElementById('sum1').innerHTML = obj.sum1; document.getElementById('sum2').innerHTML = obj.sum2; document.getElementById('lsgAdd').removeAttribute('disabled', ''); console.log(obj.sum1); console.log(obj.sum2); } window.onload = function(event) { init(); }</script></html>";


void setup() {
  initEsp();
  initWiFi();
  initWebserver();
}


void loop() {
  server.handleClient();
  webSocket.loop();
}


void initEsp() {
  Serial.begin(115200);
  unsigned long currentMillis = millis();
  randomSeed(analogRead(RandomSignalPin));

  pinMode(MotorPin, OUTPUT);
  pinMode(SwitchPin, INPUT_PULLDOWN);

  rgbLed.begin();
}


void initWiFi() {
  WiFi.softAP(Ssid, Password);
  delay(100);
  IPAddress IP = WiFi.softAPIP();
  Serial.println("AP IP address: " + IP);
}


void initWebserver() {
  server.on("/", []() {
    server.send(200, "text\html", webpage);
  });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}


void setColor(LedColor color) {
  switch (color) {
    case LedColor::NO:
      rgbLed.setPixelColor(0, 0, 0, 0);
    case LedColor::RED:
      rgbLed.setPixelColor(0, 250, 0, 0);
      break;
    case LedColor::GREEN:
      rgbLed.setPixelColor(0, 0, 250, 0);
      break;
    case LedColor::BLUE:
      rgbLed.setPixelColor(0, 0, 0, 250);
      break;
    case LedColor::WHITE:
      rgbLed.setPixelColor(0, 250, 250, 250);
      break;
  }
  rgbLed.show();
}


void webSocketEvent(byte num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("Client " + String(num) + " connected");
      updateWebContent();
      break;
    case WStype_DISCONNECTED:
      Serial.println("Client " + String(num) + " disconnected");
      break;
    case WStype_TEXT:
      Serial.println("Client " + String(num) + " received text");
      if (readWebContent(payload)) {
        processWebContent();
        updateWebContent();
      }
      break;
  }
}


void updateWebContent() {
  createWebContent();
  sendWebContent();
  setColor(LedColor::WHITE);
}


void createWebContent() {
  long sum1 = random(1, 100);
  long sum2 = random(1, 100);

  jsonDocument["sum1"] = sum1;
  jsonDocument["sum2"] = sum2;
  jsonDocument["LSGadd"] = sum1 + sum2;
}


void sendWebContent() {
  String jsonStringTx = "";
  serializeJson(jsonDocument, jsonStringTx);
  webSocket.broadcastTXT(jsonStringTx);
}


bool readWebContent(const uint8_t* payload) {
  DeserializationError error = deserializeJson(jsonDocument, payload);
  if (error) {
    Serial.println("Deserialization failed");
    return false;
  } else {
    return true;
  }
}


void processWebContent() {
  const int correctResults = jsonDocument["count"]; 
  if (correctResults > 0) {
    setColor(LedColor::GREEN);
    dispenseCandy(correctResults);
  } else {
    setColor(LedColor::RED); 
    delay(2000);
  }
}


void dispenseCandy(uint8_t quantity) {
  Serial.println("dispenseCandy");
  for (uint8_t q = 0; q < quantity; q++) {
    startMotor();
    while (getPusherPosition() != PusherPosition::DISPENSING_POSITION) {};
    while (getPusherPosition() != PusherPosition::COLLECTING_POSITION) {};
    stopMotor();
  }
}


void startMotor() {
  Serial.println("startMotor");
  uint8_t currentSpeed = 0;
  while (currentSpeed < SpeedLevel) {
    currentSpeed++;
    analogWrite(MotorPin, currentSpeed);    
    delay(1000 / Acceleration); 
  }
}


void stopMotor() {
  Serial.println("stopMotor");
  uint8_t currentSpeed = SpeedLevel;
  while (currentSpeed > 0) {
    currentSpeed--;
    analogWrite(MotorPin, currentSpeed);
    delay(1000 / Acceleration); 
  }
}


PusherPosition getPusherPosition() {
  const uint8_t DebouncingTime_ms = 100;
  bool oldState;
  bool newState;

  do {
    oldState = digitalRead(SwitchPin);
    delay(DebouncingTime_ms); 
    newState = digitalRead(SwitchPin);
  } while (oldState != newState);

  if (newState) {
    return PusherPosition::DISPENSING_POSITION;
  } else {
    return PusherPosition::COLLECTING_POSITION;
  }
}
