#ifndef Circulation_h
#define Circulation_h
#include <Arduino.h>
#include <Time.h>

class Circulation {
  public:
    Circulation(int pin);
    
    void update(boolean flow){_water_consumption = flow;}
    void run();
    void interrupt(bool state);
    void setup(unsigned int run_time, unsigned int interval);
    void manualMode(bool state);
    bool getState() { return digitalRead(_pump_pin); };
    // interval >> run_time

    
  private:
    int  _pump_pin;
    // user defined values:
    unsigned int _time_running;  // pump running time (time in seconds)
    unsigned int _time_interval; // interval between powering pump on (time in seconds)
    
    // time of last switching pump on;
    unsigned long int _last_on;
    
    // values from sensors
    boolean _water_consumption;
    
    void _pumpOn();
    void _pumpOff(){ digitalWrite(_pump_pin,LOW); }
};

#endif