/*******************************************************************************
Arcaduinome Arduino Sketch
- Bob Matcuk, 2014

This code eschews things like pinMode and digitalWrite for speed reasons, and,
instead, accesses the registers directly using bit-wise operators.

Atmega pin to Arduino mapping: http://arduino.cc/en/Hacking/PinMapping168
Arduino pin description: http://www.gammon.com.au/forum/?id=11473
*******************************************************************************/

#include <SPI.h>
#include <MIDI.h>
#include <midi_Defs.h>
#include <midi_Namespace.h>
#include <midi_Settings.h>

// Multiplexed "rows" are controlled via the high-side P-channel FET Array.
// Only one row may be turned on at a time. PORTD can be AND'd with
// DISABLE_ROWS_MASK to bring all row pins to LOW, and then XOR'd with the
// appropriate row enable mask to turn on one row.
#define DISABLE_ROWS_MASK B11000011   // AND'd to bring all row pins LOW
#define ROW1_ENABLE_MASK  B00000100   // enable Arduino pin 2
#define ROW2_ENABLE_MASK  B00001000   // enable Arduino pin 3
#define ROW3_ENABLE_MASK  B00010000   // enable Arduino pin 4
#define ROW4_ENABLE_MASK  B00100000   // enable Arduino pin 5

// Enable pins for the multiplexed "rows"
// These pins are connected to the 5x P-channel FET Array
// Only ONE pin should be HIGH at a time - the rest LOW
#define ROW1_ENABLE_PIN _BV(PD2)   // pin 2
#define ROW2_ENABLE_PIN _BV(PD3)   // pin 3
#define ROW3_ENABLE_PIN _BV(PD4)   // pin 4
#define ROW4_ENABLE_PIN _BV(PD5)   // pin 5

// The first shift register is connected to the MOSI and SCK SPI pins.
// The second shift register is daisy chainned with the first.

void setup()
{
  pinMode(ROW1_ENABLE_PIN, OUTPUT);
  pinMode(ROW2_ENABLE_PIN, OUTPUT);
  pinMode(ROW3_ENABLE_PIN, OUTPUT);
  pinMode(ROW4_ENABLE_PIN, OUTPUT);
  
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  
  MIDI.begin();
  MIDI.turnThruOff();
  MIDI.setHandleSystemExclusive(handleSystemExclusive);
}

void handleSystemExclusive(byte *array, byte size)
{
  
}

void loop()
{
  MIDI.read();
}
