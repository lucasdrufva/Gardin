#include <Arduino.h>

#include <Stepper.h>
#include <WiFi.h>
#include <WebServer.h>
#include <uri/UriBraces.h>
#include <ArduinoNvs.h>

#include "wifi_cred.h"

#define WIFI_TIMEOUT_MS 5000

WebServer server(80); 

enum state {
  down, up, moving
};

state currentState = up;
state futureState = up;

const int stepsPerRevolution = 2048;

int in1Pin = 32;
int in2Pin = 14;
int in3Pin = 22;
int in4Pin = 23;

Stepper motor(stepsPerRevolution, in1Pin, in3Pin, in2Pin, in4Pin);

void moveDirection(state direction)
{
  motor.setSpeed(3);
  //25
  for (int i = 0; i < 1; i++)
  {
    if (direction == up)
    {
      motor.step(-stepsPerRevolution);
    }
    else if(direction == down)
    {
      motor.step(stepsPerRevolution);
    }
  }
  delay(100);
  digitalWrite(in1Pin, LOW);
  digitalWrite(in2Pin, LOW);
  digitalWrite(in3Pin, LOW);
  digitalWrite(in4Pin, LOW);

  currentState = direction;
  NVS.setInt("state", currentState);
}


void stepperTask(void *parameters){
  for(;;){
    vTaskDelay(10 / portTICK_PERIOD_MS);
    if(currentState != futureState && currentState != moving){
      Serial.println("move");
      Serial.println(currentState);
      Serial.println(futureState);
      currentState = moving;

      
      moveDirection(futureState);
    }
  }
}

void keepWiFiAlive(void *parameters)
{
  for (;;)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("Wifi still connected");
      vTaskDelay(10000 / portTICK_PERIOD_MS);
      continue;
    }

    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS)
    {
      vTaskDelay(500 / portTICK_PERIOD_MS);
      Serial.print(".");
    }

    if(WiFi.status() != WL_CONNECTED){
      Serial.println("wifi failed");
      vTaskDelay(20000 / portTICK_PERIOD_MS);
      continue;
    }

    // Print local IP address and start web server
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

// HTML & CSS contents which display on web server
String HTML = "<!DOCTYPE html>\
<html>\
<body>\
<h1>Gardin &#128522;</h1>\
<a href=\"/move/up\">UP</a>\
<br/>\
<a href=\"/move/down\">DOWN</a>\
</body>\
</html>";

// Handle root url (/)
void handle_root() {
  server.send(200, "text/html", HTML);
}

void serverTask(void *parameters){
 
  server.on("/", handle_root);

  server.on(UriBraces("/move/{}"), []() {
    String directionString = server.pathArg(0);
    if(directionString == "up"){
      futureState = up;
      Serial.println("move up!");
    }else if(directionString == "down"){
      futureState = down;
      Serial.println("move down!");
      Serial.println(currentState);
      Serial.println(futureState);
    }
    server.send(200, "text/plain", "Direction: '" + directionString + "'");
  });

  //Wait for wifi
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  server.begin();
  for(;;){
    server.handleClient();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}



void setup()
{
  pinMode(in1Pin, OUTPUT);
  pinMode(in2Pin, OUTPUT);
  pinMode(in3Pin, OUTPUT);
  pinMode(in4Pin, OUTPUT);

  Serial.begin(115200);

  NVS.begin();
  delay(1000);
  currentState = static_cast<state>(NVS.getInt("state"));
  futureState = currentState;
  Serial.print("Current state: ");
  Serial.println(currentState);

  xTaskCreatePinnedToCore(keepWiFiAlive, "keepWiFiAlive", 5000, NULL, 1, NULL, CONFIG_ARDUINO_RUNNING_CORE);
  xTaskCreate(serverTask, "serverTask", 5000, NULL, 1, NULL);
  xTaskCreate(stepperTask, "stepperTask", 10000, NULL, 1, NULL);
}

void loop()
{
}


/*
#include <WiFi.h>
#include <WebServer.h>

// SSID & Password
const char* ssid = "Hemma2020";  // Enter your SSID here
const char* password = "Almvagen66@";  //Enter your Password here

WebServer server(80);  // Object of WebServer(HTTP port, 80 is defult)


// HTML & CSS contents which display on web server
String HTML = "<!DOCTYPE html>\
<html>\
<body>\
<h1>My First Web Server with ESP32 - Station Mode &#128522;</h1>\
</body>\
</html>";

// Handle root url (/)
void handle_root() {
  server.send(200, "text/html", HTML);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Try Connecting to ");
  Serial.println(ssid);

  // Connect to your wi-fi modem
  WiFi.begin(ssid, password);

  // Check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected successfully");
  Serial.print("Got IP: ");
  Serial.println(WiFi.localIP());  //Show ESP32 IP on serial

  server.on("/", handle_root);

  server.begin();
  Serial.println("HTTP server started");
  delay(100); 
}

void loop() {
  server.handleClient();
}
*/