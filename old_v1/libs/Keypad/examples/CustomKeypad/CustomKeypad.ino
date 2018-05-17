/* @file CustomKeypad.pde
|| @version 1.0
|| @author Alexander Brevig
|| @contact alexanderbrevig@gmail.com
||
|| @description
|| | Demonstrates changing the keypad size and key values.
|| #
*/
#include <Keypad.h>

byte ROW_PINS[5] = {39, 37, 35, 33, 31}; //connect to the row pinouts of the keypad
byte COL_PINS[4] = {23, 25, 27, 29}; //connect to the column pinouts of the keypad

// Keypad keys
char keys[5][4] = {
  {'F','G','#','*'}, // F - F1, G - F2
  {'1','2','3','u'}, // u - up
  {'4','5','6','d'}, // d - down
  {'7','8','9','Q'}, // l - left, Q - Esc
  {'l','0','r','E'}  // r - right, E - Ent
};
Keypad customKeypad = Keypad( makeKeymap(keys), ROW_PINS, COL_PINS, 5, 4);


void setup(){
  Serial.begin(9600);
}
  
void loop(){
  char customKey = customKeypad.getKey();
  
  if (customKey){
    Serial.println(customKey);
  }
}
