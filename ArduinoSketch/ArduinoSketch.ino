#include <MIDI.h>
#include <midi_Defs.h>
#include <midi_Namespace.h>
#include <midi_Settings.h>

void setup()
{
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
