#include <Arduino.h>
#include <Solar.h>
#include <Time.h>

Solar::Solar(int pump_pin, int switch_pin) {
  Solar(pump_pin, pump_pin, switch_pin);
}

Solar::Solar(int pump_pin, int pump_switch_pin, int switch_pin) {
  this->_valve_pin = pump_pin;
  this->_switch_pin = switch_pin;
  this->_pump_pin = pump_switch_pin;
  pinMode(_pump_pin,OUTPUT);
  pinMode(_valve_pin,OUTPUT);
  pinMode(_switch_pin,OUTPUT);
  digitalWrite(_switch_pin,LOW);
  digitalWrite(_pump_pin,LOW);
  analogWrite(_valve_pin,0);
  this->_last = now();
  // setup()
}

void Solar::setup(int solar_critical, int tank, int on, int off, int pwm_max_temp, uint8_t min_duty_cycle, uint8_t max_duty_cycle) {
  this->_temperature_critical  = solar_critical;
  this->_temperature_tank      = tank;
  this->_temperature_solar_on  = on;
  
  this->_temperature_solar_off = off;
  //this->_temperature_solar_min_PWM = off;
  this->_temperature_solar_max_PWM = pwm_max_temp;
  this->_pwm_min_duty_cycle = min_duty_cycle; //*255/100;
  this->_pwm_max_duty_cycle = max_duty_cycle; //*255/100;
}

void Solar::setPID(double Kp, double Ki, double Kd) {
  this->_Kp = Kp;
  this->_Ki = Ki;
  this->_Kd = Kd;
}

void Solar::update(int solar, int tank, int input, int output) {
  _T1 = solar;
  _T2 = input;
  _T3 = output;
  _T8 = tank;
  //if(_T1 < _T2) _T = _T1;
  //else _T = _T2;
}

void Solar::_pumpOn(){
  // detect previous pump state (ON/OFF)
  if (digitalRead(_pump_pin) == LOW){
    digitalWrite(_pump_pin, HIGH);
    analogWrite(_valve_pin, _pwm_max_duty_cycle);
    _state = true;
  }
}

void Solar::_pumpOff(){
  _pwm = 0;
  analogWrite(_valve_pin,0);
  digitalWrite(_pump_pin,LOW); 
  _state = false;
  _integral = 0; //FIXME is it needed here?
}

uint8_t Solar::getPWM(){
  return _pwm;
}
/*
void Solar::_setPWM() {
  if(! _state){
    _pwm = 0;
    return;
  }
  int curr = _T2 - _T3;
  //Serial.print("SOLAR delta: "); Serial.println(curr);
  if(curr >= _temperature_solar_max_PWM){
    _pwm = _pwm_max_duty_cycle;
    analogWrite(_valve_pin,(uint8_t)(_pwm_max_duty_cycle*255/100));
    return;
  }
  //  compute 'a' and 'b' for linear function:
  //  PWM_duty = a * curr_temperature + b
  float a = (float)(_pwm_max_duty_cycle - _pwm_min_duty_cycle) / (float)(_temperature_solar_max_PWM - _temperature_solar_off);
  //float b = _pwm_max_duty_cycle - _temperature_solar_max_PWM * a;
  float b = _pwm_min_duty_cycle - _temperature_solar_off * a;
  
  //Serial.print("SOLAR a: "); Serial.println(a);
  //Serial.print("SOLAR b: "); Serial.println(b);
  
  float pwm = a * curr + b; // from 0 to 100
  //Serial.print("SOLAR value i: "); Serial.println(pwm);
  pwm = (3*_pwm + pwm)/4; //EMA smoothing
  analogWrite(_valve_pin,(uint8_t)(pwm*255/100));
  _pwm = (uint8_t)pwm;
}
*/

int Solar::_computePID(int input, int setpoint) {
  double out = 0;
  unsigned int interval = now() - _last;
  int error = setpoint - input;
  _integral += error * interval;
  //if(_integral > _pwm_max_duty_cycle) _integral = _pwm_max_duty_cycle;
  //else if(_integral < _pwm_min_duty_cycle) _integral = _pwm_min_duty_cycle;
  
  int derivative = (error - _prev_error) / (interval * 1.0);
  _prev_error = error;
  
  out = _Kp * error + _Ki * _integral + _Kd * derivative;
  
  return (int)out;
}

void Solar::_setPWM() {
  //prevent PWM when pump is not ON
  if (! _state){
    return;
  }
  
  int dT = _T - _T3;
  if(dT >= _temperature_solar_max_PWM){
    _pwm = _pwm_max_duty_cycle;
    analogWrite(_valve_pin,(uint8_t)(_pwm_max_duty_cycle*255.0/100.0));
    return;
  }
  //int dT2 = _T1 - _T3;
  
  _pwm = _tempToSignal(dT);

  if(_pwm > _pwm_max_duty_cycle) _pwm = _pwm_max_duty_cycle;
  else if(_pwm < _pwm_min_duty_cycle) _pwm = _pwm_min_duty_cycle;
  
  //_setPWM(_pwm);
  analogWrite(_valve_pin,(uint8_t)(_pwm*255.0/100.0));
}

void Solar::_setPWM(uint8_t val) {
  if(val > 100) val = 100;
  analogWrite(_valve_pin,(uint8_t)(val*255.0/100.0));
  _pwm = val;
}

uint8_t Solar::_tempToSignal(int T){
  //  compute 'a' and 'b' for linear function:
  //  PWM_duty = a * curr_temperature + b
  float a = (float)(_pwm_max_duty_cycle - _pwm_min_duty_cycle) / (float)(_temperature_solar_max_PWM - _temperature_solar_off);
  //float b = _pwm_max_duty_cycle - _temperature_solar_max_PWM * a;
  float b = _pwm_min_duty_cycle - _temperature_solar_off * a;
  return (uint8_t)(a * T + b); // from 0 to 100
}

void Solar::run() {
  // check criticals
  if( _T1 >= _temperature_critical || _T8 > _temperature_tank){
    _pumpOff();
    _switchOff();
    _pwm = 0;
    return;
  }
  
  // do not heat solar panel
  if( _T3 > _T1 && (_T2 - _T3) < 60 ){ // 0.6 degree difference
    _pumpOff();
    _switchOff();
    _pwm = 0;
    return;
  }
  
  // check if it can start
  if( (_T1 - _T3) > _temperature_solar_on){
    _switchOn();
    _pumpOn();
    _counter = 0;
  }
  
  /*if(_Kp == 0 && _Ki == 0){
    //take lower value
    if(_T1 < _T2) _T = _T1;
    else _T = _T2;
  } else {
    //PID as alterative to above
    _T = _computePID(_T2,_T1);
  }*/
  _T = (_T1 + _T2) / 2;
  
  // make it run full or _pwm_min_duty_cycle
  if( (_T - _T3) >= _temperature_solar_off){
    _setPWM(); 
  } else {
    //reset PID
    _integral = 0;
    //30 minutes low flow cycle
    if (_counter < 30*60){
      _counter += now() - _last;
      _setPWM(_pwm_min_duty_cycle);
    } else {
      _switchOff();
      _pumpOff();
    }
  }
  
  _last = now();
}