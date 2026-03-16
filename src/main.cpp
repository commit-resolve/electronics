#include <Arduino.h>

#define STEP_A 26
#define DIR_A 27

#define STEP_B 14
#define DIR_B 12

#define MAGNET_PIN 25

int squareSizeMM = 35;
// steps/mm = 200/(40T*4mm) - 40T*4mm -> pulley specs
float stepsPerMM = 1.25;


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

  int dirA = (stepsA >= 0) ? HIGH : LOW;
  int dirB = (stepsB >= 0) ? HIGH : LOW;

  digitalWrite(DIR_A, dirA);
  digitalWrite(DIR_B, dirB);

  stepsA = abs(stepsA);
  stepsB = abs(stepsB);

  int maxSteps = max(stepsA, stepsB);

  for (int i = 0; i < maxSteps; i++) {

    if (i < stepsA) {
      digitalWrite(STEP_A, HIGH);
    }

    if (i < stepsB) {
      digitalWrite(STEP_B, HIGH);
    }

    delayMicroseconds(3);   // pulse width

    digitalWrite(STEP_A, LOW);
    digitalWrite(STEP_B, LOW);

    delayMicroseconds(800);
  }
}

void setup() {

  pinMode(STEP_A, OUTPUT);
  pinMode(DIR_A, OUTPUT);

  pinMode(STEP_B, OUTPUT);
  pinMode(DIR_B, OUTPUT);

  pinMode(MAGNET_PIN, OUTPUT);
}

void moveChess(int x1, int y1, int x2, int y2) {
  // the move
  // E2 → E4

  //Calculate displacement
  // Start = (4,1)
  // End = (4,3)
  // ΔX = X₂ - X₁
  // ΔY = Y₂ - Y₁
  // ΔX = 4 - 4 = 0
  // ΔY = 3 - 1 = 2

  // Convert squares to millimeters
  // squareSize = 35 mm
  // moveY = 2 × 35 = 70 mm

  // GT2 belt
  // 40-tooth pulley - 4mm
  // 200-step motor

  //Convert millimeters to stepper steps
  // 40 x 4mm = 160 mm

  //Steps per mm:
  // 200/160 = 1.25 mm

  // Calcullate the distance on X and Y axis
  // moveX x steos/mm = 0 x 1.25 = 0
  // moveX x steps/mm = 70 x 1.25 = 87.5

  int dx = x2 - x1;
  int dy = y2 - y1;

  int moveX = dx * squareSizeMM * stepsPerMM;
  int moveY = dy * squareSizeMM * stepsPerMM;

  int motorA = moveX + moveY;
  int motorB = moveX - moveY;

  coreXYMove(motorA, motorB);
}

void pickPiece() {

  digitalWrite(MAGNET_PIN, HIGH); // magnet ON
  delay(500);

}

void dropPiece() {

  digitalWrite(MAGNET_PIN, LOW); // magnet OFF
  delay(500);

}

void loop() {

  // move right
  // coreXYMove(200,0);
  // delay(2000);

  // // move forward
  // coreXYMove(0,200);
  // delay(2000);

  //Move electromagnet from 00 (A1) to Initial square
  moveChess(0,0,5,2); 
  delay(5000);
  //Charge the electromagnet
  pickPiece();
  //Move electromagnet from initial sqaure to final square
  moveChess(5,2,6,0);
  delay(5000);
  //discharge the electromagnet
  dropPiece();
  //Move electromagnet from final square to 00 (A1)
  moveChess(6,0,0,0);
  delay(5000);

}