#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define DEBUG Serial

unsigned int EE_DS18B20_1 = 41;

int ONE_WIRE_BUS = 24;

OneWire oneWire(ONE_WIRE_BUS);

// ----- DS18B20 -----
DallasTemperature digitalThermometers(&oneWire);
uint8_t DS18B20_COUNT = 7;
DeviceAddress ds18b20Addresses[7]; // ds18b20Addresses[DS18B20_COUNT]; 

uint8_t ds18b20s[7][8] = {
                          { 0x28, 0xFF, 0x89, 0xDB, 0x06, 0x00, 0x00, 0x34 },
                          { 0x28, 0x7C, 0xEC, 0xBF, 0x06, 0x00, 0x00, 0xDA },
                          { 0x28, 0xFF, 0xB9, 0x48, 0x15, 0x15, 0x01, 0x3A },
                          { 0x28, 0xFF, 0x1A, 0x18, 0x15, 0x15, 0x01, 0x9F },
                          { 0x28, 0xFF, 0x4D, 0x15, 0x15, 0x15, 0x01, 0xC6 },
                          { 0x28, 0xFF, 0x5A, 0xF5, 0x02, 0x15, 0x02, 0x70 },
                          { 0x28, 0xFF, 0x4C, 0x30, 0x04, 0x15, 0x03, 0xA7 },
                        };

void setup(){
  DEBUG.begin(9600);
  assignDS18B20();
  for(int i=0; i<7; i++){
    for(int j=0;j<8; j++){ DEBUG.print(ds18b20s[i][j]); DEBUG.print("."); }
    DEBUG.println();
    for(int j=0;j<8; j++){ DEBUG.print(ds18b20Addresses[i][j]); DEBUG.print("."); }
    DEBUG.println();
    DEBUG.println("------------------------------------");
  }
  digitalThermometers.begin();
  digitalThermometers.setWaitForConversion(true); //ASYNC mode
  digitalThermometers.setResolution(12);
}

void loop(){
  digitalThermometers.requestTemperatures();

  DEBUG.println("T wewnatrz,T zewnatrz,T solar (in),T solar (out),T piec (in),T piec (out),T zbiornik (gora)");
  for(int i=1; i<=DS18B20_COUNT; i++){
    
    /*DEBUG.print("ADDRESS: "); 
    for(int j=0; j<8; j++){
      DEBUG.print(ds18b20Addresses[i-1][j],HEX); DEBUG.print('.');
    }
    DEBUG.println();*/
    DEBUG.print("  VALUE: "); DEBUG.println(getTemperature(ds18b20Addresses[i-1])/100.0);
  }
  DEBUG.println();
  delay(10000);
}

int getTemperature(uint8_t* addr){
  double t = (digitalThermometers.getTempC(addr));
  //t += calculateError(t);
  t -= calculateError(t);
  return (int)(t*100);
}

double calculateError(double T){
  // function based on lines from: http://www.maximintegrated.com/en/support/faqs/ds18b20.html
  // especially: http://www.maximintegrated.com/content/dam/files/support/product-faqs/DS18B20/DS18B20_temperature_sweep.xls
  // calculated best fit quadratic polynomial: E = 0.000149583039036227*T^2 - 0.0121293180775215*T + 0.0291768205934997
  return (int)(0.0001496*T*T - 0.0121293*T + 0.0291768);
  
}

bool assignDS18B20(){
  bool ret = true;
  // check number of devices
  int no = digitalThermometers.getDeviceCount();

  // get addresses from EEPROM
  for(int idx=0; idx<DS18B20_COUNT; idx++){
    EEPROM.get(EE_DS18B20_1+idx*8,ds18b20Addresses[idx]);
    DEBUG.println("DS18B20 Address from EEPROM: ");
    for(int j=0; j<8; j++){ DEBUG.print(ds18b20Addresses[idx][j]);} DEBUG.println();
  }

  if ( no != DS18B20_COUNT){
    Serial.print("Wrong number of devices: "); Serial.println(no);
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
      DEBUG.print("DS18B20 New device detected: ");for(int j=0; j<8; j++){ DEBUG.print(ds18b20Addresses[idx][j]); DEBUG.print('.'); } DEBUG.println();
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
