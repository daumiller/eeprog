## EEProg ##

### Makeshift EEPROM Programmer using Arduino Micro. ###

**Hardware**

* [eeprog_schematic.png](https://raw.githubusercontent.com/daumiller/bitshiffed/master/eeprog/eeprog_schematic.png) - schematic
* Programs AT28C256 EEPROMS; possibly works with similar chips.
* Uses shift registers as port expanders for address bus.

**Software**

* [eeprog.ino](https://github.com/daumiller/bitshiffed/blob/master/eeprog/eeprog.ino) - Arduino source
* [eeprog.rb](https://github.com/daumiller/bitshiffed/blob/master/eeprog/eeprog.rb)  - read/write interface, in Ruby.
  * requires [serialport](https://github.com/hparra/ruby-serialport) (`gem install serialport`)
  * ([rubyserial](https://github.com/hybridgroup/rubyserial) looks nice, but had serious issues for me)

**License**

* Source code is released under the 2-Clause BSD License (see [LICENSE](https://github.com/daumiller/bitshiffed/blob/master/LICENSE) file).
* Schematics are public domain.
