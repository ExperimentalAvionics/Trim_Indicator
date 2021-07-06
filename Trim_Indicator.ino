// Trim position indicator
// 
// Center LED - Green - D7 - Always on
// Trim is Neutral (Zero) when only the Green LED is ON
// Top and Bottom LEDs are Red indicating reaching the trims limits
// When trim motor is activated the relevant Red LED is flashing
// LED display is not exactly linear. See comments in the DisplayTrim() function for details.
//

#include <EEPROM.h>

// Moving Average array
#define PositionArraySize 10 // averaging accross last 10 values
long PositionArray[PositionArraySize];
int PositionArrayIndex = 0;
long PositionArraySum =0;

int i = 0; // counter
//float Alpha = 0;
unsigned long currentMillis;
long TrimCurrent = 512;  // Current position of the trim. Normalized
long TrimMax     = 1023; // Max position of the trim (Nose Down). Read from EEPROM. Normalized.
long TrimZero    = 512;  // Neutral position of the trim. Read from EEPROM. Normalized.
long TrimMin     = 0;    // Min position of the trim (Nose Up). Read from EEPROM. Normalized.
long RefVoltage  = 0;
long TrimDown    = 0;    // If > 0 then Trim Down button is activated
long TrimUp      = 0;    // If > 0 then Trim Up button is activated
unsigned long LedTimer    = 0;

// Inputs:
int TrimSensorPin  = A2; // Measuring voltage on the trim position sensor (resistive voltage divider)
int TrimDownPin    = A0; // Trim Down is activated
int TrimUpPin      = A4; // Trim Up is activated
int PowerSensorPin = A5; // Main Power (reference) voltage sensor - Main power is not axactly 12v. it might fluctuate +/- 3..5v Co all the positioner measurment should be relative to that

int EEPROM_Offset = 0;

void setup() 
{
   Serial.begin(115200);

   analogReference(INTERNAL);

   delay(20);

// init the pins 2-11
  for (int i = 2; i <= 11; i++) {
    pinMode(i, OUTPUT);  
    delay(10);
  } 

  pinMode(A3, INPUT_PULLUP);  // convert A3 input into Digital with pullup - this is calibration button


// check if calibration is initiated
// calibration starts when the pins connected to A3 are shorted (A3 is grounded)
// calibration required to set Zero Trim point (Trim for takeoff)
// it might not be neceseraly in the midle of the trim servo position
   if (digitalRead(A3) == LOW) {
      // light up all the LEDs to signify calibration mode
      for (int i = 2; i <= 11; i++) {
        digitalWrite(i, HIGH); 
      }
        // get 100 readings and average them
        TrimZero = 0;
        for (int i = 1; i <= 100; i++) {
          TrimCurrent = analogRead(TrimSensorPin);
          RefVoltage = analogRead(PowerSensorPin);
          TrimZero = TrimZero + (1024 * TrimCurrent / RefVoltage);
        }
        TrimZero = TrimZero/100;

        Serial.print("TrimZero = ");
        Serial.println(TrimZero);
        
        EEPROM.put(EEPROM_Offset + 0, TrimMax);  // for the future
        EEPROM.put(EEPROM_Offset + 4, TrimZero);
        EEPROM.put(EEPROM_Offset + 8, TrimMin);  // for the future
        
        delay(2000);
        for (int i = 2; i <= 11; i++) {
          digitalWrite(i, LOW); 
        }
        
        digitalWrite(11, HIGH);
        digitalWrite(3, HIGH);  
        
        while (digitalRead(A3) == LOW) { //wait until the calibration shunt removed
          delay(20);
        }
   }


   EEPROM.get(EEPROM_Offset + 0, TrimMax);
   EEPROM.get(EEPROM_Offset + 4, TrimZero);
   EEPROM.get(EEPROM_Offset + 8, TrimMin);

   if (TrimZero == -1 or TrimZero > 1024) {
    // The indicator is not calibrated. Assign default values
      TrimZero = 512;
      TrimMax = 1023;
      TrimMin = 0;
   }

  delay(1000);

  for (int i = 2; i <= 11; i++) {
    digitalWrite(i, HIGH); 
    delay(200);
  }
  
  for (int i = 2; i <= 11; i++) {
    digitalWrite(i, LOW); 
    delay(200);
  }
  
  digitalWrite(7, HIGH);
}

void loop() {

  TrimCurrent = analogRead(TrimSensorPin);
  RefVoltage = analogRead(PowerSensorPin);
//  Serial.print("Position sensor reading = ");
//  Serial.println(TrimCurrent);
//  Serial.print("Reference voltage = ");
//  Serial.println(RefVoltage);

// normalise the TrimCurrent relative to the main voltage
  TrimCurrent = 1024 * TrimCurrent / RefVoltage;
//  Serial.print("Normalised position = ");
//  Serial.println(TrimCurrent);

// Averaging
  PositionArraySum -= PositionArray[PositionArrayIndex];  
  PositionArray[PositionArrayIndex] = TrimCurrent;
 
  PositionArraySum += PositionArray[PositionArrayIndex];
  
  PositionArrayIndex +=1; // shift the index for next time
  if (PositionArrayIndex == PositionArraySize) {  // if we reached the top of the array
    PositionArrayIndex = 0;                       //go to the start of the array
  }
 
  TrimCurrent = PositionArraySum / PositionArraySize;

  DisplayTrim();

// Display up/Down button activation
// if the button/server is activated - flash the related Red LED (top or bottom)
   TrimDown = analogRead(TrimDownPin);
   TrimUp = analogRead(TrimUpPin);

   FlashRedLeds();

}

void FlashRedLeds () {
  currentMillis = millis();
  if (TrimDown > 50) {      // lift the sensitivity well above noise floor to avoid false indication of trim motor activation
    if (currentMillis - LedTimer > 200) {
      digitalWrite(11, !digitalRead(11));
      LedTimer = currentMillis;
    }
  }
  if (TrimUp > 50) {        // lift the sensitivity well above noise floor to avoid false indication of trim motor activation
    if (currentMillis - LedTimer > 200) {
      digitalWrite(3, !digitalRead(3));
      LedTimer = currentMillis;
    }
  }
}

void DisplayTrim() {

long LEDpos = 0; //pesentage of LED bar to light up relative to the Green LED  

// convert the ADC reading into LED position

// Center LED remain the only LED ON within 10% or TrimZero setting
// First Blue band 10..40%
// Second Blue band 40..70%
// Red - 70..100%

if (TrimCurrent > TrimZero) {
   LEDpos = (TrimCurrent-TrimZero)*100/(TrimMax-TrimZero);
} else {
   LEDpos = (TrimCurrent-TrimZero)*100/(TrimZero-TrimMin);
}


// Top to bottom

  // RED - Trim Down
  if (LEDpos >70 and LEDpos <= 100) {
     if (TrimDown == 0) { // leave the LED alone if Trim Down button is activated
        digitalWrite(11, HIGH);
     }
     digitalWrite(9, HIGH);
     digitalWrite(8, HIGH);  
     digitalWrite(6, LOW);
     digitalWrite(5, LOW);
     if (TrimUp == 0) { // leave the LED alone if Trim Up button is activated
       digitalWrite(3, LOW);
     }
  } 
  
  // Second Blue - Trim Down
  if (LEDpos > 40 and LEDpos <= 70) {
     if (TrimDown == 0) { // leave the LED alone if Trim Down button is activated
       digitalWrite(11, LOW);
     }
     digitalWrite(9, HIGH);
     digitalWrite(8, HIGH);  
     digitalWrite(6, LOW);
     digitalWrite(5, LOW);
     if (TrimUp == 0) { // leave the LED alone if Trim Up button is activated
       digitalWrite(3, LOW);
     }
  } 
  
  // First Blue - Trim Down
  if (LEDpos > 10 and LEDpos <= 40) {   // Adjust precision of the neutral trim position in this condition
     if (TrimDown == 0) { // leave the LED alone if Trim Down button is activated
       digitalWrite(11, LOW);
     }
     digitalWrite(9, LOW);
     digitalWrite(8, HIGH);  
     digitalWrite(6, LOW);
     digitalWrite(5, LOW);
     if (TrimUp == 0) { // leave the LED alone if Trim Up button is activated
       digitalWrite(3, LOW);
     }
  } 

    // Green - Trim in the middle
  if (LEDpos > -10 and LEDpos <= 10) {   // Adjust precision of the neutral trim position in this condition
     if (TrimDown == 0) { // leave the LED alone if Trim Down button is activated
       digitalWrite(11, LOW);
     }
     digitalWrite(9, LOW);
     digitalWrite(8, LOW);  
     digitalWrite(6, LOW);
     digitalWrite(5, LOW);
     if (TrimUp == 0) { // leave the LED alone if Trim Up button is activated
       digitalWrite(3, LOW);
     }
  } 
  
  // First Blue - Trim Up
  if (LEDpos >= -40 and LEDpos < -10) {  // Adjust precision of the neutral trim position in this condition
     if (TrimDown == 0) { // leave the LED alone if Trim Down button is activated
       digitalWrite(11, LOW);
     }
     digitalWrite(9, LOW);
     digitalWrite(8, LOW);  
     digitalWrite(6, HIGH);
     digitalWrite(5, LOW);
     if (TrimUp == 0) { // leave the LED alone if Trim Up button is activated
       digitalWrite(3, LOW);
     }
  } 
  
  // Second Blue - Trim Up
  if (LEDpos >= -70 and LEDpos < -40) {
     if (TrimDown == 0) { // leave the LED alone if Trim Down button is activated
       digitalWrite(11, LOW);
     }
     digitalWrite(9, LOW);
     digitalWrite(8, LOW);  
     digitalWrite(6, HIGH);
     digitalWrite(5, HIGH);
     if (TrimUp == 0) { // leave the LED alone if Trim Up button is activated
       digitalWrite(3, LOW);
     }
  }

  // Red - Trim Up
  if (LEDpos >= -100 and LEDpos < -70) {
     if (TrimDown == 0) { // leave the LED alone if Trim Down button is activated
       digitalWrite(11, LOW);
     }
     digitalWrite(9, LOW);
     digitalWrite(8, LOW);  
     digitalWrite(6, HIGH);
     digitalWrite(5, HIGH);
     if (TrimUp == 0) { // leave the LED alone if Trim Up button is activated
       digitalWrite(3, HIGH);
     }
  } 

  
}
