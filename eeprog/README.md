## EEProg ##
------------

### Makeshift EEPROM Programmer using Arduino Micro. ###

** Hardware **

* [eeprog_schematic.png](url) - schematic
* Programs AT28C256 EEPROMS; possibly works with similar chips.
* Uses shift registers as port expanders for address bus.

** Software **

* [eeprog.ino](url) - Arduino source
* [eeprog.rb](url)  - read/write interface, in Ruby.
  * requires [serialport](https://github.com/hparra/ruby-serialport) (`gem install serialport`)
  * ([rubyserial](https://github.com/hybridgroup/rubyserial) looks nice, but had serious issues for me)

** License **

* Source code is released under the 2-Clause BSD License (see LICENSE file).
* Schematics are public domain.
