// ============================================================
//  Chess board — ESP32 main.cpp
//  Reed switch 8x8 matrix scanner + move detector
//
//  Wiring summary
//  --------------
//  Row pins (OUTPUT): pull LOW one at a time to activate rank
//    GPIO13 = rank 1 (R0)    GPIO12 = rank 2 (R1)
//    GPIO14 = rank 3 (R2)    GPIO27 = rank 4 (R3)
//    GPIO26 = rank 5 (R4)    GPIO25 = rank 6 (R5)
//    GPIO33 = rank 7 (R6)    GPIO32 = rank 8 (R7)
//
//  Col pins (INPUT_PULLUP): read LOW when piece is present
//    GPIO34 = file a (C0)    GPIO35 = file b (C1)
//    GPIO36 = file c (C2)    GPIO39 = file d (C3)
//    GPIO4  = file e (C4)    GPIO5  = file f (C5)
//    GPIO18 = file g (C6)    GPIO19 = file h (C7)
//
//  Serial output to Raspberry Pi: 9600 baud, USB
//    ESP32 sends:  "e2e4\n"   (player move)
//    ESP32 reads:  "MOVE x1,y1,x2,y2\n"  (Stockfish reply coords)
//
//  Bit index layout in uint64_t
//    bit = (rank-1)*8 + file_index
//    e.g.  a1=0  h1=7  a2=8  e2=12  e4=28  h8=63
// ============================================================
 

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

// void loop() {
//   // while (Serial.available()) {
//   //       char c = Serial.read();

//   //       if (c == '\n') {
//   //           if (inputString.startsWith("MOVE")) {
//   //               sscanf(inputString.c_str(), "MOVE %d %d %d %d", &c1, &c2, &c3, &c4);
//   //               commandReady = true;
//   //           }
//   //           inputString = "";
//   //       } else {
//   //           inputString += c;
//   //       }
//   //   }
//   // After player move is detected (reed switches/hall sensors)
//     Serial.println("e2e4");   // Send player move to Python
    
//     // Wait for Python's response
//     while (!Serial.available());
//     String aiMove = Serial.readStringUntil('\n');  // e.g. "D7D5"
//     // Parse aiMove and drive steppers
  
//     if (commandReady) {
//         executeMove(c1, c2, c3, c4);
//         //executeMove(4,1,4,3);
//         commandReady = false;

// }
// }
void loop() {
  // stockfish best move= e2e4
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.length() == 0) return;

    // Parse space-separated integers "c1 c2 c3 c4"
    int coords[4];
    int idx = 0;
    char buf[32];
    input.toCharArray(buf, sizeof(buf));
    char* token = strtok(buf, " ");

    while (token != NULL && idx < 4) {
      coords[idx++] = atoi(token);
      token = strtok(NULL, " ");
    }

    if (idx == 4) {
      executeMove(coords[0], coords[1], coords[2], coords[3]);
    } else {
      Serial.println("ERROR: Expected 4 integers");
    }
  }
}