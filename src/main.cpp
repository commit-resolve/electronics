//============================START Full flow =====================

// Player lifts piece from e2
// 1. loop() calls stableScan() every 50ms
// 2. Bit 12 (e2 = rank 2, file e → index (2-1)*8 + 4 = 12) flips from 1 → 0
// 3. curr != stableState triggers → waits 80ms (DEBOUNCE_MS) → re-scans to confirm
// 4. processDiff(stableState, curr) runs:
//  lifted = bit 12 set, placed = nothing
//  Phase 1: srcSquare = "e2", returns
// 5. stableState = curr

// Player places piece on e4
// 1. Next scan — bit 28 (e4 = (4-1)*8 + 4 = 28) flips from 0 → 1
// 2.  processDiff() runs again:
//    lifted = nothing, placed = bit 28
//    Phase 2: dstSquare = "e4", builds move = "e2e4"
//    Serial.println("e2e4") → sent to Raspberry Pi
//    srcSquare reset to ""
// 3. stableState = curr

// Raspberry Pi processes and responds
// 1. Pi receives "e2e4", feeds it to Stockfish
// 2. Pi sends back "MOVE x1,y1,x2,y2\n" (e.g. "MOVE 3,6,3,4" for the AI's chosen reply)

// ESP32 executes the robot move
// 1. Next loop() iteration — handleSerialInput() runs first
// 2. Reads "MOVE 3,6,3,4", parses x1=3, y1=6, x2=3, y2=4
// 3. Calls executeMove(3, 6, 3, 4):
//    moveChess(0,0, 3,6) → CoreXY arm travels home → source square
//    pickPiece() → magnet ON, 2s hold
//    moveChess(3,6, 3,4) → arm travels source → destination
//    dropPiece() → magnet OFF, 2s hold
//    moveChess(3,4, 0,0) → arm returns home
//    Serial.println("OK") → Pi confirmed
//============================END Full flow =====================

// ==========================PINS CONFIG==================================
//  Chess board — ESP32 main.cpp
//  Reed switch 8x8 matrix scanner + move detector
//
//  Wiring summary
//  --------------
//  Row pins (OUTPUT): pull LOW one at a time to activate rank
//    GPIO13 = rank 1 (R0)    GPIO23 = rank 2 (R1)
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

//===================================START OF DETECTION==================================================

// ── Pin definitions ──────────────────────────────────────────
 
const int ROW_PINS[8] = {13, 23, 14, 27, 26, 25, 33, 32};
// rank 1..8 mapped to R0..R7
 
const int COL_PINS[8] = {34, 35, 36, 39, 4, 5, 18, 19};
// file a..h mapped to C0..C7
 
// ── Constants ───────────────────────────────────────────────
 
// Starting position: ranks 1+2 and 7+8 fully occupied
// rank1 = bits 0-7, rank2 = bits 8-15 → 0xFFFF (low end)
// rank7 = bits 48-55, rank8 = bits 56-63 → 0xFFFF (high end)
const uint64_t START_POS      = 0xFFFF00000000FFFF;
 
const int      DEBOUNCE_MS    = 80;   // ms to wait after detecting a change
const int      SCAN_INTERVAL  = 50;   // ms between full board scans
const int      SETTLE_READS   = 3;    // consecutive identical reads before accepting
 
// ── State ───────────────────────────────────────────────────
 
uint64_t prevState  = START_POS;
uint64_t stableState = START_POS;
String   srcSquare  = "";             // set when piece lifted, cleared after move sent

//===================================END OF DETECTION==================================================

#define STEP_A     15
#define DIR_A      16

#define STEP_B     17
#define DIR_B      21

#define MAGNET_PIN 22

int squareSizeMM = 35;
// steps/mm = 200/(40T*4mm) - 40T*4mm -> pulley specs
float stepsPerMM = 1.25;

//====================================================START OF DETECTION FUNCTIONS============================

// ── Helper: convert bit index to square name ────────────────
 
String bitToSquare(int bitIndex) {
    int rank = (bitIndex / 8) + 1;             // 0-7 → rank 1-8
    char file = 'a' + (bitIndex % 8);          // 0-7 → a-h
    return String(file) + String(rank);        // e.g. "e2"
}
 
// ── Scan entire board → returns 64-bit presence map ─────────
 
uint64_t scanMatrix() {
    //The concept — row/column matrix scanning:
    //All 64 squares are wired as an 8×8 grid. You can't read all 64 at once, so you activate one row at a time and read all 8 columns for that row, then move to the next row.
    
    uint64_t state = 0;
 
    for (int row = 0; row < 8; row++) {
 
        // activate this row by pulling it LOW
        //pulls that rank's wire LOW to activate it. The reed switch under a piece connects that row wire to its column wire.
        digitalWrite(ROW_PINS[row], LOW);
        delayMicroseconds(150);                // let the line settle, gives the signal time to travel down the wire and stabilise before reading.
 
        for (int col = 0; col < 8; col++) {
            // INPUT_PULLUP: LOW = switch closed = piece present
            //if a piece is on this square, its reed switch is closed, pulling the column pin LOW (remember the pins are INPUT_PULLUP, so they sit HIGH by default and only go LOW when pulled down through a closed switch).
            if (digitalRead(COL_PINS[col]) == LOW) {
              //maps the 2D grid position to a single number 0–63. Row 0, col 0 = bit 0 (a1). Row 7, col 7 = bit 63 (h8).
                int bitIndex = row * 8 + col;
                //mark square number bitIndex as occupied in the board map
                state |= (1ULL << bitIndex);   // set bit for this square
            }
        }
 
        // deactivate row
        //deactivates the row before moving to the next one, so only one row is ever active at a time.
        digitalWrite(ROW_PINS[row], HIGH);

        //brief pause to let the line go fully HIGH before the next row activates.
        delayMicroseconds(50);
    }
    //Returns a uint64_t where each bit represents one square — 1 = piece present, 0 = empty.
    return state;
}
 
// ── Debounced scan: only accept state after SETTLE_READS ────
 
uint64_t stableScan() {
  //Takes a first scan via scanMatrix(), stores it in last, starts sameCount at 1, 
    uint64_t last = scanMatrix();
    int sameCount = 1;

    // Waits 500µs, scans again
    while (sameCount < SETTLE_READS) {
        delayMicroseconds(500);
        uint64_t curr = scanMatrix();

    //If the new scan matches last → increments sameCount
        if (curr == last) {
            sameCount++;
    //If it doesn't match → the switch is still bouncing, resets last to the new value and sameCount back to 1
        } else {
            last = curr;
            sameCount = 1;
        }
    }
  //Keeps looping until 3 reads in a row return the same state
  //Returns that stable 64-bit board state
    return last;

  //Why 500µs between reads? Reed switch bounce typically settles within a few milliseconds. Three reads × 500µs = ~1.5ms minimum inside the loop, which is enough to catch most bounce events without being slow.
  //What scanMatrix() returns is a raw snapshot — stableScan() is the layer that makes it trustworthy before anything acts on it.
}
 
// ── Detect move from state diff ─────────────────────────────
//
//  diff   = prev XOR curr   → bits that changed
//  lifted = diff AND prev   → was 1, now 0  (piece left this square)
//  placed = diff AND curr   → was 0, now 1  (piece arrived on this square)
 
void processDiff(uint64_t prev, uint64_t curr) {
    //XOR gives you every bit that changed between the two scans. A bit is 1 if it was different, 0 if it was the same
    uint64_t diff   = prev ^ curr;
    if (diff == 0) return;                     // nothing changed
    //AND with prev gives bits that were 1 before and are now 0. A piece left that square.
    uint64_t lifted = diff & prev;
    // AND with curr gives bits that were 0 before and are now 1. A piece arrived on that square.
    uint64_t placed = diff & curr;
 
    // ── Phase 1: piece lifted → record src ──────────────────
    //Condition: something was lifted (lifted non-zero), nothing placed yet, and we're not already mid-move (srcSquare == "").
    if (lifted && !placed && srcSquare == "") {
        //counts trailing zeros, which finds the position of the lowest set bit. This gives the bit index of the square the piece came from.
        int bitIndex = __builtin_ctzll(lifted);  // lowest set bit
        // converts that index to "e2".Stored in srcSquare and waits.
        srcSquare = bitToSquare(bitIndex);
        // e.g. srcSquare = "e2"
        // wait for piece to land before sending
        return;
    }
 
    // ── Phase 2: piece placed → complete move ───────────────
    if (placed && srcSquare != "") {
      //counts trailing zeros, which finds the position of the lowest set bit. This gives the bit index of the square the piece came from
        int bitIndex = __builtin_ctzll(placed);
      //converts that index to "e4". Stored in dstSquare 
        String dstSquare = bitToSquare(bitIndex);
 
        String move = srcSquare + dstSquare;    // "e2e4"
        Serial.println(move);                   // send to Raspberry Pi
 
        srcSquare = "";                         // reset for next move
        return;
    }
 
    // ── Capture edge case: only 1 square changes ────────────
    //  When you capture, the destination already has a piece (HIGH).
    //  The opponent's piece was already removed by the board's
    //  physical state after the opponent move — so you see:
    //    your src goes LOW (piece lifted from source)
    //  The dst bit was already 1 (opponent piece there), and
    //  stays 1 after you place. diff only shows src changing.
    //  The Pi handles this: it knows a capture is legal from
    //  the game state, and infers dst from the valid move list.
    //
    //  If you want the ESP32 to handle it fully, send src alone:
    if (lifted && !placed && srcSquare != "") {
        // second lift without a place = error / piece removed mid-move
        // reset and wait for correct sequence
        srcSquare = "";
    }
}
 
// ── Receive CoreXY command from Pi and echo/act ─────────────
//
//  Pi sends: "MOVE x1,y1,x2,y2\n"
//  Here we just echo it back for confirmation.
//  Replace this with your actual CoreXY stepper control.
 
void handleSerialInput() {
    //checks if there's anything waiting in the serial buffer from the Pi. If nothing has arrived, the whole function exits immediately and the loop continues scanning.
    if (Serial.available()) {
        //reads the incoming bytes up to the newline character, giving you the full message as a String. trim() strips any trailing \r or spaces.
        String line = Serial.readStringUntil('\n');
        line.trim();
        //the Pi's AI response. substring(5) strips the "MOVE " prefix leaving just "3,6,3,4". sscanf then unpacks those 4 comma-separated integers into x1, y1, x2, y2. Those get passed straight to executeMove() which drives the arm. 
        if (line.startsWith("MOVE ")) {
            // parse: "MOVE 171,342,171,228"
            String coords = line.substring(5);
            int x1, y1, x2, y2;
            sscanf(coords.c_str(), "%d,%d,%d,%d", &x1, &y1, &x2, &y2);
 
            executeMove(x1, y1, x2, y2);
            delay(500);
            stableState = stableScan();
        }
        //RESET — a recovery command. Resets prevState, stableState, and srcSquare back to the starting position. Useful if the board gets out of sync with the software state (e.g. pieces were moved while powered off). Replies "RESET_OK" so the Pi knows it landed.
        if (line == "RESET") {
            prevState    = START_POS;
            stableState  = START_POS;
            srcSquare    = "";
            Serial.println("RESET_OK");
        }
      //One thing to be aware of: readStringUntil('\n') is blocking — it waits until a newline arrives. This means if the Pi sends a malformed message with no newline, the ESP32 hangs here. Not a problem in normal operation, but worth knowing.
    }
}

//====================================================END OF DETECTION FUNCTIONS============================

//====================================================START OF AUTOMATE MOVE FUNCTIONS============================
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

  // int motorA = moveX + moveY;  // CoreXY math reference: A = X+Y, B = X-Y
  // int motorB = moveX - moveY;

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

//====================================================END OF AUTOMATE MOVE FUNCTIONS============================
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
//====================================================START SETUP============================

void setup() {
    Serial.begin(115200);

    for (int i = 0; i < 8; i++) {
        pinMode(ROW_PINS[i], OUTPUT);
        digitalWrite(ROW_PINS[i], HIGH);
    }

    for (int i = 0; i < 8; i++) {
        pinMode(COL_PINS[i], INPUT_PULLUP);
    }

    pinMode(STEP_A,     OUTPUT);
    pinMode(DIR_A,      OUTPUT);
    pinMode(STEP_B,     OUTPUT);
    pinMode(DIR_B,      OUTPUT);
    pinMode(MAGNET_PIN, OUTPUT);
    digitalWrite(MAGNET_PIN, LOW);

    delay(500);
    stableState = stableScan();
    prevState   = stableState;

    Serial.println("READY");
}
//====================================================END SETUP============================
//====================================================START LOOP============================
void loop() {

    handleSerialInput();

    //Reed switches bounce — when a piece is placed or lifted, the switch rapidly opens and closes a few times before settling. stableScan() filters that noise out by requiring 3 identical consecutive reads before trusting the result (SETTLE_READS = 3).
    uint64_t curr = stableScan();

    if (curr != stableState) {
        delay(DEBOUNCE_MS);
        curr = stableScan();

        if (curr != stableState) {
            processDiff(stableState, curr);
            stableState = curr;
        }
    }

    delay(SCAN_INTERVAL);
}
//====================================================END LOOP============================
