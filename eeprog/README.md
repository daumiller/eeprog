## EEProg ##

### Makeshift EEPROM Programmer using Arduino Micro. ###

**Hardware**
* [eeprog_schematic.png](https://raw.githubusercontent.com/daumiller/bitshiffed/master/eeprog/eeprog_schematic.png)
* Programs AT28C256 EEPROMS; possibly works with similar chips.
* Uses shift registers as port expanders for address bus.

**Software**
* [eeprog.ino](https://github.com/daumiller/bitshiffed/blob/master/eeprog/eeprog.ino) - Arduino source
* [eeprog.rb](https://github.com/daumiller/bitshiffed/blob/master/eeprog/eeprog.rb)  - read/write interface, in Ruby.
  * requires `serialport` (`gem install serialport`)
  * may switch to `rubyserial` in the future

**Specs**
* Read 32 KiB in under 5 seconds.
* Write 32 KiB in under 12 seconds (includes verification readback).
