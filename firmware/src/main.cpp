#include <Arduino.h>

#define BELL_PIN 13

void IRAM_ATTR onDoorRang() {
  // do smth
  Serial.println("Hi");
}

void setup() {
  Serial.begin(115200);

  pinMode(BELL_PIN, INPUT_PULLUP);
  attachInterrupt(BELL_PIN, onDoorRang, FALLING);

  Serial.println("starting");
}

void loop() {
}