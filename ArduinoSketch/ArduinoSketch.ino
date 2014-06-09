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
// appropriate row enable mask to turn on one row. DDRD can be OR'd with
// ENABLE_ROW_OUTPUTS to set the row pins as OUTPUT pins.
#define ROW1_ENABLE_MASK   _BV(2)   // Arduino pin 2 = Port D, pin 2
#define ROW2_ENABLE_MASK   _BV(3)   // Arduino pin 3 = Port D, pin 3
#define ROW3_ENABLE_MASK   _BV(4)   // Arduino pin 4 = Port D, pin 4
#define ROW4_ENABLE_MASK   _BV(5)   // Arduino pin 5 = Port D, pin 5
#define ENABLE_ROW_OUTPUTS (ROW1_ENABLE_MASK | ROW2_ENABLE_MASK | ROW3_ENABLE_MASK | ROW4_ENABLE_MASK)
#define DISABLE_ROWS_MASK  ~ENABLE_ROW_OUTPUTS
const byte ROW_ENABLES[] = {
  ROW1_ENABLE_MASK,
  ROW2_ENABLE_MASK,
  ROW3_ENABLE_MASK,
  ROW4_ENABLE_MASK
};

// SPI settings for the SPI Control Register:
// 0  = disable SPI Interupt
// 1  = enable SPI
// 1  = transfer LSB first
// 1  = master mode
// 0  = idle clock is low and pulses high
// 0  = sample on the leading edge of the clock
// 00 = speed is clock/4 (set LSB of SPSR to a 1 to double speed)
#define SPI_SETTINGS B01110000

// SPI output pins - SCK, MOSI, and SS
// SCK  = Arduino pin 13 = Port B, pin 5
// MOSI = Arduino pin 11 = Port B, pin 3
// SS   = Arduino pin 10 = Port B, pin 2
#define SPI_OUTPUT_PINS (_BV(5) | _BV(3) | _BV(2))

// LED array - 4 rows by 12 columns arranged in RGB order
// ie: {{R1, G1, B1, R2, G2, B2, R3, G3, B3, R4, G4, B4}, {...}, ...}
byte leds[4][12];


void setup()
{
  // Set "row" pins as output
  DDRD |= ENABLE_ROW_OUTPUTS;
  
  // Set SPI pins to output and bring SS (Port B, pin 2) low
  DDRB |= SPI_OUTPUT_PINS;
  PORTB &= ~_BV(2);
  
  // Setup SPI to communicate with the shift registers for "columns"
  // The STPIC6D595B1R shift registers recommend a clock duration of at least
  // 40ns = 25MHz. The fastest SPI clock on the Arduino is clock/2 = 8MHz.
  // To achieve clock/2, we must also set the LSB of SPSR to 1 to enable
  // "double speed". Ideas stolen from SPI library source:
  // https://github.com/arduino/Arduino/blob/master/libraries/SPI/SPI.cpp
  // https://github.com/arduino/Arduino/blob/master/libraries/SPI/SPI.h
  SRCR = SPI_SETTINGS;
  SRSR |= _BV(0);
  
  // Enable MIDI
  MIDI.begin();
  MIDI.turnThruOff();
  MIDI.setHandleSystemExclusive(handleSystemExclusive);
  
  // zero out leds
  //for (byte row = 0; row < 4; row++) {
  //  for (byte column = 0; column < 12; column++)
  //    leds[row][column] = 0;
  //}
  
  // TODO: remove!
  // for testing, let's give the LED some colors!
  for (byte row = 0; row < 4; row++) {
    for (byte column = 0; column < 4; column++) {
      leds[row][column*3]   = min(256 - row * 64, 255);      // R
      leds[row][column*3+1] = min(256 - column * 64, 255);   // G
      leds[row][column*3+2] = 255;                           // B
    }
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
  
  // Shift first byte of column data into the shift registers
  byte column_data = 0, column = 0;
  for (; column < 8; column++) {
    if (leds[row][column] > iteration_count)
      column_data |= 1 << column;
  }
  SPDR = column_data;
  
  // build second byte while the first one is transferring
  column_data = 0;
  for (byte column_bit = 1; column < 12; column++, column_bit <<= 1) {
    if (leds[row][column] > iteration_count)
      column_data |= column_bit;
  }
  
  // Wait for first byte to finish (do we need this?) then start
  // sending the second byte of column data. SPI Flag is bit 7 in
  // the status register. Wait for second byte to finish as well.
  while (!(SPSR & _BV(7)));
  SPDR = column_data;
  while (!(SPSR & _BV(7)));
  
  // Disable all rows
  PORTD &= DISABLE_ROWS_MASK;
  
  // Strobe the SS pin (Port B, pin 2) to cause the shift registers
  // to latch on the new values. The data sheet for the shift registers
  // recommends that the latch clock stays high for at least 40ns.
  // The Arduino is operating at 8MHz which means the pin should stay
  // high for around 125ns, even if we turn if back off "immediately".
  PORTB |= _BV(2);
  PORTB &= ~_BV(2);
  
  // Enable the row
  PORTD |= ROW_ENABLES[row];
  
  // increment the row counter, but wrap 4 back to 0
  // also increment the iteration_count
  row = (row + 1) & B00000011;
  iteration_count++;
}
