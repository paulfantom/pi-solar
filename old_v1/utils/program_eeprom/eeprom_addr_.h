#ifndef eeprom_addr_h
#define eeprom_addr_h
#include <stdlib.h>
//TODO comment not used variables to save some place in memory
// EEPROM address list:
const uint8_t EE_FLAGS = 0;
const uint8_t EE_CIRC_T_ON = 1;  // (16b) Min interval beetween turning on circulation pump [in seconds]
const uint8_t EE_CIRC_T_OFF = 3; // (16b) Work time of circulation pump [in seconds]

const uint8_t EE_UNDEFINED = 5; // (32b) Free Space

const uint8_t EE_HEAT_HYST = 9; // (16b) Heater hysteresis for Central Heating
const uint8_t EE_HEAT_CRIT = 11; // (16b) Critical temp. of Heater (usually 90 deg)
const uint8_t EE_HEAT_TEMP = 13;  // (16b) Last expected room temperature
const uint8_t EE_HEAT_ON  = 15; // (16b) Heater hysteresis for Central Heating
const uint8_t EE_HEAT_OFF = 17; // (16b) Heater hysteresis for Central Heating

const uint8_t EE_SOL_TANK = 19;  // (16b) Max temp. in tank when on solar
const uint8_t EE_HEAT_MAX = 21;  // (16b) Max temp. in tank when on heater
const uint8_t EE_HEAT_MIN = 23;  // (16b) Min temp. in tank when on heater

const uint8_t EE_SOL_CRIT = 25;   // (16b) Critical temperature of Solar panel
const uint8_t EE_SOL_ON = 27;    // (16b) Temp. when solar panel should turn on
const uint8_t EE_SOL_OFF = 29;   // (16b) Temp. when solar panel should turn off // same as EE_PUMP_T1

const uint8_t EE_PUMP_T1 = EE_SOL_OFF;   // (16b) min pump temperature // same as EE_SOL_OFF
const uint8_t EE_PUMP_T2 = 31;   // (16b) max pump temperature
const uint8_t EE_PUMP_D1 = 33;   // (16b) min pump duty cycle (0-100%)
const uint8_t EE_PUMP_D2 = 35;   // (16b) max pump duty cycle (0-100%)
const uint8_t EE_PUMP_DMIN = 37; // (16b) Critical min pump duty cycle (set from auto pump calibration) (NOT USED)
const uint8_t EE_PUMP_HYST = 39; // (16b) Pump Hysteresis

const uint8_t EE_THRM1_B = 41; // (16b) Thermistor Beta
const uint8_t EE_THRM1_R = 43; // (16b) Thermistor R0 (nominal resistance)
const uint8_t EE_THRM1_T = 45; // (16b) Thermistor T0 (nominal temperature, usually 25 deg)

/*const uint8_t EE_THRM2_B = 41; // (16b) Thermistor Beta
const uint8_t EE_THRM2_R = 43; // (16b) Thermistor R0 (nominal resistance)
const uint8_t EE_THRM2_T = 45; // (16b) Thermistor T0 (nominal temperature, usually 25 deg)

const uint8_t EE_THRM3_B = 47; // (16b) Thermistor Beta
const uint8_t EE_THRM3_R = 49; // (16b) Thermistor R0 (nominal resistance)
const uint8_t EE_THRM3_T = 51; // (16b) Thermistor T0 (nominal temperature, usually 25 deg)*/

/*const uint8_t EE_MBUS1_ADDR   = 53; // (16b) Meter-Bus meter address
const uint8_t EE_MBUS1_MEDIUM = 55; // (16b) Meter-Bus meter medium
const uint8_t EE_MBUS2_ADDR   = 57; // (16b) Meter-Bus meter address
const uint8_t EE_MBUS2_MEDIUM = 59; // (16b) Meter-Bus meter medium
const uint8_t EE_MBUS3_ADDR   = 61; // (16b) Meter-Bus meter address
const uint8_t EE_MBUS3_MEDIUM = 63; // (16b) Meter-Bus meter medium*/

const uint8_t EE_DS18B20_1 = 47;  // (64b) DS18B20 64 bit address
const uint8_t EE_DS18B20_2 = 55;  // (64b) DS18B20 64 bit address
const uint8_t EE_DS18B20_3 = 63;  // (64b) DS18B20 64 bit address
const uint8_t EE_DS18B20_4 = 71;  // (64b) DS18B20 64 bit address
const uint8_t EE_DS18B20_5 = 79;  // (64b) DS18B20 64 bit address
const uint8_t EE_DS18B20_6 = 87;  // (64b) DS18B20 64 bit address
const uint8_t EE_DS18B20_7 = 95;  // (64b) DS18B20 64 bit address
const uint8_t EE_DS18B20_8 = 103;  // (64b) DS18B20 64 bit address

const uint8_t EE_ETH_IP   = 111; // (32b) IP address
const uint8_t EE_ETH_DNS  = 115; // (32b) DNS address
const uint8_t EE_ETH_GATE = 119; // (32b) Gateway address
const uint8_t EE_ETH_SUBN = 123; // (32b) Subnet address

const uint8_t EE_MQTT_IP = 127;   // (32b)  Server IP address
const uint8_t EE_MQTT_PORT = 131; // (16b)  Port // TODO delete it
const uint8_t EE_MQTT_USER = 133; // (112b) MQTT username (max 14)
const uint8_t EE_MQTT_PASS = 147; // (112b) MQTT password (max 14)

const uint8_t EE_TIME_IP = 161;   // (32b) Time Server IP
const uint8_t EE_TIME_OFF = 165;  // (16b) Time offset (in minutes)
#endif
