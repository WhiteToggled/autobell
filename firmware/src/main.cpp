#include <Arduino.h>
#include <HTTPClient.h>

#include "config.h"
#include "wifi.h"
#include "camera.h"

volatile bool bellRang = false;

void IRAM_ATTR on_door_rang() {
  Serial.println("Hi");
  bellRang = true;
}

void ring_buzzer() {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
}

void notify_server() {
  // POST bell_rang 
  // stream: 192.168.10.a/stream

  HTTPClient http;

  http.begin(SERVER_IP, SERVER_PORT, SERVER_PATH);
  http.addHeader("Content-Type", "application/json");
  String body = "{\"event\":\"bell_rang\",\"stream_url\":\"http://" + WiFi.localIP().toString() + "/stream\"}";

  int responseCode = http.POST(body);
  Serial.printf("Server response: %d\n", responseCode);
  http.end();
}

// -----------

void setup() {
  Serial.begin(115200);

  pinMode(BELL_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  attachInterrupt(BELL_PIN, on_door_rang, FALLING);

  connect_to_wifi();
  init_camera();

  Serial.println("starting");
}

void loop() {
  if (bellRang) {
    bellRang = false;

    Serial.println("hi im doing stuff now");
    ring_buzzer();
    notify_server();
  }
}