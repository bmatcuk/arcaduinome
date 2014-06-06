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
// DISABLE_ROWS_MASK to bring all row pins to LOW, and then OR'd with the
// appropriate row enable mask to turn on one row.
#define DISABLE_ROWS_MASK B11000011   // AND'd to bring all row pins LOW
#define ROW1_ENABLE_MASK  B00000100   // enable Arduino pin 2
#define ROW2_ENABLE_MASK  B00001000   // enable Arduino pin 3
#define ROW3_ENABLE_MASK  B00010000   // enable Arduino pin 4
#define ROW4_ENABLE_MASK  B00100000   // enable Arduino pin 5
const byte ROW_ENABLES[] = {
  ROW1_ENABLE_MASK,
  ROW2_ENABLE_MASK,
  ROW3_ENABLE_MASK,
  ROW4_ENABLE_MASK
};

// LED array - 4 rows by 12 columns arranged in RGB order
// ie: {{R1, G1, B1, R2, G2, B2, R3, G3, B3, R4, G4, B4}, {...}, ...}
byte leds[4][12];


void setup()
{
  // Set "row" pins as output
  DDRD = ROW1_ENABLE_MASK | ROW2_ENABLE_MASK | ROW3_ENABLE_MASK | ROW4_ENABLE_MASK;
  
  // Setup SPI to communicate with the shift registers for "columns"
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  
  // Enable MIDI
  MIDI.begin();
  MIDI.turnThruOff();
  MIDI.setHandleSystemExclusive(handleSystemExclusive);
  
  // zero out leds
  for (byte row = 0; row < 4; row++) {
    for (byte column = 0; column < 12; column++)
      leds[row][column] = 0;
  }
}

void handleSystemExclusive(byte *array, byte size)
{
  // TODO
}

void loop()
{
  static byte row = 0, iteration_count = 0;
  
  // read MIDI data
  MIDI.read();
  
  // enable the current row
  PORTD = (PORTD & DISABLE_ROWS_MASK) | ROW_ENABLES[row];
  
  // enable columns
  word column_data = 0;
  for (byte column = 0; column < 12; column++) {
    if (leds[row][column] >= iteration_count)
      column_data |= 1 << column;
  }
  SPI.transfer(column_data >> 8);
  SPI.transfer(column_data & 0xff);
  
  // increment the row counter, but wrap 4 back to 0
  // also increment the iteration_count
  row = (row + 1) & B00000011;
  iteration_count++;
}
