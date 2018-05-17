#include <Arduino.h>
#include <Time.h>
#include <Circulation.h>

Circulation::Circulation(int pin) {
  _pump_pin = pin;
  pinMode(_pump_pin,OUTPUT);
  digitalWrite(_pump_pin,LOW); // initialize with pump OFF
  _last_on = 1440608400; // 2015-08-26 17:00 UTC - because why not?
}

void Circulation::setup(unsigned int run_time, unsigned int interval) {
  //run_time in seconds, interval in minutes
  _time_running = run_time;
  _time_interval = interval*60 + run_time;
}

void Circulation::_pumpOn(){
  digitalWrite(_pump_pin,HIGH);
  _last_on = now();
}

void Circulation::run() {
  unsigned long int elapsed = now() - _last_on;
  if(elapsed > _time_interval){
    if(_water_consumption){
      _pumpOn();
    } else {
      _pumpOff();
    }
  } else if (elapsed > _time_running){
    _pumpOff();
  }
}

void Circulation::interrupt(bool state){
  update(state);
  run();
}

void Circulation::manualMode(bool state){
  if(state){
    _pumpOn();
  } else {
    _pumpOff();
  }
}