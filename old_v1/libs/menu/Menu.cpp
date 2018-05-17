#include "Menu.h"

Menu::Menu(Keypad &keypad_object, LiquidCrystal_I2C &lcd_display, uint8_t rows, uint8_t cols) {
  this->keypad = &keypad_object;
  this->lcd = &lcd_display;
  this->_rows = rows;
  this->_cols = cols;
}

void Menu::setup(int16_t* data_array,uint8_t array_size){
  setup(data_array, array_size, 600, 0);
}

void Menu::setup(int16_t* data_array,uint8_t array_size, uint16_t wait_time, uint32_t time_now){
  _data = data_array;
  _data_size = array_size;
  _begin_time = time_now;
  _wait_time = wait_time;
  _menu_value_changed = false;
  _manual_mode = 0;
  _manual_mode_time = wait_time;
};

bool Menu::loop(){
  loop(true);
}

bool Menu::loop(bool dim) {
  uint8_t addr;
  char key = keypad->getKey();
  Serial.print(key);
  if (_current_menu < 0){
    switch (key){
    case 'E': _current_menu = 0; _last_menu = -1; break;
    case 'F': _current_menu = 5; _last_menu = -1; break;
    }
  } else {
    if (_current_menu != _last_menu){
      _last_menu = _current_menu;
      _submenu_has_values = renderMenu();
      _selected_row = 0;
    }
    if ((_current_menu == 1) || ((_current_menu > 4) && (_current_menu < 8))){
      if (_current_menu == 1) _current_menu = 5;
      renderValues();
      switch(key){
      case 'r': case 'd': case 'E':
        //TODO make it cleaner;
        _current_menu -= 5;
        ++_current_menu %= 3;
        _current_menu += 5;
        break;
      case 'l': case 'u':
        switch(_current_menu){
          case 5: _current_menu = 7; break;
          case 6: _current_menu = 5; break;
          case 7: _current_menu = 6; break;
        }
        //_current_menu -= 5;
        //--_current_menu %= 3;
        //_current_menu += 5;
        break;
      case 'Q':
        _current_menu = 0;
        break;
      }
    } else {
      //_selected_row = menuSelector(key, _selected_row);
      if (_submenu_has_values) {
        addr = renderValues(); //FIXME possible flickering
      }      
      switch(key){
      case 'u':
        lcd->setCursor(0,_selected_row); lcd->print(' ');
        --_selected_row %= _rows;
        lcd->setCursor(0,_selected_row); lcd->print('>');
        break;
      case 'd':
        lcd->setCursor(0,_selected_row); lcd->print(' ');
        ++_selected_row %= _rows;
        lcd->setCursor(0,_selected_row); lcd->print('>');
        break;
      case 'E':
        if (_submenu_has_values){
          _menu_value_changed = changeValue(_selected_row, addr); //FIXME blocking function
        } else {
          _current_menu *= 10;
          _current_menu += _selected_row;
          _current_menu++;
        }
        break;
      case 'Q':
        if (_current_menu == 0) {
          _current_menu = -1;
        } else {
          _current_menu /= 10;
        }
        if (_menu_value_changed) {
          //apply changes
          _menu_value_changed = false;
        }
        break;
      }
    }
    if (now() - _begin_time > _wait_time) _current_menu = -1;
  }
  switch (key) {
  case 'd': case 'u': case 'l': case 'r': case 'E':
    _begin_time = now();
    break;
  case 'G':
    _current_menu = -1;
    break;
  }

  if (_current_menu == -2){ // first run
    if (now() - _begin_time > _wait_time) _current_menu = -1; 
  }
  
  if ((_current_menu == -1) && (_last_menu != -1)){
    _current_menu = -1;
    _last_menu = -1;
    if(dim){
      lcd->setBacklight(LOW);
      lcd->noDisplay();
    }
    return false;
  }
  return true;
}

void Menu::resetManual(){
  _manual_mode = 0;
  _manual_mode_time = _wait_time;
}

uint8_t Menu::getManualState(){
  return _manual_mode;
}

unsigned int Menu::getManualTime(){
  return _manual_mode_time;
}

void Menu::decrementManual(){
  if (_manual_mode_time != 0){
    _manual_mode_time--;
  }
}

bool Menu::newSettings() {
  if (_changed_values){
    _changed_values = false;
    return true;
  } else {
    return false;
  }
}

void Menu::printScreen(const char screenArray[][19]){
  printScreen(screenArray, 2);
}

void Menu::printScreen(const char screenArray[][19], int8_t col){
  char sbuff[19];
  lcd->clear();
  lcd->noCursor();
  lcd->noBlink();
  if (col < 0) col = 2;
  for(int i=0; i<_rows; i++){
    strcpy_P(sbuff, &screenArray[i][0]);
    lcd->setCursor(0,i);
    lcd->print("  ");
    lcd->setCursor(col,i);
    lcd->print(sbuff);
  }
  if (col != 0) {
    lcd->home();
    lcd->print('>');
  }
}

bool Menu::changeValue(int8_t row, uint8_t eeprom_addr){
  return changeValue(row, _current_menu, eeprom_addr);
}

bool Menu::changeValue(int8_t row, int flag, uint8_t eeprom_addr){
  int16_t val;
  bool ret = false;
  Serial.print("CHANGING VAL, address: "); Serial.println(eeprom_addr);
  _changed_values = true;
  switch(flag){
    case 343:
      // if(row==0) fall-through to case 341
      if(row == 1){
        eeprom_addr += 4;
        EEPROM.get(eeprom_addr, val);
        if (val != EEPROM.put(eeprom_addr, setValue(val, 5, row, 15))){
          ret = true;
        }
        break; 
      } else if(row > 1) {
        return false;
      } //else go to next switch-case (341)
    case 341: case 321: case 322: case 323: case 324:// ip + time (fall-through)
      eeprom_addr += row*4;
      lcd->setCursor(5 ,row); lcd->print(F("   .   .   .   "));
      uint8_t octet;
      for(int i=0; i<4; i++){
        EEPROM.get(eeprom_addr+i,octet);
        if( octet != EEPROM.put(eeprom_addr+i, (uint8_t)setValue(octet, 3, row, 5+i*4))){
          ret = true;
        }
      }
      break;
    case 342: //mqtt
      switch (row){
        case 0:
          lcd->setCursor(5 ,row); lcd->print(F("   .   .   .   "));
          uint8_t octet;
          for(int i=0; i<4; i++){
            EEPROM.get(eeprom_addr+i,octet);
            if( octet != EEPROM.put(eeprom_addr+i, (uint8_t)setValue(octet, 3, row, 5+i*4))){
              ret = true;
            }
          }
          break;
        case 1:
          eeprom_addr += 4;
          EEPROM.get(eeprom_addr, val);
          if (val != EEPROM.put(eeprom_addr, setValue(val, 5, row, 15))){
            ret = true;
          }
          break;
        case 2: case 3: //set user,pass
          eeprom_addr += (6 + (row-2)*14); // add 6 fields (ip 4, port 2). user & pass has 14 fields
          char text[14]; //TODO maybe enable [A-z] not only [0-9]
          EEPROM.get(eeprom_addr, text);
          char new_text[14];
          if (! setValue(new_text,14,row,6)){
            break;
          }
          if (strcmp(text,new_text)){
            EEPROM.put(eeprom_addr,new_text);
            ret = true;
          }
          break;
      }
    case 41:
      switch(row){
        case 0: //valve PWM
          _data[_data_size-1] = setValue(_data[_data_size-1],5,row,15);
          if(_data[_data_size-1] > 100) _data[_data_size-1] = 100;
          if(_data[_data_size-1] < 0) _data[_data_size-1] = 0;
          _manual_mode |= 1 << 5;
          break;
        case 1: // valve binary
          _data[_data_size-2] ^= 1 << 0;
          _manual_mode |= 1 << 0;
          break;
        default: // pump
          _data[_data_size-2] ^= 1 << 1;
          _manual_mode |= 1 << 1;
          break;
      }
      ret = true;
      break;
    case 42:
      switch(row){
        case 0: // valve
          _data[_data_size-2] ^= 1 << 2;
          _manual_mode |= 1 << 2;
          break;
        case 1: // heater
          _data[_data_size-2] ^= 1 << 3;
          _manual_mode |= 1 << 3;
          break;
        default: // circulation
          _data[_data_size-2] ^= 1 << 4;
          _manual_mode |= 1 << 4;
          break;
      }
      ret = true;
      break;
    case 43:
      if (row == 1){
        resetManual();
      } else {
        _manual_mode_time = setValue(_manual_mode_time, 5, row, 15);
      }
      ret = true;
      break;
    default:
      eeprom_addr += row*2;
      EEPROM.get(eeprom_addr, val);
      if (val != EEPROM.put(eeprom_addr, setValue(val, 5, row, 15))){
        ret = true;
      }
      break;
  }
  return ret;
}

boolean Menu::setValue(char* buf, int len, uint8_t row){
  return setValue(*buf, len, row, -1);
}

boolean Menu::setValue(char* buf, int len, uint8_t row, uint8_t col){
  //TODO
  if((col>20)||(col<0)) col=20-len;
  char key = '-';
  uint8_t idx = 0;
  char letter = ' ';
  unsigned long prev = millis();
  lcd->setCursor(col,row);
  lcd->cursor();
  //memset(buf,' ',len);
  
  //clear
  for (int i=0; i<len; i++){
    lcd->print(" ");
  }
  lcd->setCursor(col,row);
  
  while (millis() - prev < _wait_time*250){ // (_wait_time * 1000 /4) (ms to s -> div by 4)
    key = keypad->getKey();
    switch(key){
    case '0': case '1': case '2': case '3': case '4': 
    case '5': case '6': case '7': case '8': case '9':
      buf[idx] = (char)key;
      lcd->print((char)buf[idx]);
      ++idx%=len;
    /*case '*': case '#':
      if (key == 35) buf[idx] = '#';
      if (key == 42) buf[idx] = '*';
      lcd->print((char)buf[idx]);
      ++idx%=len;*/
      break;
    case 'u':
      letter++;
      if(letter>122) letter = 32; // only [A-z] and [0-9]
      if(letter==33) letter = 48; // skip some ASCII
      if(letter==58) letter = 65; // skip some ASCII
      if(letter==91) letter = 97; // skip some ASCII
      lcd->print((char)letter);
      lcd->setCursor(col+idx,row);
      break;
    case 'd':
      letter--;
      if(letter<32) letter = 122;
      if(letter==47) letter = 32; // skip some ASCII
      if(letter==64) letter = 57; // skip some ASCII
      if(letter==96) letter = 90; // skip some ASCII
      lcd->print((char)letter);
      lcd->setCursor(col+idx,row);
      break;
    case 'l':
      --idx%=len;
      lcd->setCursor(col+idx,row);
      break;
    case 'r':
      buf[idx] = (char)letter;
      buf[idx+1] = '\0';
      ++idx%=len;
      lcd->setCursor(col+idx,row);
      break;
    case 'Q':
      lcd->noCursor();
      return false;
    case 'E':
      buf[idx] = (char)letter;
      //buf[idx+1] = '\0';
      idx++;
      lcd->noCursor();
      // fill spaces
      for(idx; idx<len; idx++){
        buf[idx] = ' ';
      }
      buf[len] = '\0';
      // print to display
      lcd->setCursor(col,row);
      for(int i=0; i<len; i++){
        lcd->print((char)buf[i]);
      }
      return true;
    }
    delay(30);
    if(key){ 
      prev = millis();
    }
    if(idx>=len){
      lcd->noCursor();
      return true;
    }
  }
  // time is up
  lcd->noCursor();
  return false;
}

int16_t Menu::setValue(int val, uint8_t len, uint8_t row){
  return setValue(val, len, row, -1);
}

int16_t Menu::setValue(int val, uint8_t len, uint8_t row, int col){
//uint16_t Menu::setValue(uint16_t val, uint8_t row, uint8_t col, int len){
  if(len > 5) len=5; //uint size is from 0 to 65535, hence max len = 5
  if( col>20 || col<0 ) col=20-len;
  char key = '-';
  unsigned long prev = millis();
  uint8_t pos = 0;
  char buffer[len+1];
  bool cleared = false;
  
  // change selector '>' to '#'
  //lcd->setCursor(0,row);
  //lcd->print("#");
  // start changes
  lcd->setCursor(col,row);
  lcd->blink();
  // clear buffer
  for (int i=0; i<len; i++){
    buffer[i] = '\0';
  }
  
  // clear
  for (int i=0; i<len; i++){
    lcd->print(" ");
  }
  lcd->setCursor(col,row);
  
  while (millis() - prev < _wait_time*250){ // (_wait_time * 1000 /4) (ms to s -> div by 4)
    key = keypad->getKey();
    switch(key){
    case '0': case '1': case '2': case '3': case '4': 
    case '5': case '6': case '7': case '8': case '9':
    case '.': case '-':
      if (pos < len){
        buffer[pos] = key;
        lcd->print(key);
        pos++;
      } else {
        pos = 0;
      }
      break;
    case 'Q':
      lcd->noBlink();
      lcd->setCursor(col,row);
      lcd->print(val);
      return val;
    case 'E': case 'r':
      buffer[pos+1] = '\0';
      lcd->noBlink();
      /*float new = atof(buffer);
      new*=100;
      return (int)new;*/
      return atoi(buffer);
    }
    delay(30);
  }
  lcd->noBlink();
  return val;
}

void Menu::renderVal(int val, uint8_t row, bool to_float){
  float new_val;
  new_val = (float)val / 100;
  lcd->setCursor(14,row);
  lcd->print("      ");
  
  lcd->setCursor(14,row);
  if (new_val < 0){
    //Failsafe
    if (new_val <= -100) {
      lcd->setCursor(13,row);
    }
    if ((new_val <= -10) && (new_val > -100)){
      lcd->setCursor(14,row);
    }
    if (new_val > -10) {
      lcd->setCursor(15,row);
    }
  } else {
    //Failsafe
    if (new_val >= 1000) {
      lcd->setCursor(13,row);
    }
    if ((new_val >= 10) && (new_val < 100)){
      lcd->setCursor(15,row);
    }
    if (new_val < 10) {
      lcd->setCursor(16,row);
    }
  }
  lcd->print(new_val);
};

void Menu::renderVal(bool val, uint8_t row){
  renderVal(val, row, 6);
}

void Menu::renderVal(bool val, uint8_t row, uint8_t pre_clear){
  lcd->setCursor(20-pre_clear,row);
  for(int i=0; i<pre_clear;i++){
    lcd->print(' ');
  }
  if (val){
    lcd->setCursor(18,row);
    lcd->print("ON");
  } else {
    lcd->setCursor(17,row);
    lcd->print("OFF");
  }
}

void Menu::renderVal(int val, uint8_t row){
  uint8_t col = 15;
  for(int i=4; i>0; i--){
    if ( val < pow(10,i)){
      col++;
    } else {
      break;
    }
  }
  lcd->setCursor(14,row);
  lcd->print("      ");
  lcd->setCursor(col,row);
  lcd->print(val);
};

void Menu::renderVal(uint8_t arr[4], uint8_t row){
  lcd->setCursor(5,row);
  for(int i=0; i<4; i++){
    if(arr[i] < 100) lcd->print(' ');
    if(arr[i] < 10)  lcd->print(' ');
    lcd->print(arr[i]);
    if(i<3) lcd->print('.'); 
  }
};

void Menu::renderVal(char arr[14], uint8_t row){
  lcd->setCursor(6,row);
  for(int i=0; i<14; i++){
    // check if ASCII
    if(32 <= arr[i]< 127){
      lcd->print((char)arr[i]);
    } else {
      lcd->print(' ');
    }
  }
}

void Menu::renderCurrentValues(){
  renderCurrentValues(_current_menu);
}

void Menu::renderCurrentValues(int8_t flag){
  switch (flag){
    //T solar,T wewnatrz,T zewnatrz,T solar (in),T solar (out),T piec (in),T piec (out),T zbiornik (gora), water flow, pwm_duty
  case 5:
    renderVal(_data[_data_size-1],0); //pwm_duty
    renderVal(_data[0],1,true); //T solar
    renderVal(_data[3],2,true); //T solar (in)
    renderVal(_data[4],3,true); //T solar (out)
    break;
  case 6:
    renderVal(_data[5],0,true); //T piec (in)
    renderVal(_data[6],1,true); //T piec (out)
    renderVal(_data[7],2,true); //T zbiornik (gora)
    renderVal((bool)_data[8],3); //water flow
    break;
  case 7:
    renderVal(_data[1],0,true); //T wewnatrz
    renderVal(_data[2],1,true); //T zewnatrz
    //renderVal(_data[6],2,true);
    //renderVal(_data[5],3,true);
    break;
  case 41:
    renderVal(_data[_data_size-1],0);
    renderVal((bool)(_data[_data_size-2] & 1),1);
    renderVal((bool)((_data[_data_size-2] >> 1) & 1),2);
    break;
  case 42:
    renderVal((bool)((_data[_data_size-2] >> 2) & 1),0);
    renderVal((bool)((_data[_data_size-2] >> 3) & 1),1);
    renderVal((bool)((_data[_data_size-2] >> 4) & 1),2);
    break;
  case 43:
    renderVal((int)_manual_mode_time,0);
    if(_manual_mode > 0) renderVal(true,1);
    else renderVal(false,1);
    break;
  }
}

// LOOKUP TABLE???
uint8_t Menu::renderEEPROMValues(){
  return renderEEPROMValues(_current_menu);
}

uint8_t Menu::renderEEPROMValues(int flag){
  uint8_t eeprom_addr;
  uint8_t rows;
  //uint16_t val;
  int val;
  uint8_t tmp[4];
  switch(flag){
    case 21: // solar 
      eeprom_addr = EE_SOL_CRIT;
      rows = 3;
      break;
    case 22: // tank
      eeprom_addr = EE_SOL_TANK;
      rows = 3;
      break;
    case 23: // heater
      eeprom_addr = EE_HEAT_CRIT;
      rows = 4;
      break;
    case 24: // circulation
      eeprom_addr = EE_CIRC_T_ON;
      rows = 2;
      break;
    case 311: //case 312: case 313: // therm
      eeprom_addr = (EE_THRM1_B + (flag - 311)*6);
      rows = 3;
      break;
    /*case 321: case 322: case 323: // mbus
      eeprom_addr = (EE_MBUS1_ADDR + (flag - 321)*4);
      rows = 2;
      break;*/
    case 321: case 322: case 323: case 324: // DS18B20
      eeprom_addr = (EE_DS18B20_1 + (flag - 321)*16);
      rows = 4;
      for(int i=0; i<rows; i++){
        EEPROM.get(eeprom_addr + i*4,tmp);
        renderVal(tmp,i);
      }
      return eeprom_addr;
    case 331: // pwm
      eeprom_addr = EE_PUMP_T1;
      rows = 4;
      break;
    case 332: // pid
      eeprom_addr = EE_PID_P;
      rows = 3;
      break;
    case 341: // ip
      eeprom_addr = EE_ETH_IP;
      rows = 4;
      for(int i=0; i<rows; i++){
        EEPROM.get(eeprom_addr + i*4,tmp);
        renderVal(tmp,i);
      }
      return eeprom_addr;
    case 342: // mqtt
      eeprom_addr = EE_MQTT_IP;
      EEPROM.get(eeprom_addr,tmp);
      renderVal(tmp,0);
      EEPROM.get(eeprom_addr+4,val);
      renderVal(val,1);
      char string[14];
      for(int i=0; i<2; i++){
        EEPROM.get(eeprom_addr+6+i*14,string);
        renderVal(string,i+2);
      }
      return eeprom_addr;
    case 343: // time
      eeprom_addr = EE_TIME_IP;
      EEPROM.get(eeprom_addr,tmp);
      renderVal(tmp,0);
      EEPROM.get(eeprom_addr+4,val);
      renderVal(val,1);
      val = hour()*100 + minute();
      renderVal(val,2);
      return eeprom_addr;
  }

  for(int i=0; i<rows; i++){
    EEPROM.get(eeprom_addr+i*2, val);
    renderVal((int)val, i);
    //lcd->setCursor(15,i); lcd->print(val);
  }
  return eeprom_addr;
}

uint8_t Menu::renderValues(){
  if ((_current_menu>40 && _current_menu<45) || (_current_menu>3 && _current_menu<10)){
    renderCurrentValues();
    return 255;
  } else {  
    return renderEEPROMValues();
  }
}

uint8_t Menu::renderValues(int flag){
  if ((flag>40 && flag<50) || (flag>3 && flag<10)){
    renderCurrentValues(flag);
    return 255;
  } else {  
    return renderEEPROMValues(flag);
  }
}

bool Menu::renderMenu(){
  return renderMenu(_current_menu);
}

bool Menu::renderMenu(int flag){
  _begin_time = now();
  lcd->setBacklight(HIGH);
  lcd->display();
  switch (flag){
    case 0:
      // main menu
      printScreen(MAIN_MENU,-1);
      lcd->setCursor(17,3);
      if(_manual_mode > 0){
        lcd->print(" ON");
      } else {
        lcd->print("OFF");
      }
      return false;
    //-------------------------------------------------
    case 1: case 5: case 6: case 7:
      // sensors
      if (flag == 1) flag = 5;
      printScreen(&SENSORS_MENU[(flag-5)*4],0);
      return false;
    //-------------------------------------------------    
    case 2:
      // setpoints
      printScreen(SETPOINTS_MENU,-1);
      return false;
    case 21:
      // solar
      printScreen(SOLAR_DESC,-1);
      return true;
    case 22:
      // tank
      printScreen(TANK_DESC,-1);
      return true;
    case 23:
      // heater
      printScreen(HEATER_DESC,-1);
      return true;
    case 24:
      // circulation
      printScreen(CIRC_DESC,-1);
      return true;
    //-------------------------------------------------
    case 3:
      // settings
      printScreen(SETTINGS_MENU,-1);
      return false;
    case 31:
      // thermistors
      printScreen(THERMISTORS_SUBMENU,-1);
      return false;
    case 311: //case 312: case 313:
      printScreen(THERMISTOR_DESC,-1);
      return true;
    case 32:
      // mbus or DS18B20
      //printScreen(MBUS_SUBMENU,-1);
      printScreen(DS18B20_SUBMENU,-1);
      return false;
    case 321: case 322: case 323: case 324:
      //printScreen(MBUS_DESC,-1);
      printScreen(DS18B20_DESC,-1);
      return true;
    case 33:
      printScreen(VALVE_SUBMENU,-1);
      return false;
    case 331:
      printScreen(PWM_DESC,-1);
      return true;
    case 332:
      printScreen(PID_DESC,-1);
      return true;
    case 34:
      // other
      printScreen(OTHER_SUBMENU,-1);
      return false;
    case 341:
      // ethernet ip
      printScreen(IP_DESC,-1);
      return true;
    case 342:
      // mqtt
      printScreen(MQTT_DESC,-1);
      return true;
    case 343:
      // time
      printScreen(TIME_DESC,-1);
      return true;
    case 344:
      // detect new DS18B20
      printScreen(DETECT_DESC,-1);
      return false;
    //-------------------------------------------------
    case 4:
      printScreen(MANUAL_MENU,-1);
      return false;
    case 41:
      printScreen(MANUAL_SUBMENU_1,-1);
      return true;
    case 42:
      printScreen(MANUAL_SUBMENU_2,-1);
      return true;
    case 43:
      printScreen(MANUAL_SUBMENU_OPT,-1);
      return true;
    default:
      return false;
  }
}