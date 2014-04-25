#include <EEPROM.h>
#include <LiquidCrystal.h>

// Connections:
// VSS (LCD pin 1) to ground
// VDD (LCD pin 2) to +5V
// V0 (LCD pin 3) to ground via 10K resistor - LCD display contrast 
// RS (LCD pin 4) to Arduino pin 8
// RW (LCD pin 5) to ground
// E (LCD pin 6) to Arduino pin 9
// D4, D5, D6, D7 (LCD pins 11, 12, 13, 14) to Arduino pins 4, 5, 6, 7
// A (LCD pin 15) to Arduino pin +5V via resistor if necessary - LCD backlight brightness
// K (LCD pin 16) to ground

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// set up the B00 protocol transmission values
int TriggerPulse = 15000; // trigger time in microseconds
int LongPulse = 1000; // time in microseconds for a long pulse
int ShortPulse = LongPulse / 3; // time in microseconds for a short pulse

// set up the hardware pins
byte txPort = 3; // digital pin for transmitter
byte ledPort = 13; // digital pin for LED
byte sensorPin = 2;
byte sensorInterrupt = 0;  // 0 = pin 2; 1 = pin 3
byte buttonPin = A0; // analog pin to which the reset button is attached

// The flow sensor outputs approximately 4.5 pulses per second at one litre per minute of flow.
// This is the value we need to adjust after doing the 'real' calibration of the sensor
float calibrationFactor = 3.97;

// set up the sensor 
volatile byte sensorCount;  

float flowRate;
unsigned int mLPerMin;
float totalLitres;
char buffer[10];

unsigned long pollInterval;
unsigned long lastPollTime;
unsigned long sendInterval;
unsigned long lastSendTime;

unsigned int resetCount;

void setup()
{
  // if there is data in the EEPROM, then a value for totalLitres has been stored so read it back in
  totalLitres = readFloat();
  if (isnan(totalLitres))
    totalLitres = 0;
    
  // setup LCD and display version info
  lcd.begin(16,2);
  lcd.clear();
  lcdPrint(0, 0, "Flow Monitor");
  lcdPrint(0, 1, "Version 1.0");
  delay(250);   
  
  // setup the reset button - we'll treat any of the buttons as a reset button
  pinMode(buttonPin, INPUT);         //ensure button pin is an input
  digitalWrite(buttonPin, LOW);      //ensure pullup is off on button pin
  resetCount = 0;                    // incremented while button is held;
  
  sensorCount = 0;
  flowRate = 0.0;
  mLPerMin = 0;
  // totalLitres = 0;
  lastPollTime = 0;
  pollInterval = 500; // half second poll interval

  // these aren't in use yet. soon i'll change it so that the calculations are done each second 
  // but we'll only transmit every 10 or so seconds.
  // we could also ease the rf traffic by only sending when the values actually change.
  lastSendTime = 0;
  sendInterval = 10000; // 10 second send interval
  
  pinMode(sensorPin, INPUT);
  digitalWrite(sensorPin, HIGH);
  attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
}

void loop()
{ 
  if((millis() - lastPollTime) > pollInterval)    // Only process counters once per specified interval
  { 
    // Disable the interrupt while calculating flow rate and sending data
    detachInterrupt(sensorInterrupt);
    
    // Check if the reset button is being held down
    if (analogRead(buttonPin) < 1000)
    {
      lcdPrint(0, 0, " HOLD TO RESET  ");
      lcdPrint(0, 1, "  TOTAL USAGE   ");
      resetCount++;
      if(resetCount > 5) // trigger reset
      {
        lcdPrint(0, 0, "  TOTAL USAGE   ");
        lcdPrint(0, 1, "     RESET      ");
        totalLitres = 0;
        storeFloat(totalLitres);
        delay(1000);   
      }
    }
    else
    {
      resetCount = 0;
    }
    
    // calculate flow rate in litres per minute using the predefined calibrationFactor
    flowRate = ((1000.0 / (millis() - lastPollTime)) * sensorCount) / calibrationFactor;
    lastPollTime = millis();
    
    mLPerMin = flowRate * 1000;
    totalLitres += (flowRate / (60000.0 / (float)pollInterval));

    // display data on LCD screen if not holding the reset button
    if(resetCount == 0)
    {
      dtostrf(flowRate, 5 , 3 , buffer);
      lcdPrint(0, 0, "F: " + String(buffer) + " L/min      ");
      dtostrf(totalLitres, 5 ,3 , buffer);
      lcdPrint(0, 1, "T: " + String(buffer) + " L          ");
    }
     
    if((millis() - lastSendTime) > sendInterval)    // transmit the data at the specified interval 
    { 
      lastSendTime = millis();
      // we're going to send the number of litres / 10 as that allows us to send a value 
      // up to about 655 kilolitres with a precision of 10 litres.
      
      // send: content, house, channel, value A, value B
      // sendB00Packet(0, 1, 2, mLPerMin, (unsigned int)(totalLitres / 10));  // send tens of litres
      sendB00Packet(0, 1, 2, mLPerMin, totalLitres); // send litres 
      
      // note that we should write to EEPROM sparingly as the memory is only guaranteed for 100,000 writes
      // so at 10 second intervals, that's only a few weeks.
      storeFloat(totalLitres);
          
      // blink the led
      digitalWrite(ledPort, HIGH);
      delay(30);   
      digitalWrite(ledPort, LOW);
    }
    
    // Reset the sensor counter
    sensorCount = 40;
    
    // Enable the interrupt again now that we've finished sending output
    attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
  }
}

void pulseCounter()
{
  sensorCount++;
}

void lcdPrint(int x, int y, String text)
{
  lcd.setCursor(x, y);
  lcd.print( text );
}

void storeFloat(float value)
{
    byte* p = (byte*)(void*)&value;
    for (int i = 0; i < sizeof(value); i++)
        EEPROM.write(i, *p++);
}

float readFloat()
{
    float value = 0.0;
    byte* p = (byte*)(void*)&value;
    for (int i = 0; i < sizeof(value); i++)
        *p++ = EEPROM.read(i);
    return value;
}
