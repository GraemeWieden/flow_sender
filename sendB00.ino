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

