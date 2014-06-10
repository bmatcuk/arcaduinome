/*******************************************************************************
Arcaduinome Arduino Sketch
- Bob Matcuk, 2014

This code eschews things like pinMode and digitalWrite for speed reasons, and,
instead, accesses the registers directly using bit-wise operators.

Atmega pin to Arduino mapping: http://arduino.cc/en/Hacking/PinMapping168
Arduino pin description: http://www.gammon.com.au/forum/?id=11473

TODO: buttons - http://www.ganssle.com/debouncing-pt2.htm (listing 3)
*******************************************************************************/

#include <SPI.h>
#include <MIDI.h>
#include <midi_Defs.h>
#include <midi_Namespace.h>
#include <midi_Settings.h>


// Multiplexed "rows" are controlled via the high-side P-channel FET Array.
// Only one row may be turned on at a time. Row 1 and 2 are controlled by
// Port B, pins 0 and 1. Row 3 and 4 are controlled by Port D, pins 2 and 3.
#define ROW1_ENABLE_MASK   _BV(0)   // Arduino pin 8 = Port B, pin 0
#define ROW2_ENABLE_MASK   _BV(1)   // Arduino pin 9 = Port B, pin 1
#define ROW3_ENABLE_MASK   _BV(3)   // Arduino pin 3 = Port D, pin 3
#define ROW4_ENABLE_MASK   _BV(4)   // Arduino pin 4 = Port D, pin 4
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

// Button inputs are the four high bits of Port D.
#define BUTTON1_MASK _BV(4)   // Arduino pin 4 = Port D, pin 4
#define BUTTON2_MASK _BV(5)   // Arduino pin 5 = Port D, pin 5
#define BUTTON3_MASK _BV(6)   // Arduino pin 6 = Port D, pin 6
#define BUTTON4_MASK _BV(7)   // Arduino pin 7 = Port D, pin 7

// debouncing array for buttons
#define DEBOUNCING_CHECKS 64
byte buttons[4][DEBOUNCING_CHECKS];
byte button_index = 0;


void setup()
{
  // Set "row" pins as output
  DDRB |= ROW1_ENABLE_MASK | ROW2_ENABLE_MASK;
  DDRD |= ROW3_ENABLE_MASK | ROW4_ENABLE_MASK;
  
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
  SPCR = SPI_SETTINGS;
  SPSR |= _BV(0);
  
  // Setup button input pins
  DDRD &= ~(BUTTON1_MASK | BUTTON2_MASK | BUTTON3_MASK | BUTTON4_MASK);
  
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
  
  // zero out buttons
  for (byte row = 0; row < 4; row++) {
    for (byte column = 0; column < DEBOUNCING_CHECKS; column++)
      buttons[row][column] = 0;
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
  PORTB &= ~(ROW1_ENABLE_MASK | ROW2_ENABLE_MASK);
  PORTD &= ~(ROW3_ENABLE_MASK | ROW4_ENABLE_MASK);
  
  // Strobe the SS pin (Port B, pin 2) to cause the shift registers
  // to latch on the new values. The data sheet for the shift registers
  // recommends that the latch clock stays high for at least 40ns.
  // The Arduino is operating at 8MHz which means the pin should stay
  // high for around 125ns, even if we turn if back off "immediately".
  PORTB |= _BV(2);
  PORTB &= ~_BV(2);
  
  // Enable the row - the first two rows (0 and 1) are on Port B,
  // but the second rows (2 and 3) are on Port D.
  if (row & B00000010)
    PORTD |= ROW_ENABLES[row];
  else
    PORTB |= ROW_ENABLES[row];
  
  // increment the row counter, but wrap 4 back to 0
  // also increment the iteration_count
  row = (row + 1) & B00000011;
  iteration_count++;
}
