#include <Arduino.h>
#include <HTTPClient.h>

#include "config.h"
#include "wifi.h"
#include "camera.h"
#include "server.h"
#include "esp_wifi.h"

volatile bool bellRang = false;
void IRAM_ATTR on_door_rang() { bellRang = true; }

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
  Serial.printf("PSRAM initialized. Size %d bytes\n", ESP.getPsramSize());

  pinMode(BELL_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  attachInterrupt(BELL_PIN, on_door_rang, FALLING);

  connect_to_wifi();
  init_camera();
  init_server();

  wifi_ap_record_t ap;
  esp_wifi_sta_get_ap_info(&ap);
  Serial.printf("RSSI: %d dBm\n", ap.rssi);

  Serial.println("hi im doing stuff now");
}

void loop() {
  if (bellRang) {
    bellRang = false;

    Serial.println("hello someone rang the bell");
    ring_buzzer();
    notify_server();
  }
}