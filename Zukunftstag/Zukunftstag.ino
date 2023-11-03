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
#include <string.h>

// Typen-Definition
enum class LedColor {
  NO,
  RED,
  GREEN,
  BLUE,
  WHITE
};


enum class PusherPosition {
  LEFT,
  CENTER,
  RIGHT
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
const uint16_t Acceleration_BitsPerSecond = 5000;
const uint8_t SpeedLevel = 255;
const uint8_t MaxRuntime_s = 60;


// Konfiguration der Pins
const uint8_t RandomSignalPin = 0;
const uint8_t MotorPin = 4;
const uint8_t LedPin = 8;
const uint8_t SwitchRightPin = 18;
const uint8_t SwitchLeftPin = 19;


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
String webpage = "<!DOCTYPE html><html style='text-align: center;'><head> <title>Zukunftstag Helbling 2023</title> <style> body { font-family: Corbel; min-height: 100vh; margin: 0; } .LARGE_FONT { font-size: 40px; font-family: Corbel; } .CUSTOM_BUTTON { border: none; color: white; padding: 15px 15px; width: 15%; text-align: center; text-decoration: none; font-family: Corbel; display: inline-block; font-size: 16px; margin: 0px 0px; cursor: pointer; border-radius: 8px; } .USER_BUTTON { background-color: #009900; width: 20%; padding: 30px 20px; font-size: 40px; } .CUSTOM_INPUT { border-radius: 8px; border: 5px solid lightgray; width: 45%; font-size: 40px; font-family: Corbel; text-align: center; } </style></head><br><br><br><body> <img src=data:image/png;base64,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 /> <h3>Viel Spass!</h3> <p> <button id='easyButtonId' type='button' class='CUSTOM_BUTTON EASY_BUTTON'>leicht</button> <button id='mediumButtonId' type='button' class='CUSTOM_BUTTON MEDIUM_BUTTON'>mittel</button> <button id='hardButtonId' type='button' class='CUSTOM_BUTTON HARD_BUTTON'>schwierig</button> </p> <p class='LARGE_FONT'> <strong> <span id='number1Id'>-</span> <span id='operandId'>-</span> <span id='number2Id'>-</span> = </strong> </p> <p> <input id='solutionInputId' type='number' class='CUSTOM_INPUT' max='9999' min='0' step='1' /> <br/> <br/> <button id='userButtonId' type='button' class='CUSTOM_BUTTON USER_BUTTON'> <span id='userButtonContent'></span> </button> </p> <script> var Socket; var obj; var state = 'INIT'; var difficultyLevel; document.getElementById('userButtonId').addEventListener('click', processStateMachine); document.getElementById('easyButtonId').addEventListener('click', setEasyDifficulty); document.getElementById('mediumButtonId').addEventListener('click', setMediumDifficulty); document.getElementById('hardButtonId').addEventListener('click', setHardDifficulty); window.onload = function(event) { init(); }; function init() { Socket = new WebSocket('ws://' + window.location.hostname + ':81/'); Socket.onopen = function(event) { processStateMachine(); }; Socket.onmessage = function(event) { processMessage(event); }; } function processStateMachine() { if (state == 'INIT') { setEasyDifficulty(); getNewExcercise(); state = 'READY_FOR_SOLUTION' } else if (state == 'READY_FOR_NEW_EXERCISE') { getNewExcercise(); state = 'READY_FOR_SOLUTION' } else if (state == 'READY_FOR_SOLUTION') { checkResult(); showNewExerciseButton(); state = 'READY_FOR_NEW_EXERCISE' } } function getNewExcercise() { document.getElementById('userButtonId').innerHTML = '&#10148'; document.getElementById('solutionInputId').value = ''; document.getElementById('solutionInputId').style.color = 'black'; sendData('getNewExcercise', difficultyLevel); } function processMessage(event) { obj = JSON.parse(event.data); document.getElementById('number1Id').innerHTML = obj.number1; document.getElementById('number2Id').innerHTML = obj.number2; document.getElementById('operandId').innerHTML = obj.operand; document.getElementById('solutionInputId').removeAttribute('disabled', ''); console.log(obj.number1); console.log(obj.operand); console.log(obj.number2); } function checkResult() { document.getElementById('solutionInputId').setAttribute('disabled', ''); if (document.getElementById('solutionInputId').value == obj.solution) { sendData('result', 'correct'); document.getElementById('solutionInputId').style.color = 'green'; } else { sendData('result', 'wrong'); document.getElementById('solutionInputId').style.color = 'red'; } } function showNewExerciseButton() { document.getElementById('userButtonId').innerHTML = '&#x21bb'; } function sendData(key, value) { console.log(key); console.log(value); var message = { key: key, value: value }; Socket.send(JSON.stringify(message)); } function setEasyDifficulty() { difficultyLevel = 'easy'; document.getElementById('easyButtonId').style.background = '#0073e6'; document.getElementById('mediumButtonId').style.background = '#99ccff'; document.getElementById('hardButtonId').style.background = '#99ccff'; } function setMediumDifficulty() { difficultyLevel = 'medium'; document.getElementById('easyButtonId').style.background = '#99ccff'; document.getElementById('mediumButtonId').style.background = '#0073e6'; document.getElementById('hardButtonId').style.background = '#99ccff'; } function setHardDifficulty() { difficultyLevel = 'hard'; document.getElementById('easyButtonId').style.background = '#99ccff'; document.getElementById('mediumButtonId').style.background = '#99ccff'; document.getElementById('hardButtonId').style.background = '#0073e6'; } </script></body></html>";


void setup() {
  initEsp();
  initWiFi();
  initWebserver();
}


void loop() {
  //server.handleClient();
  if (Serial.available() > 0) {
    int count = Serial.parseInt();
    if (count > 0)
      dispenseCandy(count);
  }
  //webSocket.loop();
}


void initEsp() {
  Serial.begin(115200);
  unsigned long currentMillis = millis();
  randomSeed(analogRead(RandomSignalPin));

  pinMode(MotorPin, OUTPUT);
  pinMode(SwitchRightPin, INPUT_PULLUP);
  pinMode(SwitchLeftPin, INPUT_PULLUP);

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
      break;
    case WStype_DISCONNECTED:
      Serial.println("Client " + String(num) + " disconnected");
      break;
    case WStype_TEXT:
      Serial.println("Client " + String(num) + " received text");
      if (readWebContent(payload)) {
        processWebContent();
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
      number1 = random(20, 50 * numberLeverage[difficultyLevel]);
      number2 = random(10, 50 * numberLeverage[difficultyLevel]);
      solution = number1 + number2;
      break;
    case 1:
      operand = "-";
      number1 = random(20, 50 * numberLeverage[difficultyLevel]);
      number2 = random(10, 50 * numberLeverage[difficultyLevel]);
      if (number1 < number2) {
        int16_t temp = number1;
        number1 = number2;
        number2 = temp;
      }
      solution = number1 - number2;
      break;
    case 2:
      operand = "*";
      number1 = random(2 * numberLeverage[difficultyLevel], 10 * numberLeverage[difficultyLevel]);
      number2 = random(1 + numberLeverage[difficultyLevel], 13);
      solution = number1 * number2;
      break;
    case 3:
      operand = "/";
      solution = random(2 * numberLeverage[difficultyLevel], 10 * numberLeverage[difficultyLevel]);
      number2 = random(1 + numberLeverage[difficultyLevel], 13);
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
  String key = jsonDocument["key"];
  String value = jsonDocument["value"];
  if (key == "result") {
    processResult(value);
  }
  if (key == "getNewExcercise") {
    processDifficulty(value);
    updateWebContent();
  }
}


void processResult(String result) {
  if (result == "correct") {
    setColor(LedColor::GREEN);
    dispenseCandy(1);
  } else {
    setColor(LedColor::RED);
    delay(2000);
  }
}


void processDifficulty(String value) {
  if (value == "easy") {
    difficultyLevel = DifficultyLevel::EASY;
  }
  if (value == "medium") {
    difficultyLevel = DifficultyLevel::MEDIUM;
  }
  if (value == "hard") {
    difficultyLevel = DifficultyLevel::HARD;
  }
}


void dispenseCandy(uint8_t quantity) {
  Serial.println("dispenseCandy");
  startMotor();
  for (uint8_t q = 0; q < quantity; q++) {
    waitUntilCandyIsDispensed();
  }
  stopMotor();
  Serial.println();
}

void waitUntilCandyIsDispensed() {
  unsigned long startTime = millis();
  unsigned long MaxRuntime_ms = 1000 * static_cast<unsigned long>(MaxRuntime_s);
  Serial.println("go to center");
  while (millis() - startTime < MaxRuntime_ms && getPusherPosition() != PusherPosition::CENTER) {};
  Serial.println("go to left or right position");
  while (millis() - startTime < MaxRuntime_ms && getPusherPosition() == PusherPosition::CENTER) {};
  // Serial.println("go to center");
  // while (millis() - startTime < MaxRuntime_ms && getPusherPosition() != PusherPosition::CENTER) {};
  // PusherPosition currentPusherPosition = getPusherPosition();
  // Serial.println("!= currentPusherPosition");
  // while (millis() - startTime < MaxRuntime_ms && getPusherPosition() != currentPusherPosition) {};
}

void startMotor() {
  Serial.println("startMotor");
  uint8_t currentSpeed = 0;
  while (currentSpeed < SpeedLevel) {
    currentSpeed++;
    analogWrite(MotorPin, currentSpeed);
    delayMicroseconds(1000000 / Acceleration_BitsPerSecond);
  }
}


void stopMotor() {
  Serial.println("stopMotor");
  uint8_t currentSpeed = SpeedLevel;
  while (currentSpeed > 0) {
    currentSpeed--;
    analogWrite(MotorPin, currentSpeed);
    delayMicroseconds(1000000 / Acceleration_BitsPerSecond);
  }
}


PusherPosition getPusherPosition() {
  if (getDebouncedPinState(SwitchLeftPin) == 0) {
    return PusherPosition::LEFT;
  } else if (getDebouncedPinState(SwitchRightPin) == 0) {
    return PusherPosition::RIGHT;
  } else {
    return PusherPosition::CENTER;
  }
}


bool getDebouncedPinState(const uint8_t pin) {
  const uint8_t DebouncingTime_ms = 10;
  bool oldState;
  bool newState;
  do {
    oldState = digitalRead(pin);
    delay(DebouncingTime_ms);
    newState = digitalRead(pin);
  } while (oldState != newState);
  return newState;
}
