#define SENSORS_COUNT 11
//#define INSIDE_SAMPLES 60  // one sample per 5 seconds
//#define OUTSIDE_SAMPLES 60 // one sample per 10 seconds
#define SOLAR_SAMPLES 120   // one sample per second
#define ANALOG_PULLUP 9820
//A14, A15 pull-down resistor value: 9820

#define DEBUG 1

/*
 * FIXME
 * Setting heater expected temperature from menu is always set to 0 (MQTT problem?)
 * Menu probably cannot render negative values (test)
 * MQTT subscriptions (test)
 * MQTT callback (test)
 * 
 * TODO TODO TODO TODO TODO TODO TODOOOOOOOOO ...pink panther
 * + MQTT subscribe to all setpoints (check)
 * + watchdog
 * + tests tests tests
 * + MQTT topic parser
 * + error handling and printing
 * + disable autoreset
 * + menu cleanup
 * 
 * TODO in next version:
 * + DEBUG console
 * + implement RTOS (ex. NilRTOS)
 * + move menu.loop() to thread
 * + swap all analogRead, Wire, delay, etc. to RTOS counterparts
 * 
 * TODO in version x.y:
 * + NEST API
 * + device profile
 */

#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <EthernetUdp.h>
#include <Keypad.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <stdlib.h>
#include <DS3232RTC.h>
#include <Time.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
//#include <stdlib.h>
#include <Thermistor.h>
#include "eeprom_addr.h"
#include <Menu.h>
#include "pinout.h"

#include <Solar.h>
#include <Heater.h>
#include <Circulation.h>

//#ifdef DEBUG
 #define DEBUG_PRINT(x) Serial.print(x)
 #define DEBUG_PRINTLN_BIN(x) Serial.println(x,BIN)
 #define DEBUG_PRINTLN(x) Serial.println(x)
//#else
// #define DEBUG_PRINT(x)
// #define DEBUG_PRINTLN(x)
//#endif*/

#define DEBUG Serial
//#define MBUS Serial2
//#define MBUSEvent serial2Event

// ----- KEYPAD -----
char keys[5][4] = {
  {'F','G','-','.'}, // F - F1, G - F2
  {'1','2','3','u'}, // u - up
  {'4','5','6','d'}, // d - down
  {'7','8','9','Q'}, // l - left, Q - Esc
  {'l','0','r','E'}  // r - right, E - Ent
};

// ----- COMMUNICATION GLOBALS -----
const char DEVICE_ID[] = "solarControl";
const char MAX_MQTT_TOPIC_LENGTH = 16;
//TODO find unique MAC address
uint8_t MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
uint8_t MQTT_IP[] = { 192, 168, 2, 2 }; //must be global or MQTT won't connect //FIXME assign RPi address
int MQTT_PORT = 1883; //FIXME should be 1883 (tests)
EthernetUDP udp;
EthernetClient ethClient;
PubSubClient MQTT(MQTT_IP, MQTT_PORT, mqttSubCallback, ethClient);
// sensors topic:  DEVICE_ID/...
// example: solarControl/heater/temp_in
const char MQTT_SENSORS[][MAX_MQTT_TOPIC_LENGTH] = {"solar/temp", "room/1/temp_2", "outside/temp",
                                                    "solar/temp_in", "solar/temp_out", 
                                                    "heater/temp_in", "heater/temp_out", 
                                                    "tank/temp_up", "circulate/pump",
                                                    "actuators", "solar/pump"};
// settings topic:  DEVICE_ID/+/settings/...
// example: solarControl/heater/settings/hysteresis
// example: solarControl/solar/settings/flow/pwm/s_min
const uint8_t MQTT_SETTINGS_CIRC_LEN = 2;
const char MQTT_SETTINGS_CIRC[][MAX_MQTT_TOPIC_LENGTH]   = {"interval", "time_on"};
const uint8_t MQTT_SETTINGS_HEATER_LEN = 6;
const char MQTT_SETTINGS_HEATER[][MAX_MQTT_TOPIC_LENGTH] = {"hysteresis", "critical", "expected",
                                                            "schedule/on", "schedule/off", "heating_on"};
const uint8_t MQTT_SETTINGS_TANK_LEN = 3;
const char MQTT_SETTINGS_TANK[][MAX_MQTT_TOPIC_LENGTH]   = {"solar_max", "heater_max", "heater_min"};
const uint8_t MQTT_SETTINGS_SOLAR_LEN = 10;
const char MQTT_SETTINGS_SOLAR[][MAX_MQTT_TOPIC_LENGTH]  = {"critical", "on", "off",
                                                            "flow/pwm/t_min", "flow/pwm/t_max", "flow/pwm/s_min", "flow/pwm/s_max"};
                                                            //"flow/pid/Kp", "flow/pid/Ki", "flow/pid/Kd"};

//uint8_t MBUS_ADDR = 4;
OneWire oneWire(ONE_WIRE_BUS);

// ----- DS18B20 -----
DallasTemperature digitalThermometers(&oneWire);
uint8_t DS18B20_COUNT = 7;
DeviceAddress ds18b20Addresses[7]; // ds18b20Addresses[DS18B20_COUNT];
const int T_MAX = 100;
const int T_MIN = -40;

// ----- THERMISTORS -----
//Thermistor outside(OUTSIDE_TEMP_PIN,OUTSIDE_SAMPLES);
Thermistor solar(SOLAR_TEMP_PIN,SOLAR_TEMP_AUX_PIN,SOLAR_SAMPLES);

// ----- USER INPUT/OUTPUT
Keypad keypad = Keypad( makeKeymap(keys), ROW_PINS, COL_PINS, 5, 4);
LiquidCrystal_I2C lcd(0x3F,2,1,0,4,5,6,7,3,POSITIVE);
Menu menu(keypad, lcd, MAX_LINES, 20);

// ----- CONTROL -----
Circulation circulationControll(CIRC_PUMP);
Solar solarControll(SOLAR_PUMP,SOLAR_PUMP_SWITCH,SOLAR_SWITCH);
Heater heaterControll(HEATER_PUMP,HEATER_SWITCH,HEATER_BURNER);

// ----- OTHER GLOBALS -----
uint8_t PREV_MANUAL_STATE = 0;
char ERROR_LINE[2][21] = {"", ""};
bool HEATER_WATER_MODE = false; //false - CWU, true - CO //disables CWU when true (future use in ANN, computed on more powerfull platform [Raspberry?])
bool CAN_HEAT = false; // TODO MQTT collect data //TODO write menu for it
int16_t SENSORS_DATA[SENSORS_COUNT];
//T solar,T wewnatrz,T zewnatrz,T solar (in),T solar (out),T piec (in),T piec (out),T zbiornik (gora), circulation detect, actuators, pwm_duty
// actuators: 00000000000spwhc (c - circulation, h - heater, w - co/cwu, p - solar pump, s - solar switch

// ----------------------------------- SETUP ----------------------------------
void setup() {
  // Watchdog
  //wdt_enable(WDTO_8S);
  //SPI pins
  pinMode(CS_ETH, OUTPUT);
  pinMode(CS_SD, OUTPUT);
  // --- SERIAL ---
  Serial.begin(9600);
  Serial.println();
  // --- LCD ---
  lcd.begin (20,MAX_LINES);
  lcd.setBacklight(HIGH);
  lcd.clear();
  lcd.home();
  lcd.noCursor();
  lcd.noBlink();
  uint8_t errors = 0;
  lcd.setCursor(0,0);
  //TODO after power failure wait some time for HMI to boot (use it as "wait for key func")
  lcd.print(F("Waiting for a key"));
  char key = NO_KEY;
  for(int i=0; i<300; i++){
    if(i%100 == 0){
      lcd.setCursor(19,0);
      lcd.print(3 - (int)(i/100));
    }
    key = keypad.getKey();
    if(key != NO_KEY) break;
    delay(10);
  }
  lcd.setCursor(0,0);
  lcd.print(F("Initializing        "));

  // --- SD Card ---
  SD.begin(CS_SD);
  digitalWrite(CS_SD,HIGH);
  digitalWrite(CS_ETH,LOW);

  // --- ETH ---
  if(key != NO_KEY){
    clearLine(0);
    lcd.print(F("Connecting to LAN"));
    delay(3000);
  }
  if (! reconnect()){
    newError(&errors,"*Network Error");
  }
  
  // --- RTC ---
  setSyncProvider(RTC.get);
  if(key != NO_KEY){
    clearLine(0);
    lcd.print(F("Configuring RTC"));
    delay(3000);
  }
  if(timeStatus() != timeSet) {
        DEBUG_PRINTLN(F("Unable to sync with the RTC"));
        newError(&errors,"*No external RTC");
  } else {
        DEBUG_PRINTLN(F("RTC has set the system time"));
        DEBUG_PRINT(year());   DEBUG_PRINT('-');
        DEBUG_PRINT(month());  DEBUG_PRINT('-');
        DEBUG_PRINT(day());    DEBUG_PRINT(' ');
        DEBUG_PRINT(hour());   DEBUG_PRINT(':');
        DEBUG_PRINT(minute()); DEBUG_PRINT(':');
        DEBUG_PRINTLN(second());
  }

  if(key != NO_KEY){
    clearLine(0);
    lcd.print(F("Syncing time w/ NTP"));
    delay(3000);
  }
  if (Ethernet.localIP()[0] != 0){ // No IP -> No Time Sync va NTP
    if (syncTime() == false) {
      newError(&errors,"*Cannot sync time");
    }
  }

  // --- THERMISTORS ---
  if(key != NO_KEY){
    clearLine(0);
    lcd.print(F("Setting Thermistor 1"));
  }
  analogReference(EXTERNAL);
  int B; int R0; int T0;
  EEPROM.get(EE_THRM1_B,B);
  EEPROM.get(EE_THRM1_R,R0);
  EEPROM.get(EE_THRM1_T,T0);
  solar.setup(B,R0,T0,9820,(bool)true);
  /*lcd.setCursor(19,0);
  lcd.print(2);
  EEPROM.get(EE_THRM2_B,B);
  EEPROM.get(EE_THRM2_R,R0);
  EEPROM.get(EE_THRM2_T,T0);
  //outside.setup(B,R0*1000,T0,10000,true);
  lcd.setCursor(19,0);
  lcd.print(3);
  EEPROM.get(EE_THRM3_B,B);
  EEPROM.get(EE_THRM3_R,R0);
  EEPROM.get(EE_THRM3_T,T0);
  inside.setup(B,R0*1000,T0,10000,true);*/

  /*// --- MBUS ---
  //TODO make it happen!
  clearLine(0);
  lcd.print("Setting MBus");
  delay(3000);
  MBUS.begin(2400,SERIAL_8E1);
  newError(&errors,"*MBus ERROR");*/

  // --- DS18B20 ---
  if(key != NO_KEY){
    clearLine(0);
    lcd.print(F("Setting DS18B20's"));
    delay(3000);
  }
  digitalThermometers.begin();
  if(! assignDS18B20()){
    newError(&errors,"*DS18B20 Error");
  }
  digitalThermometers.setWaitForConversion(false); //ASYNC mode
  digitalThermometers.setResolution(12);

  //TODO get some data to start with
  // --- INITIAL DATA ---
  if(key != NO_KEY){
    clearLine(0);
    lcd.print(F("Gathering data"));
    delay(2000);
  }
  DEBUG_PRINTLN(F("Gathering data"));
  //SENSORS_DATA[1] = inside.get();
  //SENSORS_DATA[2] = outside.get();
  SENSORS_DATA[0] = solar.get();
  DEBUG_PRINT(F("+ Solar Temperature: ")); DEBUG_PRINTLN(SENSORS_DATA[0]);
  digitalThermometers.requestTemperatures();
  delay(1000);
  for(int i=1; i<=DS18B20_COUNT; i++){
    SENSORS_DATA[i] = getTemperature(ds18b20Addresses[i-1]);
    DEBUG_PRINT(F("+ Temperature no.")); DEBUG_PRINT(i); DEBUG_PRINT(" : "); DEBUG_PRINTLN(SENSORS_DATA[i]);
  }

  // --- MQTT ---
  if(key != NO_KEY){
    clearLine(0);
    lcd.print(F("Configuring MQTT"));
    delay(3000);
  }
  DEBUG_PRINTLN(F("Configuring MQTT"));
  /*int port;
  uint8_t ip[4];*/
  //EEPROM.get(EE_MQTT_PORT, MQTT_PORT);  //TODO test it
  //EEPROM.get(EE_MQTT_IP, MQTT_IP); //TODO test it
  /*
  ip[0] = 10;
  DEBUG_PRINTLN(port);
  //MQTT = PubSubClient(ip, port, mqttSubCallback, ethClient); //update object MQTT
  //PubSubClient MQTT(ip, port, mqttSubCallback, ethClient); //update object MQTT
  //MQTT.setup(ip, port, mqttSubCallback, ethClient); //setup object MQTT
  DEBUG_PRINTLN("New MQTT client");
  DEBUG_PRINT(ip[0]); DEBUG_PRINT('.'); DEBUG_PRINT(ip[1]); DEBUG_PRINT('.');
  DEBUG_PRINT(ip[2]); DEBUG_PRINT('.'); DEBUG_PRINTLN(ip[3]);
  DEBUG_PRINTLN(port);*/
  //TODO retain msg
  if (Ethernet.localIP()[0] != 0){ // No IP -> No MQTT
    if (MQTT.connect((char*)DEVICE_ID)){
      mqttSubscribe();
    } else {
      newError(&errors,"*MQTT Broker no conn");
    }  
  }

  // --- CONTROLL ---
  pinMode(CIRC_DETECT,INPUT_PULLUP);
  pinMode(A9,OUTPUT);
  digitalWrite(A9, LOW);
  updateSetpoints();

  // --- FINISH ---
  menu.setup(SENSORS_DATA,SENSORS_COUNT,600,now());
  clearLine(0);
  if (errors == 0){
    lcd.print(F("   ALL SYSTEMS GO"));
    //delay(1000);
  } else {
    lcd.print(F("SYSTEM with "));
    lcd.print(errors);
    lcd.print(F(" ERROR"));
    if (errors>1){
      lcd.print('S');
    }
  }
  DEBUG_PRINTLN(F("END OF SETUP"));
}

bool MQTT_STATUS = false;
// ----------------------------------- MAIN -----------------------------------
unsigned int next_execution[] = {1, 2, 3, 4, 5, 10, 7, 5, 0}; // initialising with different values prevents all executions in one loop
uint32_t curr = 0;
bool menu_open = false; //TODO do updateSetpoints() more frequent when in menu
void loop() {
  //wdt_reset();
  MQTT.loop();
  menu_open = menu.loop(true);

  // every loop
  if (curr != now()){
    //decrement all executions
    for(int i=0; i<sizeof(next_execution)/sizeof(next_execution[0]); i++){
      next_execution[i] -= 1;
    }
    if(menu.getManualState()) menu.decrementManual();
    curr = now();
  }

  //check circulation!
  if((menu.getManualState() >> 4) & 1){
    circulationControll.manualMode((bool)(SENSORS_DATA[9] >> 4) & 1);
  } else {
    SENSORS_DATA[8] = ! (bool)digitalRead(CIRC_DETECT);
    circulationControll.interrupt((bool)SENSORS_DATA[8]);
  }

  // every second action
  if (next_execution[0] == 0){
    solar.add();
    if(! menu_open){
      /*if(! renderErrors()){ //no errors //FIXME check for errors every 1s
        lcd.setBacklight(LOW);
        lcd.noDisplay();
      }*/
    }
    next_execution[0] = 1;
  }

  // Temperatures
  if (next_execution[1] == 0){
    digitalThermometers.requestTemperatures();
    next_execution[1] = 2; // we're in ASYNC mode (need ~760ms wait time)
    next_execution[2] = 1; // just to be sure
  }
  
  if (next_execution[2] == 0){
    //if(! menu.getManualState()){
      //T solar,T wewnatrz,T zewnatrz,T solar (in),T solar (out),T piec (in),T piec (out),T zbiornik (gora), water flow
      SENSORS_DATA[0] = solar.get();
      //DEBUG_PRINT("Solar temperature: "); DEBUG_PRINTLN(SENSORS_DATA[0]);
      if (DS18B20_COUNT>0 && ! MQTT_STATUS){
        SENSORS_DATA[1] = EMAsmoothing(SENSORS_DATA[1], getTemperature(ds18b20Addresses[0]));
        DEBUG_PRINT("Inside Temperature: "); DEBUG_PRINTLN(SENSORS_DATA[1]);
      }
      for(int i=2; i<DS18B20_COUNT+1; i++){
        SENSORS_DATA[i] = EMAsmoothing(SENSORS_DATA[i], getTemperature(ds18b20Addresses[i-1]));
        //DEBUG_PRINT("Temperature no."); DEBUG_PRINT(i); DEBUG_PRINT(" : "); DEBUG_PRINT(SENSORS_DATA[i]); DEBUG_PRINT(" ADDR: "); printAddress(ds18b20Addresses[i-1]);
      }
    //}
    next_execution[2] = 2;
  }

  if (next_execution[3] == 0){
    DEBUG_PRINTLN(F("Saving data to SD Card"));
    if(logData()) next_execution[3] = 30;
    else          next_execution[3] = 1;
  }

  //FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  if (next_execution[4] == 0){
    if (MQTT_STATUS) {
      mqttPublishData();
      if(menu.newSettings()){
        mqttPublishSettings();
      }
    } else {
      // check if we need heating (only without MQTT)
      uint16_t schedule[2] = {1500, 2000};
      for(int i=0; i<2; i++){ EEPROM.get(EE_HEAT_ON+i*2,schedule[i]); }
      DEBUG_PRINT("TIME: "); DEBUG_PRINT(schedule[0]); DEBUG_PRINT(" < "); DEBUG_PRINT(hour()*100 + minute()); DEBUG_PRINT(" < "); DEBUG_PRINTLN(schedule[1]);
      uint16_t tmp = hour()*100 + minute();
      if((schedule[0] < tmp) && (tmp < schedule[1])){
      //if((schedule[0] < (hour()*100 + minute())) && ((hour()*100 + minute()) < schedule[1])){
        CAN_HEAT = true;
      } else {
        CAN_HEAT = false;
      }
    }
    next_execution[4] = 10;
  }
  
  if (next_execution[5] == 0){
    DEBUG_PRINTLN(F("Updating actuators"));
    runControl();
    updateSetpoints();
    if(menu.getManualState()){
      next_execution[5] = 2;
    }
    else next_execution[5] = 5;
  }

  if (next_execution[6] == 0){
    DEBUG_PRINTLN(F("Syncing Time"));
    if(syncTime()){
      next_execution[6] = 43200; // 12h //FIXME add missing seconds to match next 00:30:00
    } else {
      next_execution[6] = 3600; // try at next hour
    }
  }

  // check connection to MQTT broker + check (every 2 minutes)
  if (next_execution[7] == 0){
    DEBUG_PRINTLN(F("Checking MQTT Connectivity"));
    mqttCheckConn(false);
    /*if(MQTT.connected()) next_execution[7] = 120;
    else next_execution[7] = 2;*/
    next_execution[7] = 120;
  }

  if (next_execution[8] == 0){
    DEBUG_PRINTLN(F("Maintain DHCP"));
    Ethernet.maintain();
    next_execution[8] = 3600;
  }

  if (menu.getManualTime() == 0){
    menu.resetManual();
  }

  delay(50);
}

// --------------------------------- CONSOLE ----------------------------------
void serialEvent(){
  char c;
  while(Serial.available()){
    c = (char)Serial.read();
    switch(c){
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8':
      Serial.println(SENSORS_DATA[c-48]);
      break;
    case '9':
      Serial.println(SENSORS_DATA[c-47]);
      break;
    case 'c':
      //reconnect
      reconnect();
      break;
    case 'm':
      //reload MQTT
      mqttCheckConn(true);
      break;
    case 's':
      //update setpoints
      updateSetpoints();
      break;
    case 'a':
      //run algorithm
      runControl();
      break;
    }
  }
}

// FIXME delete
// function to print a device address
void printAddress(DeviceAddress deviceAddress){
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
  Serial.println();
}

// ---------------------------- CONTROLL ALGORITHM ----------------------------
void runControl(){
  int last_actuators = SENSORS_DATA[SENSORS_COUNT-2];
  //(T1, T8, T2, T3);
  solarControll.update(SENSORS_DATA[0],SENSORS_DATA[7],SENSORS_DATA[3],SENSORS_DATA[4]);
  //(T5, T8, T9, boolean schedule, boolean solar)
  // schedule - is it time to power heater on?
  // solar - is solar capable of supplying enough energy to heat water? (based on MPC w/ ANN running on RasPi)
  //TODO throw HEATER_WATER_MODE in manual menu
  DEBUG_PRINT("STATS: "); DEBUG_PRINT(CAN_HEAT); DEBUG_PRINTLN(HEATER_WATER_MODE);
  heaterControll.update(SENSORS_DATA[5],SENSORS_DATA[7],SENSORS_DATA[1],CAN_HEAT,HEATER_WATER_MODE);
  //heaterControll.update(SENSORS_DATA[5],SENSORS_DATA[7],SENSORS_DATA[1],true,true);
  
  solarControll.run();
  heaterControll.run();
  
  if((menu.getManualState() >> 5) & 1){
  //if(menu.getManualState()){
    analogWrite(SOLAR_PUMP,SENSORS_DATA[SENSORS_COUNT-1]*255/100);
  } else {
    SENSORS_DATA[SENSORS_COUNT-1] = solarControll.getPWM();
  }
  //get actuators data
  //DEBUG_PRINT("Algorithm: |"); DEBUG_PRINTLN(SENSORS_DATA[SENSORS_COUNT-2],BIN);
  //DEBUG_PRINT("Manual:    |"); DEBUG_PRINTLN(menu.getManualState(),BIN);
  SENSORS_DATA[SENSORS_COUNT-2] = 0;

  // 1U == unsigned int value 1
  //if( menu.getManualState() & 1U ){
  if( bitRead(menu.getManualState(),0) ){
    SENSORS_DATA[SENSORS_COUNT-2] += (last_actuators & 0b00000001);
    //digitalWrite(SOLAR_SWITCH,last_actuators & 1U);
    digitalWrite(SOLAR_SWITCH,bitRead(last_actuators,0));
  //} else if ( PREV_MANUAL_STATE & 1U ){
  } else if ( bitRead(PREV_MANUAL_STATE,0) ){
    digitalWrite(SOLAR_SWITCH, LOW);
  } else {
    //SENSORS_DATA[SENSORS_COUNT-2] += digitalRead(SOLAR_SWITCH) & 0b00000001;
    bitWrite(SENSORS_DATA[SENSORS_COUNT-2], 0, digitalRead(SOLAR_SWITCH));
  }
  
  //if( (menu.getManualState() >> 1) & 1U ){
  if( bitRead(menu.getManualState(),1U) ){
    SENSORS_DATA[SENSORS_COUNT-2] += (last_actuators & 0b00000010);
    //digitalWrite(SOLAR_PUMP_SWITCH,(last_actuators >> 1U) & 1U);
    digitalWrite(SOLAR_PUMP_SWITCH,bitRead(last_actuators,1));
  //} else if ( (PREV_MANUAL_STATE >> 1) & 1U ){
  } else if ( bitRead(PREV_MANUAL_STATE,1) ){
    digitalWrite(SOLAR_PUMP_SWITCH, LOW);
  } else {
    //SENSORS_DATA[SENSORS_COUNT-2] += (digitalRead(SOLAR_PUMP_SWITCH) << 1) & 0b00000010;
    bitWrite(SENSORS_DATA[SENSORS_COUNT-2], 1, digitalRead(SOLAR_PUMP_SWITCH));
  }
  
  if( (menu.getManualState() >> 2) & 1U ){
    SENSORS_DATA[SENSORS_COUNT-2] += (last_actuators & 0b00000100);
    digitalWrite(HEATER_SWITCH,(last_actuators >> 2) & 1U);
  //} else if ( (PREV_MANUAL_STATE >> 2) & 1U ){
  } else if ( bitRead(PREV_MANUAL_STATE,2) ){
    digitalWrite(HEATER_SWITCH, LOW);
  } else {
    bitWrite(SENSORS_DATA[SENSORS_COUNT-2], 2, digitalRead(HEATER_SWITCH));
    //SENSORS_DATA[SENSORS_COUNT-2] += (digitalRead(HEATER_SWITCH) << 2) & 0b00000100;
  }
  
  if( (menu.getManualState() >> 3) & 1U ){
    SENSORS_DATA[SENSORS_COUNT-2] += (last_actuators & 0b00001000);
    digitalWrite(HEATER_PUMP,(last_actuators >> 3) & 1U);
  } else {
    SENSORS_DATA[SENSORS_COUNT-2] += (digitalRead(HEATER_PUMP) << 3) & 0b00001000;
  }
  
  if( (menu.getManualState() >> 4) & 1U ){
    SENSORS_DATA[SENSORS_COUNT-2] += (last_actuators & 0b00010000);
    digitalWrite(CIRC_PUMP,(last_actuators >> 4) & 1U);
  } else {
    SENSORS_DATA[SENSORS_COUNT-2] += (digitalRead(CIRC_PUMP) << 4) & 0b00010000;
  }
  //DEBUG_PRINT("Actuators: |"); DEBUG_PRINTLN_BIN(SENSORS_DATA[SENSORS_COUNT-2]); 

  PREV_MANUAL_STATE = menu.getManualState();
}

void updateSetpoints(){
  int tmp[] = {0, 0, 0, 0, 0, 0, 0};
  int on, off; //FIXME remove
  for(int i=0; i<2; i++){
    EEPROM.get(EE_CIRC_T_ON + i*2,tmp[i]);
  }
  /*DEBUG_PRINT(F("CIRCULATION interval: ")); DEBUG_PRINT(tmp[0]);
  DEBUG_PRINT(F(", run: ")); DEBUG_PRINTLN(tmp[1]);*/
  circulationControll.setup(tmp[1],tmp[0]);

  int offset = 0;
  for(int i=0; i<5; i++){
    if(i > 2) offset = 6; //we need to offset because at EE_HEAT_TEMP + 6 is EE_SOL_TANK
    EEPROM.get(EE_HEAT_HYST + i*2 + offset,tmp[i]);
  }
  /*DEBUG_PRINT(F("HEATER expected: ")); DEBUG_PRINT(tmp[2]); DEBUG_PRINT(F(", critical: ")); DEBUG_PRINT(tmp[1]);
  DEBUG_PRINT(F(", hysteresis: ")); DEBUG_PRINT(tmp[0]); DEBUG_PRINT(F(", max: ")); DEBUG_PRINT(tmp[3]);
  DEBUG_PRINT(F(", min: ")); DEBUG_PRINTLN(tmp[4]);*/
  //heaterControll.setup(critical,minT,maxT,expected,hysteresis);
  heaterControll.setup(tmp[1],tmp[4],tmp[3],tmp[2],tmp[0]);

  EEPROM.get(EE_SOL_TANK,tmp[0]);
  for(int i=0; i<6; i++){
    EEPROM.get(EE_SOL_CRIT + i*2,tmp[i+1]);
  }
  /*DEBUG_PRINT(F("SOLAR tank: ")); DEBUG_PRINT(tmp[0]); DEBUG_PRINT(F(", critical: ")); DEBUG_PRINT(tmp[1]);
  DEBUG_PRINT(F(", T on: ")); DEBUG_PRINT(tmp[2]);  DEBUG_PRINT(F(", T off: ")); DEBUG_PRINT(tmp[3]);
  DEBUG_PRINT(F(", PWM T max: ")); DEBUG_PRINT(tmp[4]); DEBUG_PRINT(F(", PWM D min: ")); DEBUG_PRINT(tmp[5]);
  DEBUG_PRINT(F(", PWM D max: ")); DEBUG_PRINTLN(tmp[6]);*/
  //solarControll.setup(critical, tank, on, off, pwm_max_temp, min_duty_cycle, max_duty_cycle);
  solarControll.setup(tmp[1], tmp[0], tmp[2], tmp[3], tmp[4], tmp[5], tmp[6]);

  //FIXME is it used?
  for(int i=0; i<3; i++){
    EEPROM.get(EE_PID_P + i*2,tmp[i]);
  }
  /*DEBUG_PRINT(F(", PID P: ")); DEBUG_PRINT(tmp[0]); DEBUG_PRINT(F(", PID I: ")); DEBUG_PRINT(tmp[1]);
  DEBUG_PRINT(F(", PID D: ")); DEBUG_PRINTLN(tmp[2]);*/
  solarControll.setPID((double)(tmp[0]/100),(double)(tmp[1]/100),(double)(tmp[2]/100));

  for(int i=0; i<3; i++){
    EEPROM.get(EE_THRM1_B + i*2,tmp[i]);
  }
  /*DEBUG_PRINT(F("THERMISTOR 1 B: ")); DEBUG_PRINT(tmp[0]); DEBUG_PRINT(F(", R0: ")); DEBUG_PRINT(tmp[1]);
  DEBUG_PRINT(F(", T0: ")); DEBUG_PRINTLN(tmp[2]);*/
  solar.setup(tmp[0],tmp[1],tmp[2],ANALOG_PULLUP,false);
}

// -------------------------------- TEMPERATURE -------------------------------
int getTemperature(uint8_t* addr){
  double t = (digitalThermometers.getTempC(addr));
  if( t < -99) // DEVICE_DISCONNECTED
    return T_MIN*100; // TODO throw error on LCD
  t += calculateError(t); // ok?
  //t -= calculateError(t); //wrong?
  if(T_MIN < t){
    if( t < T_MAX){
      return (int)(t*100);
    } else {
      return (int)(T_MAX*100);
    }
  } else {
    return (int)(T_MIN*100);
  }
}

double calculateError(double T){
  // function based on lines from: http://www.maximintegrated.com/en/support/faqs/ds18b20.html
  // especially: http://www.maximintegrated.com/content/dam/files/support/product-faqs/DS18B20/DS18B20_temperature_sweep.xls
  // calculated best fit quadratic polynomial: E = 0.000149583039036227*T^2 - 0.0121293180775215*T + 0.0291768205934997
  return (int)(0.0001496*T*T - 0.0121293*T + 0.0291768);
}

int EMAsmoothing(int prev, int curr){
  if(curr > T_MAX*100 || curr < T_MIN*100){ // temperature this high and this low is not possible
    return prev;
  }
  // (3 * oldAverage + tempr)/4
  // no floating point operations
  return( (((prev<<1) + prev + curr) + 2) >> 2 );
}

bool assignDS18B20(){
  for(int idx=0; idx<DS18B20_COUNT; idx++){
    EEPROM.get(EE_DS18B20_1+idx*8,ds18b20Addresses[idx]);
    DEBUG_PRINT(F("DS18B20 Address from EEPROM: "));
    for(int j=0; j<8; j++){ DEBUG_PRINT(ds18b20Addresses[idx][j]);} DEBUG_PRINTLN();
  }
  return true;
}

bool assignDS18B20_withdetect(){
  bool ret = true;
  // check number of devices
  int no = digitalThermometers.getDeviceCount();

  // get addresses from EEPROM
  for(int idx=0; idx<DS18B20_COUNT; idx++){
    EEPROM.get(EE_DS18B20_1+idx*8,ds18b20Addresses[idx]);
    DEBUG_PRINTLN(F("DS18B20 Address from EEPROM: "));
    for(int j=0; j<8; j++){ DEBUG_PRINT(ds18b20Addresses[idx][j]);} DEBUG_PRINTLN();
  }

  if ( no != DS18B20_COUNT){
    Serial.print(F("Wrong number of devices: ")); Serial.println(no);
    DS18B20_COUNT = no;
    ret = false;
  }

  // detect new devices
  DeviceAddress device;
  oneWire.reset_search();
  while (oneWire.search(device)){
    if (digitalThermometers.validAddress(device)){
      if(! findInEEPROM(device)){
        if(ret) ret = false;
        break;
      }
    }
  }

  for(int idx=0; idx<DS18B20_COUNT; idx++){
    if (! digitalThermometers.isConnected(ds18b20Addresses[idx])){
      memcpy(ds18b20Addresses[idx],device,8);
      DEBUG_PRINT(F("DS18B20 New device detected: "));for(int j=0; j<8; j++){ DEBUG_PRINT(ds18b20Addresses[idx][j]); DEBUG_PRINT('.'); } DEBUG_PRINTLN();
      //EEPROM.put(EE_DS18B20_1+idx*8,ds18b20Addresses[idx]);
      //menu.error("Assigning address: ...... to device no. idx");
      if(ret) ret = false;
    }
    digitalThermometers.getAddress(ds18b20Addresses[idx],idx);
  }
  return ret;
}

bool findInEEPROM(DeviceAddress device){
  int j=0;
  for(int i=0; i<DS18B20_COUNT; i++){
    j=0;
    while(j<8){
      if(ds18b20Addresses[i][j] != device[j]){
        break;
      } else {
        j++;
      }
      if(j==8) return true; // Address found
    }
  }
  return false; // Address not found in EEPROM
}

// --------------------------------- ETHERNET ---------------------------------
bool reconnect() {
  // TODO check next 3 lines
  // do not go futher if we are connected
  //if (Ethernet.localIP()[0] != 0){
  //  return true;
  //}
  uint8_t ip[4];
  EEPROM.get(EE_ETH_IP,ip);
  DEBUG_PRINT(F("IP from EEPROM: "));
  DEBUG_PRINT(ip[0]); DEBUG_PRINT('.'); DEBUG_PRINT(ip[1]); DEBUG_PRINT('.');
  DEBUG_PRINT(ip[2]); DEBUG_PRINT('.'); DEBUG_PRINTLN(ip[3]);
  digitalWrite(CS_SD,HIGH); // disable SD card to avoid SPI mixup
  digitalWrite(CS_ETH,LOW); //
  if ((ip[0] == 192) || (ip[0] == 10) || (ip[0] == 176)){
    uint8_t dns[4];
    EEPROM.get(EE_ETH_DNS,dns);
    if (dns[0] != 0){
      uint8_t gateway[4];
      EEPROM.get(EE_ETH_GATE,gateway);
      if (gateway[0] != 0){
        uint8_t subnet[4];
        EEPROM.get(EE_ETH_SUBN,subnet);
        if ( subnet[0] != 0) {
          Ethernet.begin(MAC,ip,dns,gateway,subnet);
        } else {
          Ethernet.begin(MAC,ip,dns,gateway);
        }
      } else {
        Ethernet.begin(MAC,ip,dns);
      }
    } else {
      Ethernet.begin(MAC,ip);
    }
    
    DEBUG_PRINTLN(F("Connected to internet"));
    return true;
  } else {
    DEBUG_PRINTLN(F("Obtaining IP via DHCP"));
    //FIXME DHCP hangs all program (sometimes)
    if (Ethernet.begin(MAC) == 1){
      DEBUG_PRINTLN(Ethernet.localIP());
    } else {
      DEBUG_PRINTLN(F("Cannot connect to network"));
      return false;
    }
  }
}

// ----------------------------------- MQTT -----------------------------------
//TODO add posibility to change setpoints via MQTT and store them in EEPROM
void mqttCheckConn(bool force) {
  MQTT_STATUS = MQTT.connected();
  if (! MQTT_STATUS || force){
    DEBUG_PRINTLN(F("Connecting to MQTT Broker"));
    reconnect();
    delay(30);
    MQTT.connect((char*)DEVICE_ID);
    mqttSubscribe();
  }
}

void mqttSubscribe() {
  MQTT.subscribe("room/1/temp_current");
  MQTT.subscribe("room/1/temp_expected"); // save to EEPROM //duplicate with heater/settings/temp_expected
  char tmp[strlen(DEVICE_ID)+22];
  //char tmp[strlen(DEVICE_ID)+14];
  strcpy(tmp,DEVICE_ID);
  strcat(tmp,"/+/settings/#");
  MQTT.subscribe(tmp);
  DEBUG_PRINT("Subscribed to: '"); DEBUG_PRINT(tmp); DEBUG_PRINTLN("'");
  //MQTT.subscribe("solarControl/solar/settings/#");
  /*strcpy(tmp,DEVICE_ID);
  strcat(tmp,"/heater/settings/#");
  MQTT.subscribe(tmp);
  DEBUG_PRINT("Subscribed to: '"); DEBUG_PRINT(tmp); DEBUG_PRINTLN("'");
  strcpy(tmp,DEVICE_ID);
  strcat(tmp,"/tank/settings/#");
  MQTT.subscribe(tmp);
  DEBUG_PRINT("Subscribed to: '"); DEBUG_PRINT(tmp); DEBUG_PRINTLN("'");
  strcpy(tmp,DEVICE_ID);
  strcat(tmp,"/circulate/settings/#");
  MQTT.subscribe(tmp);
  DEBUG_PRINT("Subscribed to: '"); DEBUG_PRINT(tmp); DEBUG_PRINTLN("'");*/
}

//TODO need a better parser ASAP //maybe throw some JSON
// maybe use strncmp() instead of strcmp()?
// usage of strtok() should help // split topic in smaller chunks
void mqttSubCallback(char* topic, byte* payload, unsigned int length){
  int tmp;
  payload[length] = '\0';
  DEBUG_PRINT("MQTT rec: "); DEBUG_PRINT(topic); DEBUG_PRINT(" : ");
  for(int i=0; i<length; i++){ DEBUG_PRINT((char)payload[i]);}; DEBUG_PRINTLN();

  if (strcmp("room/1/temp_expected",topic) == 0){ // strcmp() returns 0 if strings are the same
    DEBUG_PRINT(F("setting expected temperature: "));
    setSetting(nameToAddress("expected",1),payload);
    return;
  } 
  if (strcmp("room/1/temp_current",topic) == 0){
    SENSORS_DATA[1] = (int)(atof((char*)payload)*100);
    DEBUG_PRINT(F("setting inside temperature (MQTT): ")); DEBUG_PRINTLN(SENSORS_DATA[1]);
    return;
  }

  if (strcmp(topic+strlen(DEVICE_ID)+17, MQTT_SETTINGS_HEATER[MQTT_SETTINGS_HEATER_LEN-1]) == 0){
    CAN_HEAT = (int)(atoi((char*)payload));
    return;
  }
  if (strncmp(DEVICE_ID,topic,strlen(DEVICE_ID)) == 0){
    uint8_t prefix = strlen(DEVICE_ID) + 1;
    char *id;
    DEBUG_PRINT("MQTT char: "); DEBUG_PRINTLN(topic[prefix]);
    if (strncmp(topic+strlen(DEVICE_ID)+1,"solar/settings/",15) == 0){
      prefix += 15;
      id = (char*)malloc(strlen(topic)-prefix);
      strcpy(id,topic+prefix);
      setSetting(nameToAddress(id,3),payload);
    } else if (strncmp(topic+prefix,"tank/settings/",14) == 0) {
      prefix += 14;
      id = (char*)malloc(strlen(topic)-prefix);
      strcpy(id,topic+prefix);
      DEBUG_PRINTLN(id);
      setSetting(nameToAddress(id,2),payload);
    } else if (strncmp(topic+prefix,"heater/settings/",16) == 0) {
      prefix += 16;
      id = (char*)malloc(strlen(topic)-prefix);
      strcpy(id,topic+prefix);
      DEBUG_PRINTLN(id);
      setSetting(nameToAddress(id,1),payload);
    } else if (strncmp(topic+prefix,"circulate/settings/",19) == 0) {
      prefix += 19;
      id = (char*)malloc(strlen(topic)-prefix);
      strcpy(id,topic+prefix);
      DEBUG_PRINTLN(id);
      setSetting(nameToAddress(id,0),payload);
    }
  }
  return;
}

void setSetting(uint8_t address, byte *val){
  DEBUG_PRINT("MQTT EEPROM Address: "); DEBUG_PRINTLN(address);
  int tmp;
  switch (address){
  case EE_CIRC_T_ON: case EE_CIRC_T_ON+2:
  case EE_PUMP_D1: case EE_PUMP_D1+2: case EE_HEAT_ON: case EE_HEAT_ON+2:
  //case EE_PID_P: case EE_PID_P+2: case EE_PID_P+4:
    tmp = (int)(atoi((char*)val));
    break;
  default:
    tmp = (int)(atof((char*)val)*100);
  }
  EEPROM.put(address,tmp);
}

uint8_t nameToAddress(char *id, int pos){
  int i;
  switch(pos){
  case 0:
    for(i=0; i<MQTT_SETTINGS_CIRC_LEN; i++){
      if(strcmp(id,MQTT_SETTINGS_CIRC[i]) == 0) break;
    }
    return EE_CIRC_T_ON + i*2;
    break;
  case 1:
    for(i=0; i<MQTT_SETTINGS_HEATER_LEN-1; i++){
      if(strcmp(id,MQTT_SETTINGS_HEATER[i]) == 0) break;
    }
    return EE_HEAT_HYST + i*2;
    break;
  case 2:
    for(i=0; i<MQTT_SETTINGS_TANK_LEN; i++){
      if(strcmp(id,MQTT_SETTINGS_TANK[i]) == 0) break;
    }
    return EE_SOL_TANK + i*2;
    break;
  case 3:
    for(i=0; i<MQTT_SETTINGS_SOLAR_LEN; i++){
      if(strcmp(id,MQTT_SETTINGS_SOLAR[i]) == 0) break;
    }
    if (i<3) return EE_SOL_CRIT+i*2;
    else if (i<7) return EE_SOL_CRIT+(i-1)*2;
    else return EE_PID_P+(i-7)*2;
    break;
  default:
    return EE_UNDEFINED;
  }
}

void mqttPublishData(){
  char val_buf[6];
  char name_buf[strlen(DEVICE_ID)+MAX_MQTT_TOPIC_LENGTH+1];

  //for non-retained message
  //MQTT.publish("room/1/temp_backup",buf);
  //for retained message
  //MQTT.publish("room/1/temp_backup",(uint8_t*)buf,strlen(buf),true);
  DEBUG_PRINTLN("MQTT PUBLISH TOPICS:");
  for(int i=0;i<SENSORS_COUNT;i++){
    if(i != 1){ //do not sent temperature in the room
      if(i == 2){
        strcpy(name_buf, MQTT_SENSORS[i]);
      } else {
        strcpy(name_buf, DEVICE_ID);
        strcat(name_buf,"/");
        strcat(name_buf, MQTT_SENSORS[i]);
      }
      //strcpy(name_buf, MQTT_SENSORS[i]);
      DEBUG_PRINT(" - "); DEBUG_PRINT(name_buf);
      if (i >= SENSORS_COUNT-3) itoa(SENSORS_DATA[i],val_buf,10);
      else dtostrf(SENSORS_DATA[i]/100.0, 3, 2, val_buf);
      DEBUG_PRINT(" value: "); DEBUG_PRINTLN(val_buf);
      MQTT.publish(name_buf,(uint8_t*)val_buf,strlen(val_buf),true);
    }
  }
}
//TODO FIXME wrong data!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
void mqttPublishSettings(){
  //messages not retained
  DEBUG_PRINTLN("MQTT Publishing settings");
  int tmp;
  char val_buf[6];
  char name_buf[strlen(DEVICE_ID)+MAX_MQTT_TOPIC_LENGTH+20];
  for(int i=0;i<MQTT_SETTINGS_CIRC_LEN;i++){
    strcpy(name_buf, DEVICE_ID);
    strcat(name_buf,"/circulate/settings/");
    strcat(name_buf, MQTT_SETTINGS_CIRC[i]);
    EEPROM.get(nameToAddress((char *)MQTT_SETTINGS_CIRC[i],0),tmp);
    itoa(tmp,val_buf,10);
    DEBUG_PRINT(name_buf); DEBUG_PRINT(" : "); DEBUG_PRINTLN(val_buf);
    MQTT.publish(name_buf,(char*)val_buf);
  }
  for(int i=0;i<MQTT_SETTINGS_HEATER_LEN;i++){
    strcpy(name_buf, DEVICE_ID);
    strcat(name_buf,"/heater/settings/");
    strcat(name_buf, MQTT_SETTINGS_HEATER[i]);
    //if(i == MQTT_SETTINGS_TANK_LEN-1) tmp = CAN_HEAT;
    if(i == MQTT_SETTINGS_HEATER_LEN-1) tmp = CAN_HEAT;
    else EEPROM.get(nameToAddress((char *)MQTT_SETTINGS_HEATER[i],1),tmp);
    if (i>2) itoa(tmp,val_buf,10);
    else dtostrf(tmp/100.0, 3, 2, val_buf);
    DEBUG_PRINT(name_buf); DEBUG_PRINT(" : "); DEBUG_PRINTLN(val_buf);
    MQTT.publish(name_buf,(char*)val_buf);
  }
  for(int i=0;i<MQTT_SETTINGS_TANK_LEN;i++){
    strcpy(name_buf, DEVICE_ID);
    strcat(name_buf,"/tank/settings/");
    strcat(name_buf, MQTT_SETTINGS_TANK[i]);
    //EEPROM.get(EE_SOL_TANK+i*2,tmp);
    EEPROM.get(nameToAddress((char *)MQTT_SETTINGS_TANK[i],2),tmp);
    dtostrf(tmp/100.0, 3, 2, val_buf);
    DEBUG_PRINT(name_buf); DEBUG_PRINT(" : "); DEBUG_PRINTLN(val_buf);
    MQTT.publish(name_buf,(char*)val_buf);
  }
  for(int i=0;i<MQTT_SETTINGS_SOLAR_LEN;i++){
    strcpy(name_buf, DEVICE_ID);
    strcat(name_buf,"/solar/settings/");
    strcat(name_buf, MQTT_SETTINGS_SOLAR[i]);
    EEPROM.get(nameToAddress((char *)MQTT_SETTINGS_SOLAR[i],3),tmp);
    //if (i<3) EEPROM.get(EE_SOL_CRIT+i*2,tmp);
    //else if (i<7) EEPROM.get(EE_SOL_CRIT+(i-1)*2,tmp);
    //else EEPROM.get(EE_PID_P+(i-7)*2,tmp);
    if(i == 5 || i == 6) itoa(tmp,val_buf,10);
    else dtostrf(tmp/100.0, 3, 2, val_buf);
    
    DEBUG_PRINT(name_buf); DEBUG_PRINT(" : "); DEBUG_PRINTLN(val_buf);
    MQTT.publish(name_buf,(char*)val_buf);
  }
}
/*
//FIXME TEMPORARY NOT USED
void mqttPublishSettings_old() {
  return;
  mqttSettingsCirculation();
  mqttSettingsHeater();
  mqttSettingsTank();
  mqttSettingsSolar();
  mqttSettingsFlow();
}

void mqttSettingsCirculation(){
  StaticJsonBuffer<JSON_OBJECT_SIZE(2)> jsonBuffer;
  char buff[34];
  int tmp = 0;
  JsonObject& root = jsonBuffer.createObject();
  EEPROM.get(EE_CIRC_T_ON,tmp);
  root["interval"] = tmp;
  EEPROM.get(EE_CIRC_T_OFF,tmp);
  root["work"] = tmp;
  MQTT.publish("circulation/settings",buff);
}

void mqttSettingsHeater(){
  StaticJsonBuffer<JSON_OBJECT_SIZE(5)> jsonBuffer;
  char buff[100];
  int tmp = 0;
  JsonObject& root = jsonBuffer.createObject();
  EEPROM.get(EE_HEAT_HYST,tmp);
  root["hysteresis"] = (float)(tmp/100);
  EEPROM.get(EE_HEAT_CRIT,tmp);
  root["critical"] = (float)(tmp/100);
  EEPROM.get(EE_HEAT_TEMP,tmp);
  root["expected"] = (float)(tmp/100);
  EEPROM.get(EE_HEAT_ON,tmp);
  root["time_on"] = tmp;
  EEPROM.get(EE_HEAT_OFF,tmp);
  root["time_off"] = tmp;
  MQTT.publish("heater/settings",buff);
}

void mqttSettingsTank(){
  StaticJsonBuffer<JSON_OBJECT_SIZE(3)> jsonBuffer;
  char buff[64];
  int tmp = 0;
  JsonObject& root = jsonBuffer.createObject();
  EEPROM.get(EE_SOL_TANK,tmp);
  root["solar_max"] = (float)(tmp/100);
  EEPROM.get(EE_HEAT_MAX,tmp);
  root["heater_max"] = (float)(tmp/100);
  EEPROM.get(EE_HEAT_MIN,tmp);
  root["heater_min"] = (float)(tmp/100);
  MQTT.publish("tank/settings",buff);
}

void mqttSettingsSolar(){
  StaticJsonBuffer<JSON_OBJECT_SIZE(3)> jsonBuffer;
  char buff[46];
  int tmp = 0;
  JsonObject& root = jsonBuffer.createObject();
  EEPROM.get(EE_SOL_CRIT,tmp);
  root["critical"] = (float)(tmp/100);
  EEPROM.get(EE_SOL_ON,tmp);
  root["on"] = (float)(tmp/100);
  EEPROM.get(EE_SOL_OFF,tmp);
  root["off"] = (float)(tmp/100);
  MQTT.publish("solar/settings",buff);
}

void mqttSettingsFlow(){
  StaticJsonBuffer<JSON_OBJECT_SIZE(7)> jsonBuffer;
  char buff[110];
  int tmp = 0;
  JsonObject& root = jsonBuffer.createObject();
  EEPROM.get(EE_PUMP_T1,tmp);
  root["T_min"] = (float)(tmp/100);
  EEPROM.get(EE_PUMP_T2,tmp);
  root["T_max"] = (float)(tmp/100);
  EEPROM.get(EE_PUMP_D1,tmp);
  root["D_min"] = tmp;
  EEPROM.get(EE_PUMP_D2,tmp);
  root["D_max"] = tmp;
  EEPROM.get(EE_PID_P,tmp);
  root["pid_P"] = (float)(tmp/100);
  EEPROM.get(EE_PID_I,tmp);
  root["pid_I"] = (float)(tmp/100);
  EEPROM.get(EE_PID_D,tmp);
  root["pid_D"] = (float)(tmp/100);
  MQTT.publish("flow/settings",buff);
}*/

// ------------------------------ SD CARD LOGGER ------------------------------
bool logData(){
  digitalWrite(CS_ETH,HIGH); // disable Ethernet to avoid SPI mixup
  digitalWrite(CS_SD,LOW);
  bool ret;
  // today's filename:
  char filename[] = "00-00-00.csv";
  sprintf(filename, "%02d-%02d-%02d.csv", year()-2000, month(), day());
  bool create = ! SD.exists(filename);
  File f = SD.open(filename, FILE_WRITE);
  if (f){
    if (create) {
      DEBUG_PRINTLN("Creating new file");
      f.println(F("Kolejnosc urzadzen: cyrkulacja, piec, co/cwu, pompa, zawor solarny"));
      f.println(F("Godzina;Minuta;Sekunda;T solar;T wewnatrz;T zewnatrz;T solar (in);T solar (out);T piec (in);T piec (out);T zbiornik (gora);Cyrkulacja;Urzadzenia;Przeplyw"));
      //f.println(pgm_read_word(&SD_FIRST_LINE[0]));
    }
    f.print(hour()); f.print(';');
    f.print(minute()); f.print(';');
    f.print(second()); f.print(';');
    f.flush();
    for(int i=0; i<SENSORS_COUNT-3;i++){
      f.print((float)SENSORS_DATA[i]/100);
      f.print(';');
      f.flush();
    }
    f.print(SENSORS_DATA[SENSORS_COUNT-3]); // circulation
    f.print(';');
    f.print(SENSORS_DATA[SENSORS_COUNT-2],BIN); // actuators
    f.print(';');
    f.flush();
    f.println(SENSORS_DATA[SENSORS_COUNT-1]); // flow
    f.close();
    DEBUG_PRINTLN("Data saved");
    ret = true;
  } else {
    free(filename);
    DEBUG_PRINTLN("Cannot open file on SD card");
    ret = false;
  }
  digitalWrite(CS_SD,HIGH);
  digitalWrite(CS_ETH,LOW); // revert
  
  return ret;
}

// ----------------------------------- TIME -----------------------------------
/* Francesco Potortì 2013 - GPLv3 - Revision: 1.13 */
//TODO FIXME use as prototype, move away from GPL
// http://playground.arduino.cc/Code/NTPclient
bool syncTime(){
  static int udpInited = udp.begin(123); // open socket on arbitrary port

  // Only the first four bytes of an outgoing NTP packet need to be set
  // appropriately, the rest can be whatever.
  const long ntpFirstFourBytes = 0xEC0600E3; // NTP request header

  // Fail if EthernetUdp.begin() could not init a socket
  if (! udpInited)
  //if( ! udp.begin(123))
    return false;
  //DEBUG_PRINTLN("TIME udp flush");
  // Clear received data from possible stray received packets
  udp.flush();

  // Send an NTP request
  uint8_t timeServer[4];
  EEPROM.get(EE_TIME_IP,timeServer);
  if (! (udp.beginPacket(timeServer, 123) // 123 is the NTP port
   && udp.write((byte *)&ntpFirstFourBytes, 48) == 48
   && udp.endPacket()))
    return false; // sending request failed

  //DEBUG_PRINTLN("TIME packet read");
  // Wait for response; check every pollIntv ms up to maxPoll times
  const int pollIntv = 150;   // poll every this many ms
  const byte maxPoll = 15;    // poll up to this many times
  int pktLen;       // received packet length
  for (byte i=0; i<maxPoll; i++) {
    if ((pktLen = udp.parsePacket()) == 48)
      break;
    delay(pollIntv);
  }
  if (pktLen != 48)
    return false;       // no correct packet received

  //DEBUG_PRINTLN("TIME packet OK");
  // Read and discard the first useless bytes
  // Set useless to 32 for speed; set to 40 for accuracy.
  const byte useless = 32;
  for (byte i = 0; i < useless; ++i)
    udp.read();

  // Read the integer part of sending time
  unsigned long time = udp.read();  // NTP time
  for (byte i = 1; i < 4; i++)
    time = time << 8 | udp.read();

  // Round to the nearest second if we want accuracy
  // The fractionary part is the next byte divided by 256: if it is
  // greater than 500ms we round to the next second; we also account
  // for an assumed network delay of 50ms, and (0.5-0.05)*256=115;
  // additionally, we account for how much we delayed reading the packet
  // since its arrival, which we assume on average to be pollIntv/2.
  time += (udp.read() > 115 - pollIntv/8);

  // Discard the rest of the packet
  udp.flush();
  udp.stop();
  //DEBUG_PRINT("OLD time: "); DEBUG_PRINTLN(now());
  int offset;
  EEPROM.get(EE_TIME_OFF,offset);
  RTC.set(time - 2208988800ul + offset*60); // convert NTP time to Unix time // + offset
  //DEBUG_PRINT("NEW time: "); DEBUG_PRINTLN(now());
  return true;
}
/*
bool syncTime_old(){
  if (udp.begin(8888) == 0) return false;
  byte pb[48];
  memset(pb, 0, 48); // zeroing pb
  pb[0] = 0b11100011;   // LI, Version, Mode
  pb[1] = 0;     // Stratum, or type of clock
  pb[2] = 6;     // Polling Interval
  pb[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  pb[12]  = 49; 
  pb[13]  = 0x4E;
  pb[14]  = 49;
  pb[15]  = 52;

  uint8_t srv[4];
  EEPROM.get(EE_TIME_IP,srv);
  udp.beginPacket(srv, 123); //NTP requests are to port 123
  udp.write(pb,48);
  udp.endPacket();
  DEBUG_PRINTLN("Time packet OK");
  delay(1000);
  
  if (udp.available()){
    udp.read(pb, 48);
    unsigned long t1, t2, t3, t4;
    t1 = t2 = t3 = t4 = 0;
    for (int i=0; i< 4; i++)
    {
      t1 = t1 << 8 | pb[16+i];
      t2 = t2 << 8 | pb[24+i];
      t3 = t3 << 8 | pb[32+i];
      t4 = t4 << 8 | pb[40+i];
    }
    float f1,f2,f3,f4;
    f1 = ((long)pb[20] * 256 + pb[21]) / 65536.0;      
    f2 = ((long)pb[28] * 256 + pb[29]) / 65536.0;      
    f3 = ((long)pb[36] * 256 + pb[37]) / 65536.0;      
    f4 = ((long)pb[44] * 256 + pb[45]) / 65536.0;
    const unsigned long seventyYears = 2208988800UL;
    t1 -= seventyYears;
    t2 -= seventyYears;
    t3 -= seventyYears;
    t4 -= seventyYears;

    t4 += 1; // delay(1000);
    if (f4 > 0.4) t4++;
    DEBUG_PRINT("OLD time: "); DEBUG_PRINTLN(now());
    RTC.set(t4);
    DEBUG_PRINT("NEW time: "); DEBUG_PRINTLN(now());
    udp.stop();
    //free(pb); //maybe not needed?
    return true;
  } else {
    DEBUG_PRINTLN("UDP not available");
    //free(pb); //maybe not needed?
    //DEBUG_PRINTLN("freed mem");
    udp.stop();
    DEBUG_PRINTLN("out of here");
    return false;
  }
}*/

// ----------------------------------- LCD ------------------------------------
void clearLine(uint8_t line){
  lcd.setCursor(0,line);
  for(int i=0; i<20; i++){ lcd.print(' '); }
  lcd.setCursor(0,line);
}

void newError(uint8_t* count, char* text){
  (*count)++;
  DEBUG_PRINT(F("ERROR: "));
  DEBUG_PRINTLN(text);
  if(*count<MAX_LINES){
    clearLine(*count);
    lcd.print(text);
  }
}

// FIXME better error handling
void showError(char* line1, char* line2){
  lcd.setBacklight(HIGH);
  lcd.clear();
  lcd.home();
  lcd.noCursor();
  lcd.noBlink();
  lcd.print(F("****** ERROR! ******"));
  lcd.setCursor(0,1);
  lcd.print(line1);
  lcd.setCursor(0,2);
  lcd.print(line2);
  lcd.setCursor(0,3);
  lcd.print("********************");
}

void error(char* err){
  uint8_t len = 20;
  if(strlen(ERROR_LINE[0]) != 0){
    strcpy(ERROR_LINE[0],err);
    return;
  }
  if(strlen(ERROR_LINE[1]) != 0){
    strcpy(ERROR_LINE[1],err);
    return;
  }
}

bool renderErrors(){
  if(strlen(ERROR_LINE[0]) == 0) return false;
  if(strlen(ERROR_LINE[1]) == 0){
    showError(ERROR_LINE[0]," ");
  } else {
    showError(ERROR_LINE[0],ERROR_LINE[1]);
  }
  return true;
}
