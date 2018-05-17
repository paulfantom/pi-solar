#ifndef pinout_h
#define pinout_h
#include <stdlib.h>
#include <Arduino.h>

/* RESERVED PINS:
 *  0,1   - (RX, TX) DEBUG serial
 *  4     - CS SD Card
 *  10    - CS Ethernet Shield
 *  11-13 - SPI
 *  A4,A5 - (SDA, SCL) I2C
 */

const int8_t CS_SD = 4;
const int8_t CS_ETH = 53;

const int8_t ONE_WIRE_BUS = 24;

uint8_t ROW_PINS[5] = {39, 37, 35, 33, 31}; //connect to the row pinouts of the keypad
uint8_t COL_PINS[4] = {23, 25, 27, 29}; //connect to the column pinouts of the keypad

const int8_t SOLAR_PUMP = 44; // valve (DAC)
// pins (tape) to SWITCHES: 28, 30, 32, 34, 36, 38, 40, 42
// pins (SWITCH): 42, 28, 40, 30, 38, 32, 36, 34
const int8_t SOLAR_SWITCH = 40;
const int8_t SOLAR_PUMP_SWITCH = 42; //pump

const int8_t HEATER_PUMP = 36;
const int8_t HEATER_BURNER = 36;
const int8_t HEATER_SWITCH = 30;
const int8_t CIRC_PUMP = 28;
// not used Binary pins: 38, 32, 34
const int8_t CIRC_DETECT = A8;

const int8_t SOLAR_TEMP_AUX_PIN = A15;   // analog
const int8_t SOLAR_TEMP_PIN = A14;   // analog
const int8_t OUTSIDE_TEMP_PIN = A13; // analog
const int8_t INSIDE_TEMP_PIN = A12;  // analog
#endif
