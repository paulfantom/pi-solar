#include <Arduino.h>
#include <Heater.h>

Heater::Heater(int pump_pin, int switch_pin, int burner_pin) {
  _pump_pin   = pump_pin;
  _switch_pin = switch_pin;
  _burner_pin = burner_pin;
  pinMode(_pump_pin,OUTPUT);
  pinMode(_switch_pin,OUTPUT);
  pinMode(_burner_pin,OUTPUT);
  digitalWrite(_pump_pin,LOW);
  digitalWrite(_switch_pin,LOW);
  digitalWrite(_burner_pin,LOW);
}

void Heater::setup(int critical, int min, int max, int room, int hysteresis) {
  _temperature_critical = critical;
  _temperature_tank_min = min;
  _temperature_tank_max = max;
  _temperature_room     = room;
  _hysteresis           = hysteresis;
}

void Heater::update(int T5, int T8, int T9, boolean schedule, boolean solar) {
  _T5 = T5;
  _T8 = T8;
  _T9 = T9;
  _is_time = schedule;
  //prevent changing value when in water heating mode, otherwise can cause unintended behaviour
  if( _isSwitchInHeating() ){
    _only_solar = solar;
  }
}

boolean Heater::_checkAlarm() {
  if(_T5 < _temperature_critical + _hysteresis/2){
    if(_T5 < _temperature_critical - _hysteresis/2){
      return false;
    }
  } else {
    _switchToHeating();
    _pumpOff();
    _burnerOff();
  }
  return true;
}

boolean Heater::_isTime() {
  if(_is_time){
    return true;
  } else {
    _switchToHeating();
    _burnerOff();
    _pumpOff();
    return false;
  }
}

boolean Heater::_waterFromSolar() {
  return _only_solar;
  //maybe apply ANN with ART here?
  /*if(_only_solar){
    return true;
  } else {
    return false;
  }*/
}

void Heater::_centralHeating(){
  if(_T9 >= (_temperature_room + (_hysteresis/2))){
    _burnerOff();
    _pumpOff();
  } else if(_T9 <= (_temperature_room - (_hysteresis/2))){
    _pumpOn();
    _burnerOn();
  }
}

void Heater::run() {
  //Something wrong, turn all off and exit
  if( _checkAlarm()) {
    _burnerOff();
    _pumpOff();
    _switchToHeating();
    return;
  }
  
  //Keep low temperature in the building
  if( !_isTime()) {
    _switchToHeating();  
    _centralHeating();
    return;
  }
  
  // algorithm
  if( _waterFromSolar() ){
    _switchToHeating();
    _centralHeating();
  } else {
    if( _T8 < _temperature_tank_min ){
      _switchToWater();
      _pumpOn();
      _burnerOn();
      /*Serial.print("HEATER: ");
      Serial.print(_T8);
      Serial.print(" < ");
      Serial.print(_temperature_tank_min);
      Serial.print(" max: ");
      Serial.println(_temperature_tank_max);*/
    } else {
      if( _T8 < _temperature_tank_max){
        if(_isSwitchInHeating()){
          _centralHeating();
        }
      } else {
        _switchToHeating();
        _centralHeating();
      }
    }
  }
}