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


// Typen-Definition
enum class LedColor {
  NO,
  RED,
  GREEN,
  BLUE,
  WHITE
};


// Globale Variablen
bool isNewGame = false;


// Konfiguration der Pins
const uint8_t RandomSignalPin = 0;
const uint8_t SwitchPin = 6;
const uint8_t MotorPin = 7;
const uint8_t LedPin = 8;


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
StaticJsonDocument<MaxJsonDocumentSize> jsonDocumentTx;
StaticJsonDocument<MaxJsonDocumentSize> jsonDocumentRx;
String webpage = "<p> &nbsp;</p><h1>Lisa's Suessigkeitenautomat</h1><table> <thead> <tr> <td> </td> </tr> </thead> <tbody> <tr> <td style='text-align: center;'> <p><strong><span id='sum1'>-</span></strong></p> </td> <td style='text-align: center;'> <p><strong>+</strong></p> </td> <td style='text-align: center;'> <p><strong><span id='sum2'>-</span></strong></p> </td> <td style='text-align: center;'> <p><strong>=</strong></p> </td> <td> <td><input type='number' name='lsgAdd' id='lsgAdd' max='9999' min='-9999' step='1'></td> </td> </tr> </tbody></table><p><button id='BTN_CHECK' type='button'>Check result </button></p><p><button id='BTN_SEND_BACK' type='button' hidden>Continue </button></p><script> var Socket; var obj; var CorrectAnswers; document.getElementById('BTN_CHECK').addEventListener('click', button_check_result); document.getElementById('BTN_SEND_BACK').addEventListener('click', button_send_back); function init() { Socket = new WebSocket('ws://' + window.location.hostname + ':81/'); Socket.onmessage = function(event) { processCommand(event); }; } function button_check_result() { CorrectAnswers = 0; document.getElementById('lsgAdd').setAttribute('disabled', ''); if (document.getElementById('lsgAdd').value == obj.LSGadd) { CorrectAnswers += 1; document.getElementById('lsgAdd').style.color = 'green'; } else { document.getElementById('lsgAdd').style.color = 'red'; } console.log(CorrectAnswers); document.getElementById('BTN_CHECK').setAttribute('hidden', 'hidden'); document.getElementById('BTN_SEND_BACK').removeAttribute('hidden'); } function button_send_back() { var loesungen = { count: CorrectAnswers, }; ResetCounter = 0; document.getElementById('lsgAdd').value = ''; document.getElementById('lsgAdd').style.color = 'black'; console.log(JSON.stringify(loesungen)); Socket.send(JSON.stringify(loesungen)); document.getElementById('BTN_CHECK').removeAttribute('hidden'); document.getElementById('BTN_SEND_BACK').setAttribute('hidden', 'hidden'); } function processCommand(event) { obj = JSON.parse(event.data); document.getElementById('sum1').innerHTML = obj.sum1; document.getElementById('sum2').innerHTML = obj.sum2; document.getElementById('lsgAdd').removeAttribute('disabled', ''); console.log(obj.sum1); console.log(obj.sum2); } window.onload = function(event) { init(); }</script></html>";


void setup() {
  initEsp();
  initWiFi();
  initWebserver();
}


void loop() {
  server.handleClient();
  if (isNewGame == true) {
    updateWebContent();
  }
  webSocket.loop();
}


void initEsp() {
  Serial.begin(115200);
  unsigned long currentMillis = millis();
  randomSeed(analogRead(RandomSignalPin));

  rgbLed.begin();
  setColor(LedColor::WHITE);
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


void webSocketEvent(byte num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("Client " + String(num) + " disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("Client " + String(num) + " connected");
      updateWebContent();
      break;
    case WStype_TEXT:
      DeserializationError error = deserializeJson(jsonDocumentRx, payload);
      if (error) {
        Serial.print("Deserialization failed");
        return;
      } else {
        const int g_count = jsonDocumentRx["count"];
        Serial.print("Counts: " + String(g_count));
        isNewGame = true;
      }
      Serial.println("");
      break;
  }
}


void updateWebContent() {
  isNewGame = false;
  JsonObject jsonObjectTx = jsonDocumentTx.to<JsonObject>();
  String jsonStringTx = "";

  long sum1 = random(1, 100);
  long sum2 = random(1, 100);

  jsonObjectTx["sum1"] = sum1;
  jsonObjectTx["sum2"] = sum2;
  jsonObjectTx["LSGadd"] = sum1 + sum2; 

  serializeJson(jsonDocumentTx, jsonStringTx);
  Serial.println(jsonStringTx);
  webSocket.broadcastTXT(jsonStringTx);
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
