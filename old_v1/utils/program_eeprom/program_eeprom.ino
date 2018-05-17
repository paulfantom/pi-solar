#include <EEPROM.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include "eeprom_addr_.h"

// Program to set default values and store them in EEPROM
LiquidCrystal_I2C lcd(0x3F,2,1,0,4,5,6,7,3,POSITIVE);

void time() {
  // time server 92.86.106.228
  // tempus2.gum.gov.pl  212.244.36.228
  EEPROM.put(EE_TIME_IP  ,212); //FIXME why it is overriden?
  EEPROM.put(EE_TIME_IP+1,244);
  EEPROM.put(EE_TIME_IP+2,36);
  EEPROM.put(EE_TIME_IP+3,228);
  EEPROM.put(EE_TIME_OFF,(uint16_t)120); //GMT+2
}

void mqtt(){
  // MQTT
  EEPROM.put(EE_MQTT_IP,192);
  EEPROM.put(EE_MQTT_IP+1,168);
  EEPROM.put(EE_MQTT_IP+2,2);
  EEPROM.put(EE_MQTT_IP+3,2);
  
  EEPROM.put(EE_MQTT_PORT,1883);
  // reset MQTT user and pass (set to spaces) 
  // User & Pass fields are the same length
  /*for(int i=0; i<(EE_MQTT_PASS-EE_MQTT_USER)*2-1; i++){
    EEPROM.put(EE_MQTT_USER+i,' ');
  }*/
  EEPROM.put(EE_MQTT_USER,"Administrator ");
  EEPROM.put(EE_MQTT_PASS,"password      ");
}
/*
void mbus(){
  for(int i=0; i<6; i++){
    EEPROM.put(EE_MBUS1_ADDR+i*2,(uint16_t)0); // set addresses and medium to zero
                                               // medium '0' - water, '1' - glikol
  }
}*/

void ethernet(){
  for(int i=EE_ETH_IP; i<EE_ETH_IP+16; i++){ //fill ip
    EEPROM.put(i,0);
  }
}

//"room/1/temp_backup", "outside/temp", "solar/temp_in", "solar/temp_out", "heater/temp_in", "heater/temp_out", "tank/temp_up",
void ds18b20(){
  uint8_t address[7][8] = {
                            { 0x28, 0xFF, 0x89, 0xDB, 0x06, 0x00, 0x00, 0x34 },
                            { 0x28, 0x7C, 0xEC, 0xBF, 0x06, 0x00, 0x00, 0xDA },
                            { 0x28, 0xFF, 0xB9, 0x48, 0x15, 0x15, 0x01, 0x3A },
                            { 0x28, 0xFF, 0x1A, 0x18, 0x15, 0x15, 0x01, 0x9F },
                            { 0x28, 0xFF, 0x4D, 0x15, 0x15, 0x15, 0x01, 0xC6 },
                            { 0x28, 0xFF, 0x5A, 0xF5, 0x02, 0x15, 0x02, 0x70 },
                            { 0x28, 0xFF, 0x4C, 0x30, 0x04, 0x15, 0x03, 0xA7 },
                          };
    for (int i=0; i<7; i++){
    EEPROM.put(EE_DS18B20_1 + i*8, address[i]);  
  }
}

void thermistors(int count){
  // nominal temperature of 25 degrees
  for(int i=0; i<count; i++){
    EEPROM.put(EE_THRM1_T + i*6,(uint16_t)25);
  }
  EEPROM.put(EE_THRM1_B,3950); EEPROM.put(EE_THRM1_R,(uint16_t)100);
  if (count>1){
    EEPROM.put(EE_THRM1_B+6,3380); EEPROM.put(EE_THRM1_R+6,(uint16_t)10);
  }
  if (count>2){
    EEPROM.put(EE_THRM1_B+12,3380); EEPROM.put(EE_THRM1_R+12,(uint16_t)10);
  }
}

void pump(){
  // pump calibration
  EEPROM.put(EE_PUMP_DMIN,(uint16_t)0);
  EEPROM.put(EE_PUMP_D2,(uint16_t)100);
  EEPROM.put(EE_PUMP_D1,(uint16_t)0);
  EEPROM.put(EE_PUMP_T2,(uint16_t)900); // 9.00 degree
  EEPROM.put(EE_PUMP_T1,(uint16_t)200); // 2.00 degree
  EEPROM.put(EE_PUMP_HYST,(uint16_t)20); // 0.2 degree
}

void heater(){
  // heater
  EEPROM.put(EE_HEAT_TEMP,(uint16_t)2100);
  EEPROM.put(EE_HEAT_HYST,(uint16_t)40);   //0.4   deg
  EEPROM.put(EE_HEAT_CRIT,9000);  //90.00 deg
  //EEPROM.put(EE_HEAT_MIN,4000); //40.00 deg
  //EEPROM.put(EE_HEAT_MAX,5500); //55.00 deg
  /*EEPROM.put(EE_HEAT_ON,   (uint8_t)06);    //06:00  //24 hour clock
  EEPROM.put(EE_HEAT_ON+1, (uint8_t)00);    //06:00
  EEPROM.put(EE_HEAT_OFF,  (uint8_t)08);   //08:00
  EEPROM.put(EE_HEAT_OFF+1,(uint8_t)00);   //08:00*/
  EEPROM.put(EE_HEAT_ON,(uint16_t)600); //06:00
  EEPROM.put(EE_HEAT_OFF,(uint16_t)800); //08:00
}

void solar(){
  EEPROM.put(EE_SOL_CRIT,15000); //150.00 deg
  EEPROM.put(EE_SOL_ON,(uint16_t)400);
  EEPROM.put(EE_SOL_OFF,(uint16_t)200);
}

void tank(){
  EEPROM.put(EE_SOL_TANK,6500);
  EEPROM.put(EE_SOL_TANK+2,5500);
  EEPROM.put(EE_SOL_TANK+4,4000);
}

void circ(){
  EEPROM.put(EE_CIRC_T_ON,(uint16_t)1800);
  EEPROM.put(EE_CIRC_T_OFF,(uint16_t)20);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  lcd.begin (20,4);
  lcd.setBacklight(HIGH);
  lcd.clear();
  lcd.home();
  lcd.noCursor();
  
  time();
  mqtt();
  ethernet();
  ds18b20();
  thermistors(1);
  pump();
  heater();
  solar();
  tank();
  circ();

  lcd.setCursor(0,0);
  lcd.print("EEPROM set to");
  lcd.setCursor(0,1);
  lcd.print("Factory defaults");
  Serial.println("FINITO");
  pinMode(13,OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(13,LOW);
  delay(200);
  digitalWrite(13,HIGH);
  delay(200);
}
