// ARDUINO
#include <math.h>

// LOAD CELL
#include "HX711.h"
#define LOADCELL_DOUT_PIN 11
#define LOADCELL_SCK_PIN 12
HX711 scale;
float calibration_factor = 250; 

// BUTTON
int rowCounter = 0; 
int columnCounter = 0; 
int foundColumn = 0;
boolean foundCol = false;
int keyValue = 0;
int noKey = 0;
boolean readKey = false;
int debounce = 300; 
String keyString;
int keyNum; 
boolean numericValue = false; 
const int row1 = 2;
const int row2 = 3;
const int row3 = 4;
const int row4 = 5;
const int row5 = 6;
const int colA = 7;
const int colB = 8;
const int colC = 9;
const int colD = 10;

// RELAY
const int latchPin = A1;
const int clockPin = A0;
const int dataPin = A2;
const int relay9 = 13;

// LED
const int alarmLedGreen = A3;
const int alarmLedRed = A5;
const int alarmLedOrange = A4;

// VAR
String cals = "0";
bool calsRetain = true;           // Button # continue string or new
bool calsReady = true;            // Button # Ready to press
float calsDebounce = 5500;
float calsCurTime = 0;

// FLAGS
bool calsSample = false;

// scale result
int xScaleFinish = 0;             // 0 - off  |   1 - green   |   2 - red
double xScale = 0;                // HX Load Scale Result

// scale average
int xStableCtr = 0;               // Stable Scale Result Checker Counter
double xScaleAverage = 0;

// sample result
double savedCount = 0;
double savedWeight = 0;

// calibrate
float calibrateResetTimer = 0;
float calibrateSetTimer = 0;



// =================
// SETUP
// =================
void setup()
{
  // SERIAL
  Serial.begin(9600);
  Serial.println("Counting Scale Start...");

  // LOAD CELL
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); //Reset the scale to 0

  // BUTTON
  pinMode(row1, OUTPUT);
  pinMode(row2, OUTPUT);
  pinMode(row3, OUTPUT);
  pinMode(row4, OUTPUT);
  pinMode(row5, OUTPUT);
  pinMode(colA, INPUT_PULLUP);
  pinMode(colB, INPUT_PULLUP);
  pinMode(colC, INPUT_PULLUP);
  pinMode(colD, INPUT_PULLUP);

  // RELAY
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(relay9, OUTPUT);

  // LED
  pinMode(alarmLedGreen, OUTPUT);
  pinMode(alarmLedRed, OUTPUT);
  pinMode(alarmLedOrange, OUTPUT);

  // RESET
  digitalWrite(latchPin, LOW);
  // 1 2 4 8  16 32 64 128
  shiftOut(dataPin, clockPin, MSBFIRST, 0);
  digitalWrite(latchPin, HIGH);
  digitalWrite(relay9 , LOW);
}


// =================
// LOOP
// =================
void loop()
{
  // ==========
  // LOAD CELL - START
  // ==========
  // scale
  xScale = scale.get_units();

  // SAMPLE
  float computedQty;
  float savedQty = savedCount;

  // Check if scale output is >1 Grams
  if (xScale > 1)
  {
    // check if finish - green
    if (xScaleFinish == 1)
    {
      digitalWrite(alarmLedGreen , HIGH);
      digitalWrite(alarmLedRed , LOW);
      digitalWrite(alarmLedOrange , LOW);
      return;
    }

    // check if finish - red
    else if (xScaleFinish == 2)
    {
      digitalWrite(alarmLedGreen , LOW);
      digitalWrite(alarmLedRed , HIGH);
      digitalWrite(alarmLedOrange , LOW);
      return;
    }

    // Computing
    else
    {
      // display - computing
      digitalWrite(alarmLedGreen , LOW);
      digitalWrite(alarmLedRed , LOW);
      digitalWrite(alarmLedOrange , LOW);

      // check if press
      if (calsSample)
      {
        // compute
        computedQty = (xScaleAverage / 5) / savedWeight;

        // check if stable
        if (xStableCtr >= 10)
        {
          // display
          if (round(computedQty) == savedQty) {
            // display - green
            xScaleFinish = 1;
          }
          else {
            // display - error
            xScaleFinish = 2;
          }
          
          Serial.print("Final Average: ");
          Serial.println((xScaleAverage / 5), 2);

          Serial.print("Final QTY: ");
          Serial.println(computedQty, 2);

          Serial.print("Final QTY Rounded: ");
          Serial.println(round(computedQty));
        }
        else
        {
          // ctr
          xStableCtr = xStableCtr + 1;
          if (xStableCtr > 5)
          {
            xScaleAverage = xScaleAverage + xScale;
          }

          Serial.print("Added #" + String(xStableCtr) + " ");
          Serial.println(xScale, 2);
        }
      }
      else
      {
        // reset
        xStableCtr = 0;
        xScaleAverage = 0;
        xScaleFinish = 0;
      }
    }
  }

  // Check if scale output is <= 0 Grams
  else
  {
    // display - waiting
    digitalWrite(alarmLedGreen , LOW);
    digitalWrite(alarmLedRed , LOW);
    digitalWrite(alarmLedOrange , HIGH);

    // reset
    xStableCtr = 0;
    xScaleAverage = 0;
    xScaleFinish = 0;
  }

  // ==========
  // BUTTON - START
  // ==========
  if (noKey == 20) { // no keys were pressed
    readKey = true;  // keyboard is ready to accept a new keypress
  }
  noKey = 0;
  for (rowCounter = row1; rowCounter < (row5 + 1); rowCounter++) {
    scanRow(); // switch on one row at a time
    for (columnCounter = colA; columnCounter < colD + 1; columnCounter++) {
      readColumn(); // read the switch pressed
      if (foundCol == true) {
        keyValue = (rowCounter - row1) + 5 * (columnCounter - colA);
      }
    }
  }
  if (readKey == true && noKey == 19) { // a key has been pressed
    convertKey(); //convert key number to keypad value
    readKey = false; // reset the flag
    delay(debounce); // debounceyuuuu
  }

  // Button # remove retain
  if (millis() > calsCurTime) {
    calsRetain = false;
  }
  // ==========
  // BUTTON - END
  // ==========
}



// ==========================
// FUNCTION: LOAD CELL - AVERAGE
// ==========================
float average (float * array, int len)  // assuming array is int.
{
  long sum = 0L ;  // sum will be larger than an item, long for safety.
  for (int i = 0 ; i < len ; i++)
    sum += array [i] ;
  return  ((float) sum) / len ;  // average will be fractional, so float may be appropriate.
}

// ==========================
// FUNCTION: BUTTON ROW
// ==========================
void scanRow() {
  for (int j = row1; j < (row5 + 1); j++) {
    digitalWrite(j, HIGH);
  }
  digitalWrite(rowCounter , LOW); // switch on one row
}

// ==========================
// FUNCTION: BUTTON COLUMN
// ==========================
void readColumn() {
  foundColumn = digitalRead(columnCounter);
  if (foundColumn == 0) {
    foundCol = true;
  }
  else {
    foundCol = false;
    noKey = noKey + 1; // counter for number of empty columns
  }
}

// ==========================
// FUNCTION: Button # Set Value
// ==========================
void pressVal(String x) {
  // check if Button # pressable
  if (!calsReady) {
    return;
  }

  // check if Button # continue
  if (calsRetain) {
    cals = cals + x;
  }
  else {
    cals = x;
  }

  // set
  calsRetain = true;
  calsCurTime = millis() + calsDebounce;
}

void pressDecimal() {
  calsReady = true;
  if (cals.indexOf(".") <= 0) {
    pressVal(".");
  }
}

void pressCE() {
  // Button Reset
  cals = "0";
  calsReady = true;
  calsRetain = false;

  // Sample Reset
  calsSample = false;
  savedCount = 0;
  savedWeight = 0;
}

void pressSample() {
  // get Button Value
  savedCount = cals.toDouble();

  // get Scale Value
  delay(1000);

  // compute
  savedWeight = xScale / savedCount;
  calsSample = true;

  // display
  Serial.println("SMP Detected Scale: " + String(xScale));
  Serial.println("SMP User Input QTY: " + String(savedCount));
  
  Serial.print("SMP Single Result: ");
  Serial.println(savedWeight, 2);
}

void pressTare() {
  scale.tare();
}

// ==========================
// FUNCTION: BUTTON CONVERT
// ==========================
void convertKey() {
  // converts the key number to the corresponding key
  keyString = "";
  keyNum = 99;
  numericValue = false;

  switch (keyValue) {
    // column A
    case 0:
      calibrateResetTimer += 1;
      calibrateSetTimer = 0;

      if (calibrateResetTimer >= 10)
      {
        keyString = "unit wt pst";
        digitalWrite(latchPin, LOW);
        // 1 2 4 8  16 32 64 128
        shiftOut(dataPin, clockPin, MSBFIRST, 76);
        digitalWrite(latchPin, HIGH);
        digitalWrite(relay9 , HIGH);
        delay(5000);
        digitalWrite(latchPin, LOW);
        // 1 2 4 8  16 32 64 128
        shiftOut(dataPin, clockPin, MSBFIRST, 0);
        digitalWrite(latchPin, HIGH);
        digitalWrite(relay9 , LOW);

        //pressUWPst();
        Serial.println("press uw pst");
      }
      break;
    case 1:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "unit wt";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 136);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);
      Serial.println("press unit wt");
      break;
    case 2:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "6";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 72);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);

      pressVal("6");
      Serial.println("press 6");
      break;
    case 3:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "5";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 40);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);

      pressVal("5");
      Serial.println("press 5");
      break;
    case 4:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "4";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 24);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);

      pressVal("4");
      Serial.println("press 4");
      break;


    // column B
    case 5:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "mc";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 4);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);
      Serial.println("press mc");
      break;
    case 6:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "tare";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 132);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);

      pressTare();
      Serial.println("press tare");
      break;
    case 7:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "ce";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 68);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);

      pressCE();
      Serial.println("press ce");
      break;
    case 8:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = ".";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 36);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);

      pressDecimal();
      Serial.println("press decimal");
      break;
    case 9:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "0";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 20);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);

      pressVal("0");
      Serial.println("press 0");
      break;

    // column C
    case 10:
      calibrateResetTimer = 0;
      calibrateSetTimer += 1;

      if (calibrateSetTimer >= 10)
      {
        keyString = "m+";
        digitalWrite(latchPin, LOW);
        // 1 2 4 8  16 32 64 128
        shiftOut(dataPin, clockPin, MSBFIRST, 44);
        digitalWrite(latchPin, HIGH);
        digitalWrite(relay9 , HIGH);
        delay(5000);
        digitalWrite(latchPin, LOW);
        // 1 2 4 8  16 32 64 128
        shiftOut(dataPin, clockPin, MSBFIRST, 0);
        digitalWrite(latchPin, HIGH);
        digitalWrite(relay9 , LOW);

        //pressMPlus();
        Serial.println("press m+");
      }
      break;
    case 11:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "z";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 130);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);
      Serial.println("press center");
      break;
    case 12:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "3";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 66);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);

      pressVal("3");
      Serial.println("press 3");
      break;
    case 13:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "2";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 34);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);

      pressVal("2");
      Serial.println("press 2");
      break;
    case 14:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "1";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 18);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);

      pressVal("1");
      Serial.println("press 1");
      break;

    // column D
    case 15:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "qty pst";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 1);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);
      Serial.println("press qty pst");
      break;
    case 16:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "sample";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 129);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);

      pressSample();
      Serial.println("press sample");
      break;
    case 17:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "9";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 65);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);

      pressVal("9");
      Serial.println("press 9");
      break;
    case 18:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "8";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 33);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);

      pressVal("8");
      Serial.println("press 8");
      break;
    case 19:
      calibrateResetTimer = 0;
      calibrateSetTimer = 0;

      keyString = "7";
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 17);
      digitalWrite(latchPin, HIGH);
      delay(100);
      digitalWrite(latchPin, LOW);
      // 1 2 4 8  16 32 64 128
      shiftOut(dataPin, clockPin, MSBFIRST, 0);
      digitalWrite(latchPin, HIGH);
      digitalWrite(relay9 , LOW);

      pressVal("7");
      Serial.println("press 7");
      break;
  }
  if (keyNum == 99) {
    numericValue = false; // show a non numeric key pressed
  }
  else {
    numericValue = true;  // show a number key pressed
  }
}
