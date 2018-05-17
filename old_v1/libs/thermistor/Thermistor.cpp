#include <Arduino.h>
#include <Thermistor.h>
#include <math.h>

Thermistor::Thermistor(int8_t pin){
  _init(pin,0,10);
}

Thermistor::Thermistor(int8_t pin, uint8_t size){
  _init(pin,0,size);
}

Thermistor::Thermistor(int8_t pin, int8_t aux_pin, uint8_t size){
  _init(pin,aux_pin,size);
}

void Thermistor::_init(int8_t pin, int8_t aux_pin, uint8_t size){
  this->_pin = pin;
  this->_pin_aux = aux_pin;
  this->_size = size;
  this->_arr = (int*) malloc(_size * sizeof(int));
  this->_idx = 0;
}

void Thermistor::setup(uint16_t B, unsigned int R0, int T0, int resistor){
  setup(B, R0, T0, resistor, false);
}

void Thermistor::setup(uint16_t B, unsigned int R0, int T0, int resistor, bool fill){
  this->_B = B;
  this->_R0 = R0;
  this->_T0 = T0;
  this->_resistor = resistor;
  //this->_Rc = (double)_resistor * exp(-B / (T0+273.15));
  if(fill){
    for (int i = 0; i< _size; i++){
      add();
      delay(250);
    }
  }
}

void Thermistor::add(){
  double result;
  double reading = 0;
  
  reading = analogRead(_pin);
  if (_pin_aux != 0){
    reading = reading/2 + analogRead(_pin_aux)/2;
    //reading /= 2;
  }
  reading = 1023.0 / reading - 1;
  reading = _resistor * reading; // convert voltage to resistance
  
  //result = _calculate(reading);
  result = reading / (double)(_R0);        // (R/Ro)
  result /= 1000;
  //result = 0.1;
  result = log(result);           // ln(R/Ro)
  result /= _B;                   // 1/B * ln(R/Ro)
  result += 1.0 / (_T0 + 273.15); // + (1/To)
  result = 1.0 / result;          // Invert
  result -= 273.15;               // convert to C

  if (result > -70 && result < 200){
    result *= 100; //multiply
    _arr[_idx] = (int)result; //cast to int
    _ewma = 0.1 * result + 0.9 * _ewma;
    //Serial.print("THERMISTOR: "); Serial.print(_arr[_idx]);
    _ewma = 0.1 * _arr[_idx] + 0.9 * _ewma;
    //Serial.print(" ewma: "); Serial.println(_ewma);
    _idx++;
  }
  if (_idx == _size) _idx = 0;
}

int Thermistor::get(){
  long int sum = 0;
  for (int i=0; i<_size; i++){
    sum += _arr[i];
  }
  return sum / _size;
}

int Thermistor::getEWMA(){
  return (int)_ewma;
}

double Thermistor::_calculate(double voltage){
  double result;
  result = voltage / (double)(_R0);        // (R/Ro)
  result /= 1000;
  //result = 0.1;
  result = log(result);           // ln(R/Ro)
  result /= _B;                   // 1/B * ln(R/Ro)
  result += 1.0 / (_T0 + 273.15); // + (1/To)
  result = 1.0 / result;          // Invert
  result -= 273.15;               // convert to C
  return result;
}