#include <B00Sender.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>

// construct the B00Sender object with default settings
B00Sender sender;

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

// construct the LiquidCrystal object using desired pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// set up the hardware pins
byte txPin = 3; // digital pin for transmitter
byte ledPin = 13; // digital pin for LED
byte sensorPin = 2;
byte sensorInterrupt = 0;  // 0 = pin 2; 1 = pin 3
byte buttonPin = A0; // analog pin to which the reset button is attached

// The flow sensor outputs approximately 4 pulses per second at one litre per minute of flow.
// This value will need to be modified for the particular flow sensor in use
float calibrationFactor = 4.0;

// set the house and channel codes here
int houseCode = 0; // 0 to 3
int channelCode = 0; // 0 to 7

// set up the sensor 
volatile byte sensorCount;  

float flowRate;
unsigned int mLPerMin;
float totalLitres;
float lastSentLitres;
float lastPersistLitres;

// buffer for strings
char buffer[10];

// timing variables
unsigned long pollInterval;
unsigned long lastPollTime;
unsigned long sendInterval;
unsigned long heartbeatInterval;
unsigned long lastSendTime;
unsigned long persistInterval;
unsigned long lastPersistTime;

unsigned int resetCount;

void setup()
{
  sender.setup(txPin, houseCode, channelCode);

  // if there is data in the EEPROM, then a value for totalLitres has been stored so read it back in
  totalLitres = readFloat();
  if (isnan(totalLitres))
    totalLitres = 0;
    
  // setup LCD and display version info
  lcd.begin(16,2);
  lcd.clear();
  lcdPrint(0, 0, "Flow Monitor");
  lcdPrint(0, 1, "Version 1.1");
  delay(500);   
  
  // setup the internal LED for output to show when transmissions or EEPROM writes happen
  pinMode(ledPin, OUTPUT);  
    
  // setup the reset button - we'll treat any of the buttons as a reset button
  pinMode(buttonPin, INPUT);         //ensure button pin is an input
  digitalWrite(buttonPin, LOW);      //ensure pullup is off on button pin
  resetCount = 0;                    // incremented while button is held;
  
  sensorCount = 0;
  flowRate = 0.0;
  mLPerMin = 0;

  pollInterval = 500; // half second poll interval
  lastPollTime = 0;

  sendInterval = 10000; // 10 second send interval
  heartbeatInterval = 120000; // 2 minute heartbeat interval
  lastSendTime = 0;
  lastSentLitres = 0; // keep track so we only send if total has changed

  persistInterval = 120000; // 2 minute persist interval
  lastPersistTime = 0;
  lastPersistLitres = 0; // keep track so we only persist if total has changed
  
  pinMode(sensorPin, INPUT);
  digitalWrite(sensorPin, HIGH);
  attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
}

void loop()
{ 
  if((millis() - lastPollTime) > pollInterval)    // Only process counters once per specified interval
  { 
    // Disable the interrupt while calculating flow rate
    detachInterrupt(sensorInterrupt);
    
    // calculate flow rate in litres per minute using the predefined calibrationFactor
    flowRate = ((1000.0 / (millis() - lastPollTime)) * sensorCount) / calibrationFactor;
    lastPollTime = millis();
    // Reset the sensor counter
    // sensorCount = 40; // set for debugging
    sensorCount = 0;
    
    // Enable the interrupt again
    attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
    
    mLPerMin = flowRate * 1000;
    totalLitres += (flowRate / (60000.0 / (float)pollInterval));
    
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
        lastSentLitres = 0;
        lastPersistLitres = 0;
        
        storeFloat(totalLitres);
        delay(1000);   
      }
    }
    else
    {
      resetCount = 0;
    }

    // display data on LCD screen if not holding the reset button
    if(resetCount == 0)
    {
      dtostrf(flowRate, 5 , 3 , buffer);
      lcdPrint(0, 0, "F: " + String(buffer) + " L/min      ");
      dtostrf(totalLitres, 5 ,3 , buffer);
      lcdPrint(0, 1, "T: " + String(buffer) + " L          ");
    }
    
    // transmit the data at the specified interval if the totalLitres has changed by more than 0.1 litre
    if(  (millis() - lastSendTime) > heartbeatInterval || ((millis() - lastSendTime) > sendInterval && (totalLitres - lastSentLitres) > 0.1))    
    { 
      lastSendTime = millis();
      lastSentLitres = totalLitres;

      // this option is for wrapping up both values into a single transmission
      // sender.sendB04(mLPerMin, (unsigned int)(totalLitres / 10));
      // sending number of litres / 10 allows us to send a value up to about 655 kilolitres with a precision of 10 litres.

      // preferred option, to send the data as two separate values
      sender.sendB00(flowRate); // floating point flow rate in L/min
      sender.sendB02(totalLitres); // unsigned long total value in litres

      blinkLed();
    }
    // persist the total usage to EEPROM at the specified interval if the totalLitres has changed by more than 1 litre
    if((millis() - lastPersistTime) > persistInterval && (totalLitres - lastPersistLitres) > 1)    
    { 
      lastPersistTime = millis();
      lastPersistLitres = totalLitres;
      // write to EEPROM sparingly as memory is only guaranteed for 100,000 writes (10 second interval is only a few weeks)
      storeFloat(totalLitres);
      // blink the led twice
      blinkLed();
      delay(30);   
      blinkLed();
    }
  }
}

void pulseCounter()
{
  sensorCount++;
}

void blinkLed()
{
  digitalWrite(ledPin, HIGH);
  delay(30);   
  digitalWrite(ledPin, LOW);
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
