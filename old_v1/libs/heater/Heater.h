#ifndef Heater_h
#define Heater_h

#include <Arduino.h>

class Heater {
  // all temperatures are multipled by 100 to avoid using floats
  public:
    Heater(int pump_pin, int switch_pin, int burner_pin);
    
    void setup(int critical, int min, int max, int room, int hysteresis);
    void update(int T5, int T8, int T9, boolean schedule, boolean solar);
    void run();
    void setRoomTemp(int temp){ _temperature_room = temp;}
    
  private:
    int _pump_pin;
    int _switch_pin;
    int _burner_pin;
    // user defined values
    int _temperature_critical;
    int _temperature_tank_min;
    int _temperature_tank_max;
    int _temperature_room;
    int _hysteresis;
    
    // values from sensors
    int _T5;
    int _T8;
    int _T9;
    boolean _is_time;
    boolean _only_solar;
    
    // methods
    void _pumpOn(){ digitalWrite(_pump_pin,HIGH); }
    void _pumpOff(){ digitalWrite(_pump_pin,LOW); }
    void _switchToWater(){ digitalWrite(_switch_pin,HIGH); }
    void _switchToHeating(){ digitalWrite(_switch_pin,LOW); }
    void _burnerOn(){ digitalWrite(_burner_pin,HIGH); }
    void _burnerOff(){ digitalWrite(_burner_pin,LOW); }
    
    boolean _isSwitchInHeating(){ return (! digitalRead(_switch_pin)); };
    boolean _checkAlarm();
    boolean _isTime();
    boolean _waterFromSolar();   // check if solar is capable of heating water;
    void _centralHeating();
};

#endif
