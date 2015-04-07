// EEPROG
//==============================================================================
#include <SPI.h>

//==============================================================================
// Definitions
#define VERSION "0.1.1"

#define EEPROM_CS 2
#define EEPROM_WE 3
#define EEPROM_OE 4

#define STATUS_LED1 A5
#define STATUS_LED2 A4
#define STATUS_LED3 A3

#define DATA_BUS_UNDER 4
#define DATA_BUS_LOW   5
#define DATA_BUS_HIGH 12
#define DATA_BUS_OVER 13

#define ADDR_BUS_SER   MOSI
#define ADDR_BUS_SRCLK SCK
#define ADDR_BUS_RCLK  SS

#define WRITE_BYTE_VERIFY_ATTEMPTS 128
#define PAGE_SIZE 64

//#define SERIAL_BAUD_SPEED 9600
#define SERIAL_BAUD_SPEED 115200

//==============================================================================
// Functions
void set_data_bus(byte data);
void set_addr_bus(byte high, byte low);
byte eeprom_read_byte(byte high, byte low);
bool eeprom_write_byte(byte high, byte low, byte data);
void eeprom_read_page(byte high, byte low, byte *buffer);
bool eeprom_write_page(byte high, byte low, byte *buffer);

void page_increment(byte *high, byte *low);
bool memcmp(char *a, char *b, byte length);
bool scan_hex_byte(char *cmd, byte *data);
bool scan_hex_short(char *cmd, byte *high, byte *low);
bool scan_hex_digit(char *cmd, byte *data);
bool scan_chunk_params(char *cmd, byte *high, byte *low, byte *pages);
void serial_print_byte(byte data);
void serial_print_short(byte high, byte low);
void serial_print_opt(bool opt);

void serial_cmd_help();
void serial_cmd_read(char *cmd);
void serial_cmd_write(char *cmd);
void serial_cmd_chunk_read(char *cmd);
void serial_cmd_chunk_write(char *cmd);

//==============================================================================
// Globals
char str_command_data[56];
char *str_command = str_command_data;
byte serial_read_byte;
byte flags = 0;

byte chunk_buffer_data[PAGE_SIZE * 8]; // 8 pages * 64 B == 512 B
byte verify_buffer_data[PAGE_SIZE];    // verify one page at a time
byte *page_buffer;                     // page read/write location

//==============================================================================
// Setup
void setup() {
  // EEPROM Control
  pinMode(EEPROM_CS, OUTPUT); digitalWrite(EEPROM_CS, HIGH);
  pinMode(EEPROM_WE, OUTPUT); digitalWrite(EEPROM_WE, HIGH);
  pinMode(EEPROM_OE, OUTPUT); digitalWrite(EEPROM_OE, HIGH);

  // Data Bus
  for(byte ii=DATA_BUS_LOW; ii<DATA_BUS_OVER; ii++) { pinMode(ii, OUTPUT); digitalWrite(ii, LOW); }

  // Address Bus
  pinMode(ADDR_BUS_SER,   OUTPUT);
  pinMode(ADDR_BUS_SRCLK, OUTPUT);
  pinMode(ADDR_BUS_RCLK,  OUTPUT);
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);

  // Status LEDs
  pinMode(STATUS_LED1, OUTPUT); digitalWrite(STATUS_LED1, LOW);
  pinMode(STATUS_LED2, OUTPUT); digitalWrite(STATUS_LED1, LOW);
  pinMode(STATUS_LED3, OUTPUT); digitalWrite(STATUS_LED1, LOW);

  // Command Protocol
  Serial.begin(SERIAL_BAUD_SPEED);
  while(!Serial) { ; }
  Serial.println("EEPROG Ready");

  // LED1 Ready
  digitalWrite(STATUS_LED1, HIGH);
}

void loop() {}

//==============================================================================
// Serial Dispatch
void serial_process_command(char *cmd) {
  // help
  // version
  // read    0000    - read byte
  // write   0000 00 - write byte
  // chunkr  0000  0 - read chunk (size <<= 2)
  // chunkw  0000  0 - write chunk

  if(memcmp(cmd, "help"   , 4)) { serial_cmd_help();             return; }
  if(memcmp(cmd, "read "  , 5)) { serial_cmd_read(cmd+5);        return; }
  if(memcmp(cmd, "write " , 6)) { serial_cmd_write(cmd+6);       return; }
  if(memcmp(cmd, "chunkr ", 7)) { serial_cmd_chunk_read(cmd+7);  return; }
  if(memcmp(cmd, "chunkw ", 7)) { serial_cmd_chunk_write(cmd+7); return; }
  if(memcmp(cmd, "version", 7)) { Serial.print("EEPROG v"); Serial.println(VERSION); return; }

  Serial.print("Unrecognized command \""); Serial.print(cmd); Serial.println("\".");
}

//==============================================================================
// Serial Commands
const char help_message[] PROGMEM = "--------------------------------------------\n"
                                    "  help            - this listing            \n"
                                    "  version         - print version           \n"
                                    "  read aaaa       - read byte               \n"
                                    "  write aaaa dd   - write byte              \n"
                                    "  chunkr aaaa p   - read chunk              \n"
                                    "  chunkw aaaa p   - write chunk             \n"
                                    "                                            \n"
                                    "  aaaa - four character hex address         \n"
                                    "    dd - two character hex byte             \n"
                                    "     p - single character hex page count    \n"
                                    "                                            \n"
                                    "  Note: spacing and casing are important.   \n"
                                    "  Note: pages are 64 bytes each             \n"
                                    "        a chunk may be 1-8 pages            \n"
                                    "        chunks must start on a page boundary\n"
                                    "  Note: chunk data is read and wrote in     \n"
                                    "        lines of 16, spaced, hex bytes      \n"
                                    "        (4 lines per page)                  \n"
                                    "--------------------------------------------\n";

void serial_cmd_help() {
  Serial.print("EEPROG v"); Serial.println(VERSION);

  const char *message = help_message;
  byte ch = pgm_read_byte_near(message);
  while(ch != 0x00) {
    Serial.write(ch);
    message++;
    ch = pgm_read_byte_near(message);
  }
}

void serial_cmd_read(char *cmd) {
  byte high, low;
  bool result = scan_hex_short(cmd, &high, &low);
  if(!result) { return; }

  byte data = eeprom_read_byte(high, low);
  Serial.print("Read 0x"); serial_print_byte(data); Serial.print(" from 0x"); serial_print_short(high, low); Serial.println(".");
}

void serial_cmd_write(char *cmd) {
  bool result; byte high, low, data, test;
  if(*cmd == ' ') { cmd++; } // "write" || "writev"
  result = scan_hex_short(cmd, &high, &low); if(!result) { return; }
  result = scan_hex_byte(cmd + 5, &data); if(!result) { return; }

  result = eeprom_write_byte(high, low, data);
  if(result == false) { return; }

  Serial.print("Wrote 0x"); serial_print_byte(data); Serial.print(" to 0x"); serial_print_short(high, low); Serial.println(".");
}

void serial_cmd_chunk_read(char *cmd) {
  byte high, low, pages;
  bool result = scan_chunk_params(cmd, &high, &low, &pages);
  if(!result) { return; }

  digitalWrite(STATUS_LED3, HIGH);
  Serial.println("OKAY: Beginning Chunk.");

  // Read Pages in Chunk Buffer
  page_buffer = chunk_buffer_data;
  byte page_index = 0;
  for(page_index=0; page_index<pages; page_index++, page_buffer += PAGE_SIZE) {
    eeprom_read_page(high, low, page_buffer);
    page_increment(&high, &low);
  }

  // Write Chunk to Serial
  page_buffer = chunk_buffer_data;
  byte line_index, byte_index; page_index = 0;
  for(page_index=0; page_index<pages; page_index++) {
    for(line_index=0; line_index<4; line_index++) {
      for(byte_index=0; byte_index<16; byte_index++, page_buffer++) {
        serial_print_byte(*page_buffer);
        Serial.write(' ');
      }
      Serial.write('\n');
    }
  }

  digitalWrite(STATUS_LED3, LOW);
}

void serial_cmd_chunk_write(char *cmd) {
  byte high, low, pages, origin_high, origin_low;
  bool result = scan_chunk_params(cmd, &origin_high, &origin_low, &pages);
  if(!result) { return; }

  digitalWrite(STATUS_LED2, HIGH);
  Serial.println("OKAY: Ready for data.");

  // Read Chunk from Serial
  page_buffer = chunk_buffer_data;
  byte page_index, line_index, byte_index;
  for(page_index=0; page_index<pages; page_index++) {
    for(line_index=0; line_index<4; line_index++) {
      serial_read_line();
      for(byte_index=0; byte_index<16; byte_index++, str_command+=3, page_buffer++) {
        if(scan_hex_byte(str_command, page_buffer) == false) {
          digitalWrite(STATUS_LED2, LOW);
          return;
        }
      }
    }
  }

  // Write Chunk to EEPROM
  page_buffer = chunk_buffer_data;
  page_index = 0;
  high = origin_high; low = origin_low;
  while(page_index < pages) {
    if(eeprom_write_page(high, low, page_buffer) == false) {
      digitalWrite(STATUS_LED2, LOW);
      return;
    }
    page_increment(&high, &low);
    page_buffer += PAGE_SIZE;
    page_index++;
  }

  // Verify Chunk
  page_buffer = chunk_buffer_data;
  high = origin_high; low = origin_low;
  for(page_index=0; page_index<pages; page_index++, page_buffer+=PAGE_SIZE) {
    eeprom_read_page(high, low, verify_buffer_data);

    if(memcmp((char *)page_buffer, (char *)verify_buffer_data, PAGE_SIZE) == false) {
      Serial.println("FAIL: Chunk verification failed.");
      digitalWrite(STATUS_LED2, LOW);
      return;
    }
    page_increment(&high, &low);
  }

  Serial.println("OKAY: Chunk writing and verification succeeded.");
  digitalWrite(STATUS_LED2, LOW);
}

//==============================================================================
// Serial Event Processing
void serialEvent() {
  while(Serial.available()) {
    serial_read_line();
    serial_process_command(str_command);
  }
}

void serialEventRun() { if(Serial.available()) serialEvent(); }

void serial_read_line() {
  str_command = str_command_data;
  byte ch = 0x00;

  while(ch != '\n') {
    if(!Serial.available()) { continue; }
    ch = Serial.read();

    if(ch != '\n') {
      if(ch != '\r') {
        *str_command = (char)ch;
        str_command++;
      }
    } else {
      str_command[0] = 0x00;
      str_command[1] = 0x00;
      str_command = str_command_data;
    }
  }
}

//==============================================================================
// Utilities
void page_increment(byte *high, byte *low) {
  // this should only ever be called on page-aligned addresses
  if(*low == (256-PAGE_SIZE)) {
    *high += 1;
    *low = 0;
  } else {
    *low += PAGE_SIZE;
  }
}

bool memcmp(char *a, char *b, byte length) {
  for(byte ii=0; ii<length; ii++) {
    if(a[ii] != b[ii]) { return false; }
  }
  return true;
}

bool scan_hex_byte(char *cmd, byte *data) {
  *data = 0;
  if((*cmd >= '0') && (*cmd <= '9')) { *data |= *cmd - '0'; }
  else if((*cmd >= 'a') && (*cmd <= 'f')) { *data |= (*cmd - 'a') + 10; }
  else if((*cmd >= 'A') && (*cmd <= 'F')) { *data |= (*cmd - 'A') + 10; }
  else {
    Serial.print("Unrecognized hex character '"); Serial.write(*cmd); Serial.println("'.");
    return false;
  }

  cmd++;
  *data <<= 4;

  if((*cmd >= '0') && (*cmd <= '9')) { *data |= *cmd - '0'; }
  else if((*cmd >= 'a') && (*cmd <= 'f')) { *data |= (*cmd - 'a') + 10; }
  else if((*cmd >= 'A') && (*cmd <= 'F')) { *data |= (*cmd - 'A') + 10; }
  else {
    Serial.print("Unrecognized hex character '"); Serial.write(*cmd); Serial.println("'.");
    return false;
  }

  return true;
}

bool scan_hex_short(char *cmd, byte *high, byte *low) {
  bool result = scan_hex_byte(cmd, high);
  if(result) { result = scan_hex_byte(cmd+2, low); }
  return result;
}

bool scan_hex_digit(char *cmd, byte *data) {
  if((*cmd >= '0') && (*cmd <= '9')) { *data = *cmd - '0';        return true; }
  if((*cmd >= 'a') && (*cmd <= 'f')) { *data = (*cmd - 'a') + 10; return true; }
  if((*cmd >= 'A') && (*cmd <= 'F')) { *data = (*cmd - 'A') + 10; return true; }

  Serial.print("Unrecognized hex character '"); Serial.write(*cmd); Serial.println("'.");
  return false;
}

bool scan_chunk_params(char *cmd, byte *high, byte *low, byte *pages) {
  bool result = scan_hex_short(cmd, high, low); if(!result) { return false; }
  result = scan_hex_digit(cmd + 5, pages);      if(!result) { return false; }

  if(*low & 63) {
    Serial.print("Chunk Error: Address 0x"); serial_print_short(*high, *low); Serial.println(" is not on a page boundary.");
    return false;
  }

  if((*pages == 0) || (*pages > 8)) {
    Serial.print("Chunk Error: Chunks may only be 1-8 pages (attempted to use "); Serial.print(*pages); Serial.println(").");
    return false;
  }

  return true;
}

static const char hex_table[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

void serial_print_byte(byte data) {
  Serial.write(hex_table[data >> 4]);
  Serial.write(hex_table[data & 0xF]);
}

void serial_print_short(byte high, byte low) {
  Serial.write(hex_table[high >>  4]);
  Serial.write(hex_table[high & 0xF]);
  Serial.write(hex_table[low  >>  4]);
  Serial.write(hex_table[low  & 0xF]);
}

void serial_print_opt(bool opt) {
  if(opt) {
    Serial.print('on');
  } else {
    Serial.print('off');
  }
}

//==============================================================================
// Actual Data Functions
void set_data_bus(byte data) {
  digitalWrite( 5, (data & 0x01) != 0);
  digitalWrite( 6, (data & 0x02) != 0);
  digitalWrite( 7, (data & 0x04) != 0);
  digitalWrite( 8, (data & 0x08) != 0);
  digitalWrite( 9, (data & 0x10) != 0);
  digitalWrite(10, (data & 0x20) != 0);
  digitalWrite(11, (data & 0x40) != 0);
  digitalWrite(12, (data & 0x80) != 0);
}

void set_addr_bus(byte high, byte low) {
  digitalWrite(ADDR_BUS_RCLK, LOW);
  SPI.transfer(high);
  SPI.transfer(low);
  digitalWrite(ADDR_BUS_RCLK, HIGH);
}

byte eeprom_read_byte(byte high, byte low) {
  byte ii, value = 0;
  set_addr_bus(high, low);
  for(ii=DATA_BUS_HIGH; ii>DATA_BUS_UNDER; ii--) { pinMode(ii, INPUT); }
  digitalWrite(EEPROM_CS, LOW);
  digitalWrite(EEPROM_OE, LOW);
  for(ii=DATA_BUS_HIGH; ii>DATA_BUS_UNDER; ii--) { value <<= 1; value |= digitalRead(ii); }
  digitalWrite(EEPROM_OE, HIGH);
  digitalWrite(EEPROM_CS, HIGH);
  for(ii=DATA_BUS_HIGH; ii>DATA_BUS_UNDER; ii--) { pinMode(ii, OUTPUT); }
  return value;
}

// finish a write command, waiting until Bit6 stops toggling
// expects Address to already be set, and EEPROM_CS to already be low
bool eeprom_read_through_toggle(byte target) {
  byte value, prev, ii, cc = 0;

  for(ii=DATA_BUS_HIGH; ii>DATA_BUS_UNDER; ii--) { pinMode(ii, INPUT); }
  digitalWrite(EEPROM_OE, LOW);
  for(ii=DATA_BUS_HIGH; ii>DATA_BUS_UNDER; ii--) { value <<= 1; value |= digitalRead(ii); }
  digitalWrite(EEPROM_OE, HIGH);
  prev = ~value;

  // Bit6 will toggle, changing the read value, for every read until writing is complete
  while((cc < WRITE_BYTE_VERIFY_ATTEMPTS) && (value != prev)) {
    prev = value;
    digitalWrite(EEPROM_OE, LOW);
    for(ii=DATA_BUS_HIGH; ii>DATA_BUS_UNDER; ii--) { value <<= 1; value |= digitalRead(ii); }
    digitalWrite(EEPROM_OE, HIGH);
    cc++;
  }

  for(ii=DATA_BUS_HIGH; ii>DATA_BUS_UNDER; ii--) { pinMode(ii, OUTPUT); }
  if((cc == WRITE_BYTE_VERIFY_ATTEMPTS) || (value != target)) { return false; }
  return true;
}

bool eeprom_write_byte(byte high, byte low, byte data) {
  byte _a, _b;

  set_addr_bus(high, low);
  set_data_bus(data);
  digitalWrite(EEPROM_CS, LOW);
  digitalWrite(EEPROM_WE, LOW);
  _a = ~high; // intentionally wasting cycles
  _b = ~low;  // may not be needed w/ faster EEPROM or slower Arduino
  digitalWrite(EEPROM_WE, HIGH);

  bool verified = eeprom_read_through_toggle(data);
  digitalWrite(EEPROM_CS, HIGH);
  if(verified == false) { Serial.println("FAIL: byte write verification failed."); }

  return verified;
}

void eeprom_read_page(byte high, byte low, byte *buffer) {
  byte ii, value;

  for(ii=DATA_BUS_HIGH; ii>DATA_BUS_UNDER; ii--) { pinMode(ii, INPUT); }
  digitalWrite(EEPROM_CS, LOW);

  for(byte pg=0; pg<PAGE_SIZE; pg++) {
    set_addr_bus(high, low);
    digitalWrite(EEPROM_OE, LOW);
    for(ii=DATA_BUS_HIGH, value=0; ii>DATA_BUS_UNDER; ii--) { value <<= 1; value |= digitalRead(ii); }
    digitalWrite(EEPROM_OE, HIGH);
    *buffer = value;
    buffer++;
    low++;
  }

  digitalWrite(EEPROM_CS, HIGH);
  for(ii=DATA_BUS_HIGH; ii>DATA_BUS_UNDER; ii--) { pinMode(ii, OUTPUT); }
}

bool eeprom_write_page(byte high, byte low, byte *buffer) {
  byte ii, byte_verify, byte_last;
  digitalWrite(EEPROM_CS, LOW);

  for(ii=0; ii<PAGE_SIZE; ii++) {
    set_addr_bus(high, low);
    set_data_bus(*buffer);
    digitalWrite(EEPROM_WE, LOW);
    buffer++;
    low++;
    digitalWrite(EEPROM_WE, HIGH);
  }

  bool verified = eeprom_read_through_toggle(*(buffer-1));
  digitalWrite(EEPROM_CS, HIGH);

  if(verified == false) { Serial.println("FAIL: page write verification failed."); }
  return verified;
}
