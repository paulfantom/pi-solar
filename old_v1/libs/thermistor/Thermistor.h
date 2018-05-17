#include <Arduino.h>

class Thermistor {
  public:
    Thermistor(int8_t pin);
    Thermistor(int8_t pin, uint8_t size);
    Thermistor(int8_t pin, int8_t aux_pin, uint8_t size);
    
    void setup(uint16_t B, unsigned int R0, int T0, int resistor);
    void setup(uint16_t B, unsigned int R0, int T0, int resistor, bool fill);
    int get();
    void add();
    int addGet() {add(); return get();};
    void changeB(uint16_t B){_B = B;};
    void changeR(unsigned int R){_R0 = R;};
    void changeT(int T){_T0 = T;};
    void change(uint16_t B, unsigned int R, int T){_B = B; _R0 = R; _T0 = T;};
    int getEWMA();
    
  private:
    void _init(int8_t pin, int8_t aux_pin, uint8_t size);
    double _calculate(double voltage);
    int8_t _pin;
    int8_t _pin_aux;
    uint16_t _B;
    unsigned int _R0;
    int _T0;
    int _resistor;
    uint8_t _size;
    uint8_t _idx;
    int * _arr;
    double _ewma;
};