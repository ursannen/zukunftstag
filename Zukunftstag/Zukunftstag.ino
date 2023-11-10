/*
Firma:        Helbling Technik Wil AG
Projekt:      Suessigkeitenautomat 
Autor:        Urs Annen
Datum:        09.11.2023
Controller:   ESP32C3 Dev Module
Beschreibung: Diese Software steuert den Suessigkeitenautomaten, welcher im Rahmen des Zukunftstages 2023 bei der Helbling Technik in Wil gebaut wird.
              
              Zur Funktionalität:
              Der ESP32 baut ein WiFi-Netzwerk mit der IP-Adresse 192.168.4.1. auf. Verbindet man sich mit dieser IP via einem Web-Browser, 
              kann man mit dem Suessigkeitenautomaten interagieren.
              
              Tipps für die Entwicklung:
              Die Webseite wurde mit einem Online HTML Editor (Link: https://www.tutorialspoint.com/online_html_editor.php) entwickelt und anschliessend 
              mittels eines HTML-Compress-Tools (Link: https://www.textfixer.com/html/compress-html-compression.php) zum String konvertiert. 
*/


// Libraries
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <WebSocketsServer.h>  // by Markus Sattler - search for "Arduino Websockets"
#include <ArduinoJson.h>       // by Benoit Blanchon
#include <map>
#include <String.h>


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


struct Range {
  uint16_t min;
  uint16_t max;
};


struct User {
  uint8_t id;
  uint16_t numberOfExercises = 0;
  uint16_t numberOfCorrects = 0;
  long timeStampOfLiveSignal = 0;
  bool operator==(const User& u) const {
    return id == u.id;
  }
  bool operator!=(const User& u) const {
    return id != u.id;
  }
};


// Konfiguration der Anwendung
const int16_t SecretNumber = 1234;
const uint8_t MaxNumberOfUsers = 4;
const uint16_t TimeOfResultStatusNotification_ms = 1500;


// Konfiguration der Users
std::vector<User> users;


// Konfiguration der Aufgabe
String operand;
int16_t number1, number2;
int16_t solution;
DifficultyLevel difficultyLevel;

std::map<DifficultyLevel, String> difficultyLevelIds{
  { DifficultyLevel::EASY, "easy" },
  { DifficultyLevel::MEDIUM, "medium" },
  { DifficultyLevel::HARD, "hard" }
};

std::map<DifficultyLevel, Range> rangeAddition{
  { DifficultyLevel::EASY, { 5, 25 } },
  { DifficultyLevel::MEDIUM, { 50, 100 } },
  { DifficultyLevel::HARD, { 100, 300 } }
};

std::map<DifficultyLevel, Range> rangeMultiplicationNumber1{
  { DifficultyLevel::EASY, { 2, 11 } },
  { DifficultyLevel::MEDIUM, { 5, 11 } },
  { DifficultyLevel::HARD, { 7, 13 } }
};

std::map<DifficultyLevel, Range> rangeMultiplicationNumber2{
  { DifficultyLevel::EASY, { 5, 11 } },
  { DifficultyLevel::MEDIUM, { 11, 20 } },
  { DifficultyLevel::HARD, { 13, 21 } }
};

std::map<DifficultyLevel, uint8_t> numberCandyPerDifficultyLevel{
  { DifficultyLevel::EASY, 1 },
  { DifficultyLevel::MEDIUM, 2 },
  { DifficultyLevel::HARD, 3 }
};


// Konfiguration des Antriebs
const uint16_t Acceleration_BitsPerSecond = 2500;
const uint8_t SpeedLevel = 255;
const uint8_t MaxRuntimePerSingleDispense_s = 3;


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
const char* Ssid = "Suessigkeitenautomat_Name";
const char* Password = "";
const bool IsSsidHidden = false;


// Konfiguration von Webserver und Websocket
const uint8_t WebServerPort = 80;
const uint8_t WebSocketPort = 81;
WebServer server(WebServerPort);
WebSocketsServer webSocket = WebSocketsServer(WebSocketPort);
const uint16_t MaxInresponsiveTime_ms = 7500 + 1000 * MaxRuntimePerSingleDispense_s;
const long ResponsivityCheckInterval_ms = 500;
long LastResponsivityCheck_ms = 0;


// Konfiguration der Web-Applikation
const uint8_t MaxJsonPayloadSize = 250;
StaticJsonDocument<MaxJsonPayloadSize> jsonPayload;
String webpage = "<!DOCTYPE html><html style='text-align: center;'><head> <title>Zukunftstag Helbling 2023</title> <style> body { font-family: Corbel; min-height: 100vh; margin: 0; } .STATISTICS_TEXT { font-size: 30px; font-family: Corbel; } .DIFFICULTY_BUTTON { color: white; border: none; text-align: center; text-decoration: none; display: inline-block; cursor: pointer; font-family: Corbel; font-size: 30px; margin: 0px 0px; border-radius: 8px; width: 25%; padding: 40px 0px; } .EXERCISE_TEXT { font-size: 75px; font-family: Corbel; font-weight: bold; } .RESULT_INPUT { font-size: 75px; font-family: Corbel; font-weight: bold; border-radius: 8px; border: 5px solid lightgray; width: 75%; padding: 30px 0px; text-align: center; } .USER_BUTTON { color: white; border: none; text-align: center; text-decoration: none; display: inline-block; cursor: pointer; font-family: Corbel; font-size: 50px; margin: 0px 0px; border-radius: 8px; width: 50%; padding: 50px 0px; } </style></head><br><br><br><body> <img src=data:image/png;base64,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 /> <br><br> <p class='STATISTICS_TEXT'>Du hast <strong> <span id='numberOfCorrectsId'>0</span> von <span id='numberOfExercisesId'>0</span></strong> Aufgaben richtig gel&ouml;st.</p> <p> <button id='easyButtonId' type='button' class='DIFFICULTY_BUTTON'>leicht</button> <button id='mediumButtonId' type='button' class='DIFFICULTY_BUTTON'>mittel</button> <button id='hardButtonId' type='button' class='DIFFICULTY_BUTTON'>schwierig</button> </p> <p class='EXERCISE_TEXT'><span id='number1Id'>-</span> <span id='operandId'>-</span> <span id='number2Id'>-</span> = </p> <p> <input id='solutionInputId' type='number' class='RESULT_INPUT' max='9999' min='0' step='1' /> <br/> <br/> <button id='userButtonId' type='button' class='USER_BUTTON'> <span id='userButtonContent'></span> </button> </p> <script> var Socket; var difficultyLevel; var brightColor = '#99ccff'; var darkColor = '#0073e6'; document.getElementById('userButtonId').addEventListener('click', processUserButtonClick); document.getElementById('easyButtonId').addEventListener('click', setEasyDifficulty); document.getElementById('mediumButtonId').addEventListener('click', setMediumDifficulty); document.getElementById('hardButtonId').addEventListener('click', setHardDifficulty); const intervalID = setInterval(sendLiveSignal, 100); window.onload = function(event) { init(); }; function init() { Socket = new WebSocket('ws://' + window.location.hostname + ':81/'); Socket.onopen = function(event) { showInputMask(); }; Socket.onmessage = function(event) { processMessage(event); }; } function processMessage(event) { obj = JSON.parse(event.data); if (obj.payloadId == 'setDifficultyLevel') { difficultyLevel = obj.param1; showDifficultyLevel(); } if (obj.payloadId == 'setExercise') { showInputMask(); showExercise(obj.param1, obj.param2, obj.param3); } if (obj.payloadId == 'setResultStatus') { showWaitingMask(); showResultStatus(obj.param1, obj.param2); } if (obj.payloadId == 'setStatistics') { showStatistics(obj.param1, obj.param2); } } function processUserButtonClick() { sendResult(); } function setEasyDifficulty() { if (difficultyLevel != 'easy') { difficultyLevel = 'easy'; sendDifficultyLevel(); } } function setMediumDifficulty() { if (difficultyLevel != 'medium') { difficultyLevel = 'medium'; sendDifficultyLevel(); } } function setHardDifficulty() { if (difficultyLevel != 'hard') { difficultyLevel = 'hard'; sendDifficultyLevel(); } } function sendLiveSignal() { sendPayload('liveSignal'); } function sendDifficultyLevel() { sendPayload('setDifficultyLevel', difficultyLevel); } function sendResult() { result = document.getElementById('solutionInputId').value; sendPayload('processResult', result); } function sendPayload(payloadId, param1 = '', param2 = '', param3 = '', param4 = '') { var message = { payloadId: payloadId, param1: param1, param2: param2, param3: param3, param4: param4 }; Socket.send(JSON.stringify(message)); } function showStatistics(numberOfExercisesId, numberOfCorrects) { document.getElementById('numberOfExercisesId').innerHTML = numberOfExercisesId; document.getElementById('numberOfCorrectsId').innerHTML = numberOfCorrects; } function showDifficultyLevel() { if (difficultyLevel == 'easy') { document.getElementById('easyButtonId').style.background = darkColor; document.getElementById('mediumButtonId').style.background = brightColor; document.getElementById('hardButtonId').style.background = brightColor; } if (difficultyLevel == 'medium') { document.getElementById('easyButtonId').style.background = brightColor; document.getElementById('mediumButtonId').style.background = darkColor; document.getElementById('hardButtonId').style.background = brightColor; } if (difficultyLevel == 'hard') { document.getElementById('easyButtonId').style.background = brightColor; document.getElementById('mediumButtonId').style.background = brightColor; document.getElementById('hardButtonId').style.background = darkColor; } } function showExercise(operand, number1, number2) { document.getElementById('operandId').innerHTML = operand; document.getElementById('number1Id').innerHTML = number1; document.getElementById('number2Id').innerHTML = number2; document.getElementById('solutionInputId').removeAttribute('disabled', ''); } function showResultStatus(status, solution) { if (status == 'correct') { document.getElementById('solutionInputId').style.color = 'green'; document.getElementById('solutionInputId').style.background = '#F0FFF0'; } if (status == 'wrong') { document.getElementById('solutionInputId').style.color = 'red'; document.getElementById('solutionInputId').style.background = '#FFF0F0'; } if (status == 'tooLate') { document.getElementById('solutionInputId').value = solution; document.getElementById('solutionInputId').style.background = '#FFE5B4'; } document.getElementById('solutionInputId').setAttribute('disabled', ''); } function showInputMask() { document.getElementById('userButtonId').innerHTML = '&#10148'; document.getElementById('solutionInputId').value = ''; document.getElementById('solutionInputId').style.color = 'black'; document.getElementById('solutionInputId').style.background = 'white'; document.getElementById('userButtonId').style.background = darkColor; document.getElementById('userButtonId').removeAttribute('disabled', ''); } function showWaitingMask() { document.getElementById('userButtonId').innerHTML = '&#x21bb'; document.getElementById('userButtonId').style.background = brightColor; document.getElementById('userButtonId').setAttribute('disabled', ''); } </script></body></html>";


// Application
void setup() {
  initEsp();
  initRandomGenerator();
  initWiFi();
  initWebserver();
  initExercise();
}


void loop() {
  server.handleClient();
  webSocket.loop();
  checkResponsivity();
}


void initEsp() {
  Serial.begin(115200);
  pinMode(MotorPin, OUTPUT);
  pinMode(SwitchRightPin, INPUT_PULLUP);
  pinMode(SwitchLeftPin, INPUT_PULLUP);
  rgbLed.begin();
}


void initRandomGenerator() {
  unsigned long seed = 0;
  for (int i = 0; i < 32; i++) {
    seed = seed | ((analogRead(RandomSignalPin) & 0x01) << i);
  }
  randomSeed(seed);
}


void initWiFi() {
  WiFi.softAP(Ssid, Password, IsSsidHidden, MaxNumberOfUsers);
  IPAddress ipAdress = WiFi.softAPIP();
  Serial.print("IP address: ");
  Serial.println(ipAdress);
}


void initWebserver() {
  server.on("/", []() {
    server.send(200, "text\html", webpage);
  });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  setColor(LedColor::WHITE);
}


void initExercise() {
  difficultyLevel = DifficultyLevel::EASY;
  createExercise();
}


void checkResponsivity() {
  long currentTime = millis();
  if (currentTime - LastResponsivityCheck_ms > ResponsivityCheckInterval_ms) {
    std::vector<uint8_t> inresponsiveUserIds;
    for (auto& user : users) {
      if (currentTime - user.timeStampOfLiveSignal > MaxInresponsiveTime_ms) {
        inresponsiveUserIds.push_back(user.id);
      }
    }
    for (auto& id : inresponsiveUserIds) {
      webSocket.disconnect(id);
      removeUser(id);
    }
    LastResponsivityCheck_ms = currentTime;
  }
}


void webSocketEvent(byte num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("User " + String(num) + " connected.");
      handleConnection(num);
      break;
    case WStype_DISCONNECTED:
      Serial.println("User " + String(num) + " disconnected.");
      handleDisconnection(num);
      break;
    case WStype_TEXT:
      Serial.println("User " + String(num) + " sent payload.");
      handlePayload(num, payload);
      break;
  }
  printUsers();
}


void handleConnection(uint8_t id) {
  addUser(id);
  sendDifficultyLevel();
  User& currentUser = getUser(id);
  sendExercise(currentUser, true);
}


void handleDisconnection(uint8_t id) {
  removeUser(id);
}


void handlePayload(uint8_t id, uint8_t* payload) {
  User& currentUser = getUser(id);
  if (readPayload(payload)) {
    String payloadId = jsonPayload["payloadId"];
    if (payloadId == "liveSignal") {
      currentUser.timeStampOfLiveSignal = millis();
    }
    if (payloadId == "setDifficultyLevel") {
      setDifficultyLevel(jsonPayload["param1"]);
      sendDifficultyLevel();
      sendNewExercise(false);
    }
    if (payloadId == "processResult") {
      processResult(currentUser, jsonPayload["param1"]);
      sendStatistics();
      sendNewExercise(true);
    }
  }
}


bool readPayload(const uint8_t* payload) {
  DeserializationError error = deserializeJson(jsonPayload, payload);
  if (error) {
    Serial.println("Deserialization failed");
    return false;
  } else {
    return true;
  }
}


void writePayload(String payloadId, String param1 = "", String param2 = "", String param3 = "", String param4 = "") {
  jsonPayload["payloadId"] = payloadId;
  jsonPayload["param1"] = param1;
  jsonPayload["param2"] = param2;
  jsonPayload["param3"] = param3;
  jsonPayload["param4"] = param4;
}


void addUser(uint8_t id) {
  User user;
  user.id = id;
  users.push_back(user);
}


void removeUser(uint8_t id) {
  if (isUserExisting(id)) {
    users.erase(users.begin() + getUserVectorPos(id));
  }
}


User& getUser(uint8_t id) {
  return users.at(getUserVectorPos(id));
}


uint8_t getUserVectorPos(uint8_t id) {
  uint8_t pos = 0;
  while (users.at(pos).id != id) {
    pos++;
  }
  return pos;
}


bool isUserExisting(uint8_t id) {
  bool isExisting = false;
  for (auto& user : users) {
    if (user.id == id) {
      isExisting = true;
    }
  }
  return isExisting;
}


void printUsers() {
  Serial.println("Registered Users: ");
  for (auto& user : users) {
    Serial.println(user.id);
  }
  Serial.println();
}


void processResult(User& currentUser, int16_t result) {
  if (result == SecretNumber) {
    setColor(LedColor::BLUE);
    dispenseCandy(numberCandyPerDifficultyLevel[difficultyLevel]);
  } else if (result == solution) {
    sendResultStatus(currentUser, true);
    setColor(LedColor::GREEN);
    dispenseCandy(numberCandyPerDifficultyLevel[difficultyLevel]);
  } else {
    sendResultStatus(currentUser, false);
    setColor(LedColor::RED);
  }
  delay(TimeOfResultStatusNotification_ms);
  setColor(LedColor::WHITE);
  sendStatistics();
}


void sendResultStatus(User& currentUser, bool isResultCorrect) {
  String payloadId = "setResultStatus";
  String status;
  if (isResultCorrect) {
    currentUser.numberOfCorrects++;
    status = "correct";
  } else {
    status = "wrong";
  }
  writePayload(payloadId, status, String(solution));
  sendPayload(currentUser);

  status = "tooLate";
  for (auto& user : users) {
    if (user != currentUser) {
      writePayload(payloadId, status, String(solution));
      sendPayload(user);
    }
  }
}


void sendStatistics() {
  for (auto& user : users) {
    writePayload("setStatistics", String(user.numberOfExercises), String(user.numberOfCorrects));
    sendPayload(user);
  }
}


void sendNewExercise(bool isIncrementExerciseCounter) {
  createExercise();
  for (auto& user : users) {
    sendExercise(user, isIncrementExerciseCounter);
  }
}


void sendExercise(User& user, bool isIncrementExerciseCounter) {
  if (isIncrementExerciseCounter) {
    user.numberOfExercises++;
  }
  writePayload("setExercise", String(operand), String(number1), String(number2));
  sendPayload(user);
}


void sendDifficultyLevel() {
  writePayload("setDifficultyLevel", difficultyLevelIds[difficultyLevel]);
  sendPayloadToAllUsers();
}


void sendPayloadToAllUsers() {
  String jsonStringTx = "";
  serializeJson(jsonPayload, jsonStringTx);
  webSocket.broadcastTXT(jsonStringTx);
}


void sendPayload(User& user) {
  String jsonStringTx = "";
  serializeJson(jsonPayload, jsonStringTx);
  webSocket.sendTXT(user.id, jsonStringTx);
}


void createExercise() {
  switch (random(0, 4)) {
    case 0:
      operand = "+";
      number1 = random(rangeAddition[difficultyLevel].min, rangeAddition[difficultyLevel].max);
      number2 = random(rangeAddition[difficultyLevel].min, rangeAddition[difficultyLevel].max);
      solution = number1 + number2;
      break;
    case 1:
      operand = "-";
      number1 = random(rangeAddition[difficultyLevel].min, rangeAddition[difficultyLevel].max);
      number2 = random(rangeAddition[difficultyLevel].min, rangeAddition[difficultyLevel].max);
      if (number1 < number2) {
        int16_t temp = number1;
        number1 = number2;
        number2 = temp;
      }
      solution = number1 - number2;
      break;
    case 2:
      operand = "*";
      number1 = random(rangeMultiplicationNumber1[difficultyLevel].min, rangeMultiplicationNumber1[difficultyLevel].max);
      number2 = random(rangeMultiplicationNumber2[difficultyLevel].min, rangeMultiplicationNumber2[difficultyLevel].max);
      solution = number1 * number2;
      break;
    case 3:
      operand = ":";
      solution = random(rangeMultiplicationNumber1[difficultyLevel].min, rangeMultiplicationNumber1[difficultyLevel].max);
      number2 = random(rangeMultiplicationNumber2[difficultyLevel].min, rangeMultiplicationNumber2[difficultyLevel].max);
      number1 = solution * number2;
      break;
  }
}


void setDifficultyLevel(String value) {
  if (value == difficultyLevelIds[DifficultyLevel::EASY]) {
    difficultyLevel = DifficultyLevel::EASY;
  }
  if (value == difficultyLevelIds[DifficultyLevel::MEDIUM]) {
    difficultyLevel = DifficultyLevel::MEDIUM;
  }
  if (value == difficultyLevelIds[DifficultyLevel::HARD]) {
    difficultyLevel = DifficultyLevel::HARD;
  }
}


void dispenseCandy(uint8_t quantity) {
  startMotor();
  for (uint8_t q = 0; q < quantity; q++) {
    waitUntilCandyIsDispensed();
  }
  stopMotor();
}


void waitUntilCandyIsDispensed() {
  unsigned long startTime = millis();
  unsigned long MaxRuntime_ms = 1000 * static_cast<unsigned long>(MaxRuntimePerSingleDispense_s);
  while (millis() - startTime < MaxRuntime_ms && getPusherPosition() != PusherPosition::CENTER) {};
  while (millis() - startTime < MaxRuntime_ms && getPusherPosition() == PusherPosition::CENTER) {};
}


void startMotor() {
  uint8_t currentSpeed = 0;
  while (currentSpeed < SpeedLevel) {
    currentSpeed++;
    analogWrite(MotorPin, currentSpeed);
    delayMicroseconds(1000000 / Acceleration_BitsPerSecond);
  }
}


void stopMotor() {
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


void setColor(LedColor color) {
  switch (color) {
    case LedColor::NO:
      rgbLed.setPixelColor(0, 0, 0, 0);
      break;
    case LedColor::RED:
      rgbLed.setPixelColor(0, 120, 0, 0);
      break;
    case LedColor::GREEN:
      rgbLed.setPixelColor(0, 0, 120, 0);
      break;
    case LedColor::BLUE:
      rgbLed.setPixelColor(0, 0, 0, 120);
      break;
    case LedColor::WHITE:
      rgbLed.setPixelColor(0, 40, 40, 40);
      break;
  }
  rgbLed.show();
}
