// set up the B00 protocol transmission values
int TriggerPulse = 15000; // trigger time in microseconds
int LongPulse = 1000; // time in microseconds for a long pulse
int ShortPulse = LongPulse / 3; // time in microseconds for a short pulse

// set up the hardware pins
byte txPort = 9; // digital pin for transmitter
byte ledPort = 13; // digital pin for LED
byte sensorPin = 2;
byte sensorInterrupt = 0;  // 0 = pin 2; 1 = pin 3

// The flow sensor outputs approximately 4.5 pulses per second at one litre per minute of flow.
// This is the value we need to adjust after doing the 'real' calibration of the sensor
float calibrationFactor = 4.5;

// set up the sensor 
volatile byte sensorCount;  

float flowRate;
unsigned int litresPerMin;
unsigned int totalLitres;

unsigned long pollInterval;
unsigned long lastPollTime;
unsigned long sendInterval;
unsigned long lastSendTime;

void setup()
{
  sensorCount = 0;
  flowRate = 0.0;
  litresPerMin = 0;
  totalLitres = 0;
  lastPollTime = 0;
  pollInterval = 3000; // 1 second poll interval

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
  if((millis() - lastPollTime) > pollInterval)    // Only process counters once per second
  { 
    // Disable the interrupt while calculating flow rate and sending data
    detachInterrupt(sensorInterrupt);
    
    // calculate flow rate in litres per minute using the predefined calibrationFactor
    flowRate = ((1000.0 / (millis() - lastPollTime)) * sensorCount) / calibrationFactor;
    lastPollTime = millis();
    
    litresPerMin = flowRate;
    totalLitres += litresPerMin;
    
    // send: content, house, channel, value A, value B
    sendB00Packet(0, 1, 2, litresPerMin, totalLitres);  
    
    // blink led
    digitalWrite(ledPort, HIGH);
    delay(50);   
    digitalWrite(ledPort, LOW);
    
    // Reset the sensor counter
    sensorCount = 0;
    
    // Enable the interrupt again now that we've finished sending output
    attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
  }
}

void pulseCounter()
{
  sensorCount++;
}

void sendB00Packet(int content, int house, int channel, int valueA, int valueB)
{
  int repeats = 2;
  
  sendB00Trigger();
  for(int i = 0; i < repeats; i++)
  {
    sendB00Bits(0xB, 4); // announce!
    sendB00Bits(content, 8); // default 00
    sendB00Bits(house, 2);
    sendB00Bits(channel, 2);
    sendB00Bits(valueA, 16);
    sendB00Bits(valueB, 16);
    sendB00Trigger();
  }
}

void sendB00Trigger()
{
  digitalWrite(txPort, HIGH);
  delayMicroseconds(TriggerPulse);   
  digitalWrite(txPort, LOW);
  delayMicroseconds(TriggerPulse);    
}

void sendB00Bits(unsigned int data, int bits)
{
  unsigned int bitMask = 1;
  bitMask = bitMask << (bits - 1);
  for(int i = 0; i < bits; i++)
  {
    sendB00Bit( (data&bitMask) == 0 ? 0 : 1);
    bitMask = bitMask >> 1;
  }
}

void sendB00Bit(byte b)
{
  if(b == 0)
  {
    digitalWrite(txPort, HIGH);
    delayMicroseconds(ShortPulse);
    digitalWrite(txPort, LOW);
    delayMicroseconds(LongPulse);  
  }
  else // any bit in the byte is on
  {
    digitalWrite(txPort, HIGH);
    delayMicroseconds(LongPulse);
    digitalWrite(txPort, LOW);
    delayMicroseconds(ShortPulse);  
  }
}

