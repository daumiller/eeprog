#!/usr/bin/env ruby
require 'optparse'
require 'serialport'

# ================ DEFAULTS ================ #
DEFAULT_FILE      = 'eep.rom'
DEFAULT_DEVICE    = '/dev/cu.usbmodem*'
EEPROM_SIZE       = 32_768
SERIAL_SPEED      = 115_200
SERIAL_TIMEOUT    = 5.0
VERSION_MATCH     = /EEPROG v([0-9]+)\.([0-9]+)\.([0-9]+)/
VERSION_MAX_MAJOR = 0
VERSION_MIN_MAJOR = 0
VERSION_MIN_MINOR = 1
VERSION_MIN_PATCH = 0

# ================ OPTIONS ================ #
class Options
  attr_reader :mode, :device, :address, :size, :file

  def initialize
    collect
  end

  def show_usage
    puts 'Usage: eeprog (read|write) (-d serial-device)       (-a start(-end))        (-s size)     (-f file.rom)'
    puts '       eeprog (read|write) (--device serial-device) (--address start(-end)) (--size size) (--file file.rom)'
    puts ''
    puts "       > If device not specified, will attempt to use first device matching \"#{DEFAULT_DEVICE}\"." unless DEFAULT_DEVICE.nil?
    puts "       > If file not specified, will attempt to use \"#{DEFAULT_FILE}\"." unless DEFAULT_FILE.nil?
    puts '       > Address and Size values are assumed to be hexadecimal, unless followed by a \'d\' (ex: "100d")'
    puts '       > Starting address and total size must be multiples of 40 (64 decimal).'
    puts "       > Total ROM size assumed to be #{EEPROM_SIZE} decimal." unless EEPROM_SIZE.nil?
  end

  def bail(message, usage=false, status=nil)
    status = usage if status.nil?
    $stderr.puts "ERROR: #{message}"
    if usage
      puts ''
      show_usage
    end

    exit status
  end

  def warn(message)
    puts "WARN: #{message}"
  end

  private

  def check_device_and_file
    bail 'No device specified, and default device not found.' if @device.nil?
    bail "Device path not found (#{@device})."                unless File.exist? @device
    bail "File to write not found (#{@file})."                if (@mode == :write) && (File.exist?(@file) == false)
  end

  def check_address_and_size
    @size = File.size @file                     if @size.nil? &&  @address_end.nil? && (@mode == :write)
    @size = (EEPROM_SIZE - @address)            if @size.nil? &&  @address_end.nil?
    @size = (@address_end - @address_start + 1) if @size.nil? && !@address_end.nil?

    bail('Beginning address must be on a 64 byte boundary.') if (@address % 64) > 0
    bail('Data size must be a multiple of 64.')              if (@size % 64) > 0
    bail("Size out of range ( > #{EEPROM_SIZE}).")           if (@address + @size - 1) > EEPROM_SIZE
    bail('Data size of zero.')                               if @size == 0
    warn('File size is smaller than requested write size. Will write zero padded.') if (@mode == :write) && (@size > File.size(@file))
  end

  def parse_number(number)
    return nil if number.nil?
    number.end_with?('d') ? number.to_i : number.to_i(16)
  end

  def collect_mode
    if    (File.basename($PROGRAM_NAME).end_with? 'read')  || (ARGV[0] == 'read')  ; @mode = :read
    elsif (File.basename($PROGRAM_NAME).end_with? 'write') || (ARGV[0] == 'write') ; @mode = :write
    else ; bail('Program mode not specified (read or write).', true) ; end
  end

  def collect_address(specified_address)
    if specified_address.nil?
      @address = 0
      @address_end = nil
      return
    end

    @address, @address_end = specified_address.split '-'
    @address     = parse_number @address
    @address_end = parse_number @address_end
  end

  def collect
    specified_device, specified_address, specified_size, specified_file = nil, nil, nil, nil
    OptionParser.new do |options|
      options.on('-d', '--device /dev/tty0',          'serial device')              { |argument| specified_device  = argument }
      options.on('-a', '--address 9999(d)(-9999(d))', 'address (range)')            { |argument| specified_address = argument }
      options.on('-s', '--size 9999(d)',              'size in bytes')              { |argument| specified_size    = argument }
      options.on('-f', '--file eep.rom',              'file to read-to/write-from') { |argument| specified_file    = argument }
    end.parse!

    collect_mode
    collect_address specified_address
    @device = specified_device || Dir[DEFAULT_DEVICE][0]
    @file   = specified_file || DEFAULT_FILE
    @size   = parse_number specified_size

    check_device_and_file
    check_address_and_size
  end
end

# ================ SERIAL PORT ================ #
class Serial
  attr_accessor :debug

  def initialize(device, speed)
    @read_buffer = ''
    @device = SerialPort.new device, { :baud => speed }
    ObjectSpace.define_finalizer(self, proc { @device.close })
  end

  def read_line
    read = @device.readline.strip
    puts "Serial READ  | #{read}" if @debug
    read
  end

  def write(data)
    puts "Serial WRITE | #{data}" if @debug
    @device.write data
  end

  def write_line(line)
    puts "Serial WRITE | #{line}" if @debug
    line += "\n" unless line[-1] == "\n"
    @device.write line
  end

  def expect_line(expects)
    line = read_line
    expects = { true => expects } if expects.class != Hash
    expects.each do |key, value|
      return [key, value]             if (value.class == String) && (value == line)
      return [key, value.match(line)] if (value.class == Regexp) && !value.match(line).nil?
    end

    [nil, line]
  end

  def expect_or_fail(expects)
    key, value = expect_line(expects)
    if key.nil?
      $stderr.puts 'Serial Expect Error: didn\'t match expected line.'
      $stderr.puts "    Read:     #{value}"
      $stderr.puts "    Expected: #{expects}"
      exit
    end
    [key, value]
  end

end

# ================ EEPROG ================ #
class EEProg
  def initialize
    @options = Options.new
    # if options.mode == :read
    #   puts "Reading #{options.size} bytes, from address #{options.address}, to file #{options.file}, from device #{options.device}."
    # else
    #   puts "Writing #{options.size} bytes, to address #{options.address}, from file #{options.file}, to device #{options.device}"
    # end

    @serial = Serial.new @options.device, SERIAL_SPEED
    # @serial.debug = true
    check_firmware_version

    read  if @options.mode == :read
    write if @options.mode == :write
  end

  private

  def check_firmware_version
    # this command knows that it is the very first serial exchange made.
    # other commands know that this command is responsible for clearing out the intro line, if it is received.
    # (this should eventually be cleaned up with a Serial#read_until_expect (or similar); but I'm being lazy about that...)

    @serial.write_line 'version'
    key, value = @serial.expect_or_fail({ :intro => 'EEPROG Ready', :version => VERSION_MATCH })
    _,   value = @serial.expect_or_fail(VERSION_MATCH) if key == :intro
    major, minor, patch = value[1].to_i, value[2].to_i, value[3].to_i

    if major > VERSION_MAX_MAJOR
      $stderr.puts "ERROR: Firmware version #{major}.x.x not yet supported (or this program needs updated)."
      exit
    end
    
    unless version_compare([major, minor, patch], [VERSION_MIN_MAJOR, VERSION_MIN_MINOR, VERSION_MIN_PATCH])
      $stderr.puts "ERROR: Firmware version #{major}.#{minor}.#{patch} is too old."
      $stderr.puts "       Version #{VERSION_MIN_MAJOR}.#{VERSION_MIN_MINOR}.#{VERSION_MIN_PATCH} is required for this program."
      exit
    end
  end
  
  def version_compare(version, minimum)
    if version[0] > minimum[0] ; return true ; elsif version[0] < minimum[0] ; return false ; end
    if version[1] > minimum[1] ; return true ; elsif version[1] < minimum[1] ; return false ; end
    return version[2] >= minimum[2]
  end

  def read
    time_started = Time.now
    page_count   = @options.size >> 6
    read_address = @options.address
    read_buffer  = ''

    until page_count == 0
      chunk_pages   = (page_count > 8) ? 8 : page_count
      read_buffer  += read_chunk(read_address, chunk_pages)
      page_count   -= chunk_pages
      read_address += (chunk_pages << 6)
    end

    File.write @options.file, read_buffer
    time_elapsed = (Time.now - time_started).round(2)
    puts "Read #{@options.size} bytes to \"#{@options.file}\" in #{time_elapsed} seconds."
  end

  def read_chunk(address, pages)
    read_buffer = ''

    @serial.write_line "chunkr #{address_string(address)} #{pages}"
    key, value = @serial.expect_or_fail({ :okay => /OKAY: .+/, :fail => /FAIL: .+/ })
    if key == :fail
      puts "ERROR: read operation failed - \"#{value[0]}\"."
      exit
    end
    (1..(pages << 2)).each { read_buffer += read_hex_line }

    read_buffer
  end

  def read_hex_line
    @serial.read_line.split(' ').map { |x| x.to_i(16).chr }.join
  end

  def write
    time_started = Time.now
    page_count    = @options.size >> 6
    write_address = @options.address
    write_buffer  = write_buffer_create

    until page_count == 0
      chunk_pages    = (page_count > 8) ? 8 : page_count
      chunk_bytes    = chunk_pages << 6
      chunk_buffer   = write_buffer.slice! 0, chunk_bytes
      write_chunk write_address, chunk_buffer, chunk_pages
      page_count    -= chunk_pages
      write_address += chunk_bytes
    end

    time_elapsed = (Time.now - time_started).round(2)
    puts "Wrote #{@options.size} bytes from \"#{@options.file}\" in #{time_elapsed} seconds."
  end

  def write_buffer_create
    file_size = File.size @options.file
    write_buffer = File.read @options.file
    write_buffer += ("\x00" * (@options.size - file_size)) if file_size < @options.size
    write_buffer
  end

  def write_chunk(address, buffer, pages)
    @serial.write_line "chunkw #{address_string(address)} #{pages}"
    key, value = @serial.expect_or_fail({ :okay => /OKAY: .+/, :fail => /FAIL: .+/ })
    if key == :fail
      puts "ERROR: write operation failed - \"#{value[0]}\"."
      exit
    end

    while buffer.length > 0
      @serial.write_line buffer.slice!(0, 16).bytes.map { |x| byte_string(x) }.join(' ')
      # apparently, i need to rate-limit Serial writes, or things go wonky...
      # maybe i overflow the arduino's serial buffer? CTS gets stuck on, and writes stop working...
      sleep 0.001
    end

    key, value = @serial.expect_or_fail({ :okay => /OKAY: .+/, :fail => /FAIL: .+/ })
    if key == :fail
      puts "ERROR: write operation failed - \"#{value[0]}\"."
      exit
    end
  end

  def address_string(address)
    address.to_s(16).upcase.rjust(4, '0')
  end

  def byte_string(byte)
    byte.to_s(16).upcase.rjust(2, '0')
  end
end

# ================ RUNTIME ================ #
EEProg.new
