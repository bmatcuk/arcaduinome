# arcaduinome
Arcaduinome is an [Arduino](http://www.arduino.cc/) based MIDI controller that uses arcade buttons for input. The name is a combination of Arcade, Arduino, and [Monome](http://monome.org/).

## Software
The ArduinoSketch directory contains the Arduino source code. It requires that you have the MIDI Library installed. The latest build (at time of writing) is included in this repo. You'll first need to install it into your Arduino app: Open the Arduino application and select Sketch -> Import Library -> Add Library, then select Arduino_MIDI_Library_v4.1.zip.

## Hardware
Arcaduinome is based around the [Arduino Uno](http://arduino.cc/en/Main/ArduinoBoardUno), third revision. Most of the other hardware components are to support the LEDs which light the arcade buttons. Each component links to a website where you could buy the part, but you can probably find most of these parts at a variety of distributors. A lot of these parts could be swapped out for similar parts. I've made some notes below about why I've chosen a specific part and whether or not you can use some other generic part.

* **[Arduino Uno R3](http://www.adafruit.com/products/50)**
    I highly recommend a 3rd Revision (R3) board here. The R3 board has a larger USB-to-serial microcontroller (Atmega16U2) which will be reprogrammed later. If you have an older R1 or R2 board, you'll need to reprogram the USB-to-serial microcontroller every time you need to switch between MIDI and Arduino mode. Very annoying. With the R3, you just jumper two pins to switch back to Arduino mode.

* **16x [Sanwa OBSC 24 CW](http://www.paradisearcadeshop.com/sanwa-obsc-24mm-translucent-pushbuttons/689-sanwa-obsc-24-cw.html)**
    Any arcade button will do. Sanwa buttons are extremely popular with arcade cabinet and fighting game enthusiasts for their quick response and high-reliability. However, the buttons might be the most expensive part, so you can save a bit with some cheaper buttons. Just make sure they are translucent/transparent.

* **[Protoshield for Arduino](http://www.adafruit.com/products/51)**
    This part is optional. I like using the protoshield to break out the IO pins from the Arduino and it gives me a nice area to solder everything together. Plus, it's compact.

* **16x [Common Anode RGB LEDs](http://www.adafruit.com/products/302)**
    Theoretically, any RGB LEDs will do, as long as they are common-anode. You may need to adjust resistor values depending on the forward voltage of the LEDs you pick. Link goes to a 25-pack that's actually cheaper, I've found, than buying 16 discrete LEDs from other distributors. So save some money and you'll have 9 extra LEDs for your next project! Woot!

* **[Sanken SLA5086 - 5x P-Channel FET Array](http://www.digikey.com/product-search/en?pv606=52&pv69=80&FV=fff40015%2Cfff80346&mnonly=0&newproducts=0&ColumnSort=0&page=1&stock=1&quantity=0&ptm=0&fid=0&pageSize=500)**
    The P-Channel FETs are used on the "high-side" of the LEDs to turn on the "rows". FETs are well-suited for this switching application, specifically P-Channels for the high-side. If every LED in a row is turned on, the high-side switch must be able to source ~0.24A. This part can handle 5A, so no problems there. Additionally, this particular part has a low (logic level) gate-threshold voltage, which means that it should fully "turn on" easily from the Arduino IO pin. All around, a good fit, and the hardest part to find alternatives for.

* **2x [STMicroelectronics STPIC6D595B1R 8-bit Shift Register](http://www.digikey.com/product-detail/en/STPIC6D595B1R/497-6328-5-ND/1762207)**
    The shift registers are used on the "low-side" of the LEDs to control individual LEDs (ie, the columns). We need 2 of these because we have "12" LEDs per row (each RGB LED is basically like 3 LEDs - we have 4 RGB LEDs per row, so, 4 x 3 = 12). The two ICs are "daisy chained" to make a 16-bit shift register; we just ignore the last 4-bits since we only need 12. Each pin of the shift register must be able to sink 20mA, and, if every LED in the row is "on", the IC as a whole must be able to sink 8 x 20mA = 160mA. This IC can easily handle that with it's open-drain outputs. Additionally, the outputs have latches and a global "enable", which the code takes advantage of.

* **16x [NXP Semiconductors 1N4148,113 Standard Diode](http://www.digikey.com/product-detail/en/1N4148,113/568-1360-1-ND/763357)**
    Use any standard diodes you want, though lower forward voltages are preferable. These are just used to ensure that the buttons don't interfere with each other.

* **[12-pin SIP Socket](http://www.digikey.com/product-detail/en/643644-3/A29107-ND/294602)**
    Socket for the P-Channel FET Array - if you decide on some other high-side solution, you might not need this.

* **2x [16-pin DIP Socket](http://www.digikey.com/product-detail/en/A16-LC-TR-R/AE9994-ND/821748)**
    Sockets for the low-side shift registers - if you decide on some other low-side solution, you might not need this.

## Arduino Firmware
The Arduino Uno R3 uses a microcontroller, specifically the Atmega16U2, as a USB-to-Serial adapter. Arcaduinome takes advantage of this fact using the [Moco](http://morecatlab.akiba.coocan.jp/lab/index.php/aruino/midi-firmware-for-arduino-uno-moco/?lang=en) (aka dualMocoLUFA) firmware. The latest version (at the time of writing) is included in this repo as dualMoco.hex. Installing this firmware on the Atmega16U2 will cause the Arduino to appear as a generic MIDI device to Windows/OSX. This way, we won't need any special drivers to interface with Arcaduinome.

To install the firmware into the Atmega16U2, follow the [DFU Programming Directions](http://arduino.cc/en/Hacking/DFUProgramming8U2) on Arduino's website. Once the new firmware has been installed, it will appear as a generic MIDI device when connected to your computer. You will no longer be able to use it as an Arduino (in other words, you won't be able to reprogram it). However, if you want to reenable Arduino mode, disconnect the Arduino from your computer, connect a jumper between pin 4 and 6 on the ICSP header, and reconnect to your computer. It should now appear as a normal Arduino Uno. When you're done reprogramming, disconnect from your computer, remove the jumper, and reconnect - it will now appear as a MIDI device again!

## License
This work is licensed under a [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](http://creativecommons.org/licenses/by-nc-sa/4.0/)

[![Creative Commons License](http://i.creativecommons.org/l/by-nc-sa/4.0/88x31.png)](http://creativecommons.org/licenses/by-nc-sa/4.0/)