/*******************************************************************************
Arcaduinome Arduino Sketch
- Bob Matcuk, 2014

This code eschews things like pinMode and digitalWrite for speed reasons, and,
instead, accesses the registers directly using bit-wise operators. It's not
meant to look pretty; it's meant to handle the LEDs as quickly as possible,
since the MIDI stuff is much more important. Hopefully it's documented well
enough that you can follow along! =)

Atmega pin to Arduino mapping: http://arduino.cc/en/Hacking/PinMapping168
Arduino pin description: http://www.gammon.com.au/forum/?id=11473

TODO: buttons - http://www.ganssle.com/debouncing-pt2.htm (listing 3)
*******************************************************************************/

#include <EEPROM.h>
#include <MIDI.h>
#include <midi_Defs.h>
#include <midi_Namespace.h>
#include <midi_Settings.h>

// if defined, disables MIDI and just prints stuff to serial
//#define DEBUG 1

// The MIDI manufacturer ID is used to determine if a sysex message is
// destined for us. 0x7D is specifically reserved for "educational use", so it
// shouldn't be used by any commercial product. However, if you find that this
// is causing some problems with some other MIDI equipment, you might need to
// change this. The code does NOT support extended Manufacturer IDs (the MIDI
// spec includes support for an ID of 0x00 which means that the next two bytes
// are the actual ID), but it could be added...
#define MIDI_MANUFACTURER_ID 0x7D

// Sysex message IDs
#define SYSEX_SET_ALL_LEDS 0x00
#define SYSEX_SET_LED      0x01
#define SYSEX_SAVE_LEDS    0x02
#define SYSEX_RESET_LEDS   0x03

#define SYSEX_SET_ALL_BUTTONS 0x10
#define SYSEX_SET_BUTTON      0x11
#define SYSEX_SAVE_BUTTONS    0x12
#define SYSEX_RESET_BUTTONS   0x13

// Offsets in EEPROM for LED and Button settings
#define EEPROM_LED_OFFSET    0x0000
#define EEPROM_BUTTON_OFFSET (EEPROM_LED_OFFSET + 4 * 12)

// Multiplexed "rows" are controlled via the high-side P-channel FET Array.
// Only one row may be turned on at a time. Row 1 and 2 are controlled by
// Port B, pins 0 and 1. Row 3 and 4 are controlled by Port D, pins 2 and 3.
// To turn "on" a row, the corresponding pin must be brought LOW. Therefore,
// ROW_ENABLES has negated the masks already so they can be AND'd.
#define ROW1_ENABLE_MASK _BV(0)   // Arduino pin 8 = Port B, pin 0
#define ROW2_ENABLE_MASK _BV(1)   // Arduino pin 9 = Port B, pin 1
#define ROW3_ENABLE_MASK _BV(2)   // Arduino pin 2 = Port D, pin 2
#define ROW4_ENABLE_MASK _BV(3)   // Arduino pin 3 = Port D, pin 3
const byte ROW_ENABLES[] = {
  ~ROW1_ENABLE_MASK,
  ~ROW2_ENABLE_MASK,
  ~ROW3_ENABLE_MASK,
  ~ROW4_ENABLE_MASK
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
#define BUTTON_MASK  (BUTTON1_MASK | BUTTON2_MASK | BUTTON3_MASK | BUTTON4_MASK)
const byte BUTTON_MASKS[] = {
  BUTTON1_MASK,
  BUTTON2_MASK,
  BUTTON3_MASK,
  BUTTON4_MASK
};

// Debouncing array for buttons: the first dimension of the array represents
// rows. The second is a circular queue representing the last X many readings
// from Port D (where the buttons are - see above). X is DEBOUNCING_CHECKS
// and MUST be a power of 2. If all of these readings are identical, that
// means the user really did press (or release) the button. button_index
// holds the current index into the circular queue. Once it hits the maximum
// (DEBOUNCING_CHECKS), it will wrap back to zero.
#define DEBOUNCING_CHECKS 16
byte button_debounce[4][DEBOUNCING_CHECKS];
byte button_index = 0;

// We also keep track of the current state of the buttons... The cooresponding
// bit will be a 1 if we have already decided the button was pressed and sent
// the cooresponding "NoteOn" command. It'll be a zero if we think the button
// hasn't been pressed.
byte button_state[4];

// This array holds the note that a button should make when it is pressed.
byte buttons[4][4];


void setup()
{
  // Set "row" pins as output and turn them all off for now (by setting the
  // pins high). We set the pins high first, which will engage the internal
  // pull-up resistors since they're still in "input" mode. Externally, we
  // have more pull up resistors, so nothing will really happen. Then when
  // we switch to output mode, the pins will already be pulled high so more
  // nothing will happen - exactly as we planned =)
  PORTB |= ROW1_ENABLE_MASK | ROW2_ENABLE_MASK;
  DDRB |= ROW1_ENABLE_MASK | ROW2_ENABLE_MASK;
  PORTD |= ROW3_ENABLE_MASK | ROW4_ENABLE_MASK;
  DDRD |= ROW3_ENABLE_MASK | ROW4_ENABLE_MASK;
  
  // Set SPI pins to output and bring SS (Port B, pin 2) low
  DDRB |= SPI_OUTPUT_PINS;
  PORTB &= ~_BV(2);
  
  // Setup SPI to communicate with the shift registers for "columns"
  // The STPIC6D595B1R shift registers recommend a clock duration of at least
  // 40ns = 25MHz. The fastest SPI clock on the Arduino is clock/2 = 8MHz,
  // which is slower than the quickest clock the shift registers can handle.
  // To achieve clock/2, we must also set the LSB of SPSR to 1 to enable
  // "double speed". Ideas stolen from SPI library source:
  // https://github.com/arduino/Arduino/blob/master/libraries/SPI/SPI.cpp
  // https://github.com/arduino/Arduino/blob/master/libraries/SPI/SPI.h
  SPCR = SPI_SETTINGS;
  SPSR |= _BV(0);
  
  // Setup button input pins - shouldn't really need to do this, since
  // input is default... but for completeness...
  DDRD &= ~BUTTON_MASK;
  
  // Enable MIDI
#ifdef DEBUG
  Serial.begin(115200);
#else
  MIDI.begin();
  MIDI.turnThruOff();
  MIDI.setHandleSystemExclusive(handleSystemExclusive);
#endif
  
  // load LEDs from EEPROM
  //for (byte row = 0; row < 4; row++) {
  //  for (byte column = 0; column < 12; column++) {
  //    leds[row][column] = EEPROM.read(EEPROM_LED_OFFSET + (12 * row) + column);
  //  }
  //}
  
  // TODO: remove and uncomment above!
  // for testing, let's give the LED some colors!
  for (byte row = 0; row < 4; row++) {
    for (byte column = 0; column < 4; column++) {
      leds[row][column*3]   = min(256 - row * 64, 255);      // R
      leds[row][column*3+1] = min(256 - column * 64, 255);   // G
      leds[row][column*3+2] = 255;                           // B
    }
  }
  
  // zero out debouncing, button state, and load buttons
  for (byte row = 0; row < 4; row++) {
    button_state[row] = 0;
    for (byte column = 0; column < 4; column++)
      buttons[row][column] = 0;   // TODO: EEPROM.read(EEPROM_BUTTON_OFFSET + (4 * row) + column);
    for (byte checks = 0; checks < DEBOUNCING_CHECKS; checks++)
      button_debounce[row][checks] = 0;
  }
}

void handleSystemExclusive(byte *array, byte size)
{
  // array[0] will be 0xF0 which starts the sysex message
  // array[1] will be the manufacturer ID - ignore the message if it
  //   doesn't match our manufacturer ID
  // array[2] will be the operation, assuming the message is for us
  // array[3..size-2] will be the data encoded using the following scheme:
  //   bytes evenly divisible by 8 (0, 8, 16, ...) will represent the MSb
  //   of the following 7 bytes, with the LSb corresponding to the first
  //   byte, and the MSb corresponding to the 7th byte. This is because
  //   the MIDI standard reserves bytes that have the MSb set. This scheme
  //   allows us to encode values greater than 127 and is, conveniently,
  //   implemented by the Arduino MIDI library.
  // array[size-1] is 0xF7, the MIDI code for "end of sysex"
  // size includes the 0xF0 "start sysex" code, the manufacturer ID,
  //   the operation code, and the terminating 0xF7 code, so the actual
  //   data length is size - 4 starting at array + 3.
  if (array[1] == MIDI_MANUFACTURER_ID) {
    if (array[2] == SYSEX_SET_ALL_LEDS) {
      // Set All LEDs
      // leds is a 2D array, but C lays out 2D arrays like 1D arrays, so
      // we trick it into dumping the data in with a cast.
      midi::decodeSysEx(array + 3, (byte*)leds, size - 4);
    } else if (array[2] == SYSEX_SET_LED) {
      // Set an Individual LED Color
      // array[3..6] is R,G,B for the LED, encoded as described above (hence
      //   the extra byte)
      // array[7] is the row
      // array[8] is the column
      // Technically, array[7] and array[8] are part of the encoding as
      // described above, but since both row and column will be less than 4,
      // we don't need to worry about decoding their MSb.
      midi::decodeSysEx(array + 3, (byte*)leds + (12 * array[7] + 3 * array[8]), 4);
    } else if (array[3] == SYSEX_SAVE_LEDS) {
      // Save the LED Colors to the EEPROM
      for (byte i = 0; i < 4; i++) {
        for (byte j = 0; j < 12; j++) {
          EEPROM.write(EEPROM_LED_OFFSET + (12 * i) + j, leds[i][j]);
        }
      }
    } else if (array[3] == SYSEX_RESET_LEDS) {
      // Reset the LEDs from the EEPROM
      for (byte i = 0; i < 4; i++) {
        for (byte j = 0; j < 12; j++) {
          leds[i][j] = EEPROM.read(EEPROM_LED_OFFSET + (12 * i) + j);
        }
      }
    } else if (array[3] == SYSEX_SET_ALL_BUTTONS) {
      // Set All Buttons
      // buttons in a 2D array, but C lays out 2D arrays like 1D arrays, so
      // we trick it into dumping the data in with a cast.
      midi::decodeSysEx(array + 3, (byte*)buttons, size - 4);
    } else if (array[3] == SYSEX_SET_BUTTON) {
      // Set an Individual Button
      // array[3] will always be zero as a result of encoding
      // array[4] will be the note to use for the button
      // array[5] is the row
      // array[6] is the column
      buttons[array[5]][array[6]] = array[4];
    } else if (array[3] == SYSEX_SAVE_BUTTONS) {
      // Save the Buttons to the EEPROM
      for (byte i = 0; i < 4; i++) {
        for (byte j = 0; j < 4; j++) {
          EEPROM.write(EEPROM_BUTTON_OFFSET + (4 * i) + j, buttons[i][j]);
        }
      }
    } else if (array[3] == SYSEX_RESET_BUTTONS) {
      // Reset the buttons from the EEPROM
      for (byte i = 0; i < 4; i++) {
        for (byte j = 0; j < 4; j++) {
          buttons[i][j] = EEPROM.read(EEPROM_BUTTON_OFFSET + (4 * i) + j);
        }
      }
    }
  }
}

void loop()
{
  // The iteration count is used to vary LED intensity. If we have a red
  // value of 128, for example, we only turn that LED "on" for the first
  // half of the passes through the main loop. It's kind of like a poor
  // man's PWM.
  static byte last_row = 3, row = 0, iteration_count = 0;
  
  // read MIDI data
#ifdef DEBUG
#else
  MIDI.read();
#endif
  
  // The shift registers are configured so that RGB LED 1 is on pins
  // 0 = R, 1 = G, 2 = B of the first shift register. RGB LED 2 is also
  // on the first shift register, pins 3, 4, and 5 in the same order.
  // RGB LED 3 is on pins 0-2 of the second register, and RGB LED 4 is
  // on pins 3-5. The first byte that we shift out will be destined for
  // the second shift register. Since we are shifting out LSB first, we
  // want column 11 (RGB LED #4, blue channel) to end up setting bit 2.
  // Column 10 (RGB LED #3, green channel) should set bit 3, and so on.
  // This means we use 13 - column to calculate the bit position.
  byte column_data = 0, column = 11;
  for (; column >= 6; column--) {
    if (leds[row][column] > iteration_count)
      column_data |= 1 << (13 - column);
  }
  SPDR = column_data;
  
  // Build second byte while the first one is transferring. This will
  // be the byte destined for the first shift register. We want column
  // 5 (RGB LED #2, blue channel) to end up setting bit 2. Column 4
  // (RGB LED #2, green channel) should set bit 3, and so on. Because
  // we shift out LSB first, this means we use 7 - column to calculate
  // the bit positions.
  column_data = 0;
  for (; column >= 0; column--) {
    if (leds[row][column] > iteration_count)
      column_data |= 1 << (7 - column);
  }
  
  // Wait for first byte to finish (do we need this?) then start
  // sending the second byte of column data. SPI Flag (bit 7 in the
  // SPI Status Register) will go high when SPI is done transferring.
  while (!(SPSR & _BV(7)));
  SPDR = column_data;
  
  // While we wait for the second byte to transfer, let's check our
  // buttons, shall we? Currently, the previous "row" will still have
  // power, since we haven't turned it off yet (that happens below).
  // pressed_buttons is initialized so that we'll only get a 1 for the
  // buttons that we didn't already know were pressed. Likewise,
  // released_buttons will have a 0 for buttons we didn't already know
  // were released - we negate it after the loop so it'll have 1's instead.
  byte pressed_buttons = BUTTON_MASK & ~button_state[last_row];
  byte released_buttons = ~BUTTON_MASK | ~button_state[last_row];
  button_debounce[last_row][button_index] = PORTD;
  for (byte i = 0; i < DEBOUNCING_CHECKS; i++) {
    pressed_buttons &= button_debounce[last_row][i];
    released_buttons |= ~button_debounce[last_row][i];
  }
  released_buttons = ~released_buttons;
  
  // If pressed_buttons is non-zero, we have a newly pressed button!
  if (pressed_buttons) {
    for (byte column = 0; column < 4; column++) {
      if (pressed_buttons & BUTTON_MASKS[column]) {
        button_state[last_row] |= BUTTON_MASKS[column];
#ifdef DEBUG
        Serial.print("ON ");
        Serial.print(buttons[last_row][column], HEX);
        Serial.print("\n");
#else
        MIDI.sendNoteOn(buttons[last_row][column], 127, 1);
#endif
      }
    }
  }
  
  // if released_buttons is non-zero, we have a newly released button!
  if (released_buttons) {
    for (byte column = 0; column < 4; column++) {
      if (released_buttons & BUTTON_MASKS[column]) {
        button_state[last_row] &= ~BUTTON_MASKS[column];
#ifdef DEBUG
        Serial.print("OFF ");
        Serial.print(buttons[last_row][column], HEX);
        Serial.print("\n");
#else
        MIDI.sendNoteOff(buttons[last_row][column], 127, 1);
#endif
      }
    }
  }
  
  // Make sure we're done sending the second byte via SPI and disable all rows.
  // Probably don't need to check SPI here, since the previous part will take
  // a pretty long time... but just to be safe... Remember, disabling a row
  // means that we pull the pin "high"!!
  while (!(SPSR & _BV(7)));
  PORTB |= ROW1_ENABLE_MASK | ROW2_ENABLE_MASK;
  PORTD |= ROW3_ENABLE_MASK | ROW4_ENABLE_MASK;
  
  // We've connected the SS pin (Port B, pin 2) to the latch clock on the
  // shift registers. Here we strobe the pin to cause the shift registers
  // to latch on the new values. The data sheet for the shift registers
  // recommends that the latch clock stays high for at least 40ns.
  // The Arduino is operating at 8MHz which means the pin should stay
  // high for at least 125ns, even if we turn it back off "immediately".
  PORTB |= _BV(2);
  PORTB &= ~_BV(2);
  
  // Enable the row - the first two rows (0 and 1) are on Port B,
  // but the second rows (2 and 3) are on Port D. Remember, enabling the
  // row means we pull the pin "low"!!
  if (row & B00000010)
    PORTD &= ROW_ENABLES[row];   // we're on row 2 or 3
  else
    PORTB &= ROW_ENABLES[row];   // we're on row 0 or 1
  
  // Set the last_row to the current row, and, if the current row is zero,
  // increment the button_index.
  last_row = row;
  if (!last_row)
    button_index = (button_index + 1) & (DEBOUNCING_CHECKS - 1);
  
  // increment the row counter, but wrap 4 back to 0
  // also increment the iteration_count
  row = (row + 1) & B00000011;
  iteration_count++;
}
