#ifndef Solar_h
#define Solar_h

#include <Arduino.h>

class Solar {
  // all temperatures are multipled by 100 to avoid using floats
  public:
    //Solar(int pump_pin);
    Solar(int pump_pin, int switch_pin);
    Solar(int pump_pin, int pump_switch_pin, int switch_pin);
    
    void setup(int solar_critical, int tank, int on, int off, int pwm_max_temp, uint8_t min_duty_cycle, uint8_t max_duty_cycle);
    void update(int solar, int tank, int input, int output);
    void setPID(double Kp, double Ki, double Kd);
    void run();
    void setCriticalDuty(uint8_t duty){_pwm_critical_duty_cycle = duty;}
    uint8_t getPWM();
    
  private:
    int _valve_pin;
    int _switch_pin;
    int _pump_pin;
    bool _state;
    unsigned long int _last = 0;
    unsigned int _counter = 0;
    int _T;
    
    // PID
    double _Kp = 0;
    double _Ki = 0;
    double _Kd = 0;
    
    double _integral = 0;
    int _prev_error = 0;
    
    // user defined values:
    int _temperature_critical;
    int _temperature_tank;
    int _temperature_solar_on;
    int _temperature_solar_off;
    //int _temperature_solar_min_PWM = _temperature_solar_off;
    int _temperature_solar_max_PWM;
    uint8_t _pwm_min_duty_cycle;
    uint8_t _pwm_max_duty_cycle;
    uint8_t _pwm_critical_duty_cycle;
    
    // values from sensors
    int _T1;
    int _T2;
    int _T3;
    int _T8;
    
    uint8_t _pwm = 0;
    
    int _computePID(int input, int output);
    uint8_t _tempToSignal(int T);
    void _setPWM(); // PWM freq = 490Hz, pin 2-13 and 44-46
    void _setPWM(uint8_t val);
    void _pumpOn();
    void _pumpOff();
    void _switchOn(){ digitalWrite(_switch_pin,HIGH); }
    void _switchOff(){ digitalWrite(_switch_pin,LOW); }
};

#endif