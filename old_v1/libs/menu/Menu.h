#ifndef Menu_h
#define Menu_h
#include <Arduino.h>
#include "Keypad.h"
#include "LCD.h"
#include "LiquidCrystal_I2C.h"
#include "Time.h"
#include "EEPROM.h"
#include "eeprom_addr.h"

class Menu {
  public:
    Menu(Keypad &keypad, LiquidCrystal_I2C &lcd, uint8_t rows, uint8_t cols);
    void setup(int16_t *data_array, uint8_t array_size);
    void setup(int16_t *data_array,uint8_t array_size, uint16_t wait_time, uint32_t time_now);
    bool loop();
    bool loop(bool);
    void resetManual();
    uint8_t getManualState();
    unsigned int getManualTime();
    void decrementManual();
    bool newSettings();
    
  private:
    LiquidCrystal_I2C *lcd;
    Keypad *keypad;
    uint8_t _rows;
    uint8_t  _cols;
    int _current_menu = -2;
    int _last_menu = -2;
    uint8_t _selected_row = 0;
    boolean _submenu_has_values = false;
    uint32_t _begin_time = 0;
    uint16_t _wait_time;
    bool _menu_value_changed;
    uint8_t _manual_mode;
    unsigned int _manual_mode_time;
    bool _changed_values = false;
    
    int16_t *_data;
    uint8_t _data_size;
    
    void printScreen(const char screenArray[][19]);
    void printScreen(const char screenArray[][19], int8_t col);
    void renderCurrentValues();
    void renderCurrentValues(int8_t flag); //FIXME remove this
    uint8_t renderEEPROMValues();
    uint8_t renderEEPROMValues(int flag); //FIXME remove this
    uint8_t renderValues();
    uint8_t renderValues(int flag); //FIXME remove this
    bool renderMenu();
    bool renderMenu(int flag); //FIXME remove this
    bool changeValue(int8_t row, uint8_t eeprom_addr);
    bool changeValue(int8_t row, int flag, uint8_t eeprom_addr); //FIXME remove this
    int16_t setValue(int val, uint8_t len, uint8_t row);
    int16_t setValue(int val, uint8_t len, uint8_t row, int col);
    boolean setValue(char* buf, int len, uint8_t row);
    boolean setValue(char* buf, int len, uint8_t row, uint8_t col);
    
    void renderVal(bool val, uint8_t row); // render bool
    void renderVal(bool val, uint8_t row, uint8_t pre_clear); // render bool w/ clearing
    void renderVal(int val, uint8_t row, bool to_float); // render Float
    void renderVal(int val, uint8_t row); //render int
    void renderVal(uint8_t arr[4], uint8_t row); // render IP
    void renderVal(char arr[14], uint8_t row); // render User, password
};

const uint8_t WAIT_TIME = 600; //seconds
const uint8_t MAX_LINES = 4;

// menu lines (all lines are prepended with double whitespace)
const PROGMEM char BLANK[MAX_LINES][19] = {};
const PROGMEM char MAIN_MENU[MAX_LINES][19] = {"Dane z czujnikow","Nastawy", "Ustawienia", "Tryb manualny"};

const PROGMEM char SENSORS_MENU[MAX_LINES*3][19] = {"Zawor kolektor", "T kolektor", "T sol (zas)", "T sol (pow)",
                                                    "T piec (zas)", "T piec (pow)", "T zb. (gora)","Stan cyrk.", 
                                                    "T wewnetrzna","T zewnetrzna"};
 
const PROGMEM char SETPOINTS_MENU[MAX_LINES][19]= {"Solar", "Zbiornik CWU", "Piec (CO)", "Cyrkulacja"};
 const PROGMEM char SOLAR_DESC[MAX_LINES][19] = {"Krytyczna","Delta ON", "Delta OFF"};
 const PROGMEM char TANK_DESC[MAX_LINES][19] = {"T max (kol.)", "T max (piec)", "T min (piec)"};
 const PROGMEM char HEATER_DESC[MAX_LINES][19] = {"T krytyczna", "T oczekiwana", "wlacz o", "wylacz o"};
 const PROGMEM char CIRC_DESC[MAX_LINES][19] = {"przerwa [min]", "praca [s]"};

const PROGMEM char SETTINGS_MENU[MAX_LINES][19] = {"Termistory", "Czujniki DS18B20", "Regulacja przeplyw", "Inne ustawienia"};
 const PROGMEM char THERMISTORS_SUBMENU[MAX_LINES][19] = {"Kolektor", "Zewnatrz", "Wewnatrz"};
  const PROGMEM char THERMISTOR_DESC[MAX_LINES][19] = {"B [Kelvin]", "R0 [kOhm]", "T0 [Celsius]"};
 const PROGMEM char DS18B20_SUBMENU[MAX_LINES][19] = {"Wew/zew", "Solar", "Piec", "Zbiornik"};
  const PROGMEM char DS18B20_DESC[MAX_LINES][19] = {"I1", "I2", "O1", "O2"};
// const PROGMEM char MBUS_SUBMENU[MAX_LINES][19] = {"Solar", "Zbiornik CWU", "Piec"};
//  const PROGMEM char MBUS_DESC[MAX_LINES][19] = {"Adres", "Medium"};
 const PROGMEM char VALVE_SUBMENU[MAX_LINES][19] = {"PWM Zawor", "PID Kolektor"};
  const PROGMEM char PWM_DESC[MAX_LINES][19] = {"Temp.  (min)", "Temp.  (max)", "Sygnal (min)", "Sygnal (max)"};
  const PROGMEM char PID_DESC[MAX_LINES][19] = {"Kp", "Ki", "Kd"};
 const PROGMEM char OTHER_SUBMENU[MAX_LINES][19] = {"Ustawienia IP", "MQTT", "Czas", "Detekcja DS18B20"};
  const PROGMEM char IP_DESC[MAX_LINES][19] = {"IP", "DN", "GW", "SN"};
  const PROGMEM char MQTT_DESC[MAX_LINES][19] = {"IP", "Port", "Usr", "Pas"};
  const PROGMEM char TIME_DESC[MAX_LINES][19] = {"IP", "Offset [min]", "Czas"};
  const PROGMEM char DETECT_DESC[MAX_LINES][19] = {"No idea", "how I can", "do it."};

const PROGMEM char MANUAL_MENU[MAX_LINES][19] = {"Solar", "Piec + Cyrk.", "Ustawienia" };
 const PROGMEM char MANUAL_SUBMENU_1[MAX_LINES][19]   = { "Zawor reg.", "Zawor ON/OFF", "Pompa" };
 const PROGMEM char MANUAL_SUBMENU_2[MAX_LINES][19]   = { "CO/CWU", "Piec", "Cyrkulacja" };
 const PROGMEM char MANUAL_SUBMENU_OPT[MAX_LINES][19] = { "Czas [s]", "Status" };
 
#endif