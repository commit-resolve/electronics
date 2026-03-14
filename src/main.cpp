#include <Arduino.h>

#define STEP_A 26
#define DIR_A 27

#define STEP_B 14
#define DIR_B 12

void stepMotor(int stepPin, int dirPin, int steps) {

  if (steps >= 0)
    digitalWrite(dirPin, HIGH);
  else {
    digitalWrite(dirPin, LOW);
    steps = -steps;
  }

  for (int i = 0; i < steps; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(800);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(800);
  }
}

void coreXYMove(int x, int y) {

  int stepsA = x + y;
  int stepsB = x - y;

  int maxSteps = max(abs(stepsA), abs(stepsB));

  for (int i = 0; i < maxSteps; i++) {

    if (i < abs(stepsA)) {
      digitalWrite(STEP_A, HIGH);
      digitalWrite(STEP_A, HIGH);
    }

    if (i < abs(stepsB)) {
      digitalWrite(STEP_B, HIGH);
      digitalWrite(STEP_B, LOW);
    }

    delayMicroseconds(800);
  }
}

void setup() {

  pinMode(STEP_A, OUTPUT);
  pinMode(DIR_A, OUTPUT);

  pinMode(STEP_B, OUTPUT);
  pinMode(DIR_B, OUTPUT);
}

void loop() {

  // move right
  coreXYMove(200,0);
  delay(2000);

  // move forward
  coreXYMove(0,200);
  delay(2000);

}