#include <Arduino.h>

#include <Stepper.h>
#include <WiFi.h>
#include <WebServer.h>
#include <uri/UriBraces.h>
#include <ArduinoNvs.h>
#include <string>  

#include "wifi_cred.h"

#define WIFI_TIMEOUT_MS 5000

WebServer server(80); 

enum state {
  down, up, moving
};

state currentState = up;
state futureState = up;

#define STEPS_PER_REVOLUTION 2048

#define STEPPER_PIN_1 32
#define STEPPER_PIN_2 14
#define STEPPER_PIN_3 22
#define STEPPER_PIN_4 23

Stepper motor(STEPS_PER_REVOLUTION, STEPPER_PIN_1, STEPPER_PIN_3, STEPPER_PIN_2, STEPPER_PIN_4);

void moveDirection(state direction)
{
  motor.setSpeed(3);
  for (int i = 0; i < 25; i++)
  {
    if (direction == up)
    {
      motor.step(-STEPS_PER_REVOLUTION);
    }
    else if(direction == down)
    {
      motor.step(STEPS_PER_REVOLUTION);
    }
  }
  delay(100);
  digitalWrite(STEPPER_PIN_1, LOW);
  digitalWrite(STEPPER_PIN_2, LOW);
  digitalWrite(STEPPER_PIN_3, LOW);
  digitalWrite(STEPPER_PIN_4, LOW);

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

void buttonTask(void *parameters){
  int buttonState;

  for(;;){
    vTaskDelay(10 / portTICK_PERIOD_MS);
    buttonState = digitalRead(0);
    if(buttonState == HIGH) continue;
    if(currentState == up) {
      futureState = down;
    }
    else if(currentState == down){
      futureState = up;
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
    }
    server.send(200, "text/plain", "Direction: '" + directionString + "'");
  });

  server.on("/current", []() {
    server.send(200, "text/plain", String(currentState));
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
  pinMode(STEPPER_PIN_1, OUTPUT);
  pinMode(STEPPER_PIN_2, OUTPUT);
  pinMode(STEPPER_PIN_3, OUTPUT);
  pinMode(STEPPER_PIN_4, OUTPUT);

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
  xTaskCreate(buttonTask, "buttonTask", 5000, NULL, 1, NULL);
}

void loop() {}