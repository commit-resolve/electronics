#include <Arduino.h>

#define STEP_A 26
#define DIR_A 27

#define STEP_B 14
#define DIR_B 12

#define MAGNET_PIN 25

bool commandReady = false;
String inputString = "";
bool stringComplete = false;
int c1, c2, c3, c4;

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

    delayMicroseconds(1000);   // pulse width

    digitalWrite(STEP_A, LOW);
    digitalWrite(STEP_B, LOW);

    delayMicroseconds(2000);
  }
}

void setup() {
  Serial.begin(115200);
  inputString.reserve(50);
  delay(1000);
  Serial.println("ESP32 started");
  delay(1000);
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

  int moveX = round(dx * squareSizeMM * stepsPerMM);
  int moveY = round(dy * squareSizeMM * stepsPerMM);

  int motorA = moveX + moveY;
  int motorB = moveX - moveY; 

  coreXYMove(moveX, moveY);
}

void pickPiece() {

  digitalWrite(MAGNET_PIN, HIGH); // magnet ON
  Serial.println("Electromagnet picked the piece");
  delay(2000);

}

void dropPiece() {

  digitalWrite(MAGNET_PIN, LOW); // magnet OFF
  Serial.println("Electromagnet dropped the piece");
  delay(2000);

}

void executeMove(int x1, int y1, int x2, int y2) {

    moveChess(0,0,x1,y1);
    pickPiece();

    moveChess(x1,y1,x2,y2);
    dropPiece();

    moveChess(x2,y2,0,0);

    Serial.println("OK");
}

void loop() {
  while (Serial.available()) {
        char c = Serial.read();

        if (c == '\n') {
            if (inputString.startsWith("MOVE")) {
                sscanf(inputString.c_str(), "MOVE %d %d %d %d", &c1, &c2, &c3, &c4);
                commandReady = true;
            }
            inputString = "";
        } else {
            inputString += c;
        }
    }

    if (commandReady) {
        executeMove(c1, c2, c3, c4);
        //executeMove(4,1,4,3);
        commandReady = false;

}
}