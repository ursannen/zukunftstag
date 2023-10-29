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
#include <map>

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


enum class DifficultyLevel {
  EASY,
  MEDIUM,
  HARD
};


// Konfiguration der Aufgabe
DifficultyLevel difficultyLevel = DifficultyLevel::EASY;
std::map<DifficultyLevel, int16_t> numberLeverage{
  { DifficultyLevel::EASY, 1 },
  { DifficultyLevel::MEDIUM, 2 },
  { DifficultyLevel::HARD, 4 }
};


// Konfiguration des Antriebs
const uint8_t Acceleration = 700;
const uint8_t SpeedLevel = 255;


// Konfiguration der Pins
const uint8_t RandomSignalPin = 0;
const uint8_t MotorPin = 4;
const uint8_t LedPin = 8;
const uint8_t SwitchPin = 18;


// Konfiguration von RGB-LED
const uint8_t NumberOfLeds = 1;
Adafruit_NeoPixel rgbLed = Adafruit_NeoPixel(NumberOfLeds, LedPin);


// Konfiguration WiFi
const char* Ssid = "Suessigkeiten-Automat_Name";
const char* Password = "";


// Konfiguration von Webserver und Websocket
const uint8_t WebServerPort = 80;
const uint8_t WebSocketPort = 81;
WebServer server(WebServerPort);
WebSocketsServer webSocket = WebSocketsServer(WebSocketPort);


// Konfiguration vom Payload
const uint8_t MaxJsonDocumentSize = 250;
StaticJsonDocument<MaxJsonDocumentSize> jsonDocument;
String webpage = "<!DOCTYPE html><html style='text-align:center'><img src=data:image/png;base64,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 /><h3>Gutes gegen Richtiges</h3><p> <button id='EASY_BUTTON' type='button' class='button EASY_BUTTON'>leicht </button> <button id='MEDIUM_BUTTON' type='button' class='button MEDIUM_BUTTON'>mittel </button> <button id='HARD_BUTTON' type='button' class='button HARD_BUTTON'>schwierig </button></p><p> <strong><span id='number1'>-</span></strong> <strong><span id='operand'>-</span></strong> <strong><span id='number2'>-</span></strong> <strong>=</strong></p><p><input type='number' name='solution' id='solution' max='9999' min='0' step='1'></p><p><button id='BTN_CHECK' type='button'>Check result </button></p><p><button id='BTN_SEND_BACK' type='button' hidden>Continue </button></p><style> .button { border: none; color: white; padding: 10px 10px; width: 100px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 4px 2px; cursor: pointer; border-radius: 2px; } .EASY_BUTTON { background-color: #4CAF50; } .MEDIUM_BUTTON { background-color: #FFA500; } .HARD_BUTTON { background-color: #C71585; }</style><script> var Socket; var obj; var CorrectAnswers; document.getElementById('BTN_CHECK').addEventListener('click', button_check_result); document.getElementById('BTN_SEND_BACK').addEventListener('click', button_send_back); function init() { Socket = new WebSocket('ws://' + window.location.hostname + ':81/'); Socket.onmessage = function(event) { processCommand(event); }; } function button_check_result() { CorrectAnswers = 0; document.getElementById('solution').setAttribute('disabled', ''); if (document.getElementById('solution').value == obj.solution) { CorrectAnswers += 1; document.getElementById('solution').style.color = 'green'; } else { document.getElementById('solution').style.color = 'red'; } document.getElementById('BTN_CHECK').setAttribute('hidden', 'hidden'); document.getElementById('BTN_SEND_BACK').removeAttribute('hidden'); console.log(CorrectAnswers); var loesungen = { count: CorrectAnswers, }; console.log(JSON.stringify(loesungen)); Socket.send(JSON.stringify(loesungen)); } function button_send_back() { ResetCounter = 0; document.getElementById('solution').value = ''; document.getElementById('solution').style.color = 'black'; document.getElementById('BTN_CHECK').removeAttribute('hidden'); document.getElementById('BTN_SEND_BACK').setAttribute('hidden', 'hidden'); } function processCommand(event) { obj = JSON.parse(event.data); document.getElementById('number1').innerHTML = obj.number1; document.getElementById('number2').innerHTML = obj.number2; document.getElementById('operand').innerHTML = obj.operand; document.getElementById('solution').removeAttribute('disabled', ''); console.log(obj.number1); console.log(obj.number2); } window.onload = function(event) { init(); }</script></html>";



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
  pinMode(SwitchPin, INPUT_PULLUP);

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
      rgbLed.setPixelColor(0, 127, 0, 0);
      break;
    case LedColor::GREEN:
      rgbLed.setPixelColor(0, 0, 127, 0);
      break;
    case LedColor::BLUE:
      rgbLed.setPixelColor(0, 0, 0, 127);
      break;
    case LedColor::WHITE:
      rgbLed.setPixelColor(0, 50, 50, 50);
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
  String operand; 
  int16_t number1, number2, solution;
  switch (random(0, 4)) {
    case 0:
      operand = "+";
      number1 = random(10, 50 * numberLeverage[difficultyLevel]);
      number2 = random(10, 50 * numberLeverage[difficultyLevel]);
      solution = number1 + number2;
      break;
    case 1:
      operand = "-";
      number1 = random(10, 50 * numberLeverage[difficultyLevel]);
      number2 = random(10, 50 * numberLeverage[difficultyLevel]);
      if(number1 < number2)
      {
        int16_t temp = number1;
        number1 = number2;
        number2 = temp;
      }
      solution = number1 - number2;
      break;
    case 2:
      operand = "*";
      number1 = random(2 * numberLeverage[difficultyLevel], 10 * numberLeverage[difficultyLevel]);
      number2 = random(2, 13);
      solution = number1 * number2;
      break;
    case 3:
      operand = "/";
      solution = random(2 * numberLeverage[difficultyLevel], 10 * numberLeverage[difficultyLevel]);
      number2 = random(2, 13);
      number1 = solution * number2;
      break;
  }
  jsonDocument["operand"] = operand;
  jsonDocument["number1"] = number1;
  jsonDocument["number2"] = number2;
  jsonDocument["solution"] = solution;
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
    return PusherPosition::COLLECTING_POSITION;
  } else {
    return PusherPosition::DISPENSING_POSITION;
  }
}
