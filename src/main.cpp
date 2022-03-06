#include "drmaster.h"

#include <EEPROM.h>

#include <LiquidCrystal.h>
#include <Keypad.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <TimerOne.h>

#define MAX_CHARS 6
#define MAX_DECIMALS 1

#define DISPLAY_CHRONO  TIMERS_NUM
#define DISPLAY_TEMP  -1

LiquidCrystal lcd(LCD_PIN_RS, LCD_PIN_EN, LCD_PIN_D4, LCD_PIN_D5, LCD_PIN_D6, LCD_PIN_D7);

Keypad keypad = Keypad( makeKeymap(KEYS), KP_ROWPINS, KP_COLPINS, KP_ROWS, KP_COLS);
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// device address
DeviceAddress insideThermometer;

/* Global Vars */
Tmrstruct timers[TIMERS_NUM];
uint16_t chrono_interval;
uint16_t chrono_count;
uint16_t beep_countdown;
bool do_beep = false;
uint16_t chrono_value;
float temperature;  // Wanted temp
float current_temperature;
int8_t timer_running; // Timer running
int8_t timer_displaying;
bool timer_editing;
bool temperature_editing;
bool interval_editing;
bool refreshView;
bool thermoPresent = false;
bool tempRead = false;
bool warming = false;

unsigned long last_refreshed = 0;
unsigned long last_requested_temp = 0;
unsigned long beeper_up = 0;
bool beep = false;

struct InputStruct {
  char inputChars[MAX_CHARS + 1];
  uint8_t currentChar;
  bool has_point;
  uint8_t decimals;
} inputStruct;


void setup_thermo()
{
  sensors.begin();
  //Serial.print("GOT Devices: ");
  //Serial.println(sensors.getDeviceCount());
  if(sensors.getDeviceCount()) {
    if (sensors.getAddress(insideThermometer, 0)) {
      sensors.setResolution(insideThermometer, 12);
      sensors.setWaitForConversion(false);
      thermoPresent = true;
    }
  }
}

/* Initialize variables  by reading values from eeprom */
void initialize_values() 
{
  uint16_t address = EEPROM_ADDR;
  char input_name[8];
  // Retrieve timers values
  for (int i=0; i<TIMERS_NUM; i++) {
    sprintf(input_name, "Timer %d", i);
    timers[i].description = String(input_name);
    timers[i].value = EEPROM.get(address, timers[i].value);
    timers[i].remaining = timers[i].value;
    address += sizeof(timers[i].value);
  }
  // Retrieve chonometer acoustic signal interval value
  chrono_interval = EEPROM.get(address, chrono_interval);
  if (chrono_interval != 0 && (chrono_interval < CHRONO_INTERVAL_MIN || chrono_interval > CHRONO_INTERVAL_MAX)) {
    chrono_interval = CHRONO_INTERVAL_DEFAULT;
    EEPROM.put(address, chrono_interval);
  }
  chrono_count = 0;
  beep_countdown = chrono_interval;
  address += sizeof(chrono_interval);
  // Retrieve temperature
  temperature = EEPROM.get(address, temperature);
  if (isnan(temperature) || temperature < TEMP_MIN || temperature > TEMP_MAX) {
    temperature = TEMP_DEFAULT;
    EEPROM.put(address, temperature);
  }
}

/* Write remaining time */
void display_remaining(const uint8_t timer_number)
{
  lcd.setCursor(0,1);
  lcd.print("     ");
  lcd.setCursor(0,1);
  lcd.print(String(timers[timer_number].remaining / 10.0, 1));
}

/* Outputs given timer number */
void display_timer(const uint8_t timer_number)
{
  lcd.clear();
  lcd.noCursor();
  lcd.noBlink();
  lcd.print(timers[timer_number].description);
  if (timer_editing)
    lcd.print(" EDIT");
  else if (timer_running == timer_number)
    lcd.print(" RUNNING");
  lcd.setCursor(0,1);
  if (!timer_editing) {
    display_remaining(timer_number);
    lcd.setCursor(7,1);
    lcd.print("/ " + String(timers[timer_number].value / 10.0, 1) + " S");
  }
  else {
    lcd.cursor();
    lcd.blink();
  }
}

void display_chrono_value() {
  lcd.setCursor(0,1);
  lcd.print("           ");
  lcd.setCursor(0,1);
  lcd.print(String(chrono_value / 10.0, 1));
}

void display_crono() {
  lcd.clear();
  lcd.noCursor();
  lcd.noBlink();
  lcd.print("Chrono");
  if (!interval_editing && timer_running != TIMERS_NUM) {
    lcd.print("(");
    lcd.print(String(chrono_interval / 10.0, 1));
    lcd.print(")");
  }
  if (interval_editing)
    lcd.print(" Int. EDIT");
  else if (timer_running == TIMERS_NUM)
    lcd.print(" RUNNING");
  lcd.setCursor(0,1);
  if (!interval_editing)
    display_chrono_value();
  else {
    lcd.cursor();
    lcd.blink();
  }
}

/* Outputs temperature */
void display_temperature() {
  lcd.clear();
  lcd.noCursor();
  lcd.noBlink();
  lcd.print("TemperC ");
  if (temperature_editing)
    lcd.print (" EDIT");
  lcd.setCursor(0,1);
  if (temperature_editing) {
    lcd.cursor();
    lcd.blink();
  } else {
    lcd.print(String(current_temperature, 1)+ " / " + String(temperature, 1));
  }
  if (warming && !temperature_editing)
    lcd.print(" W");
}

void tickTimer()
{
  if (timer_running == -1)
    return;
  if (timers[timer_running].remaining) {
    timers[timer_running].remaining--;
    refreshView = true;
    return;
  }
  noInterrupts();
  Timer1.stop();
  digitalWrite(ENLARGER_OUT, OFF);
  interrupts();
  timer_running = -1;
  refreshView = true;
  do_beep = true;
}

void tickChrono()
{
  if (timer_running == -1)
    return;
  // chrono_count++;
  chrono_value++;
  if (chrono_interval > 0) {
    beep_countdown--;
    if (beep_countdown == 0) {
      do_beep = true;
      beep_countdown = chrono_interval;
    }
  }
  refreshView = true;
}

void setup() 
{
  // TODO: Remove serial routines
  //Serial.begin(9600);
  //Serial.println("Portanna");
  initialize_values();
  // setup LCD
  lcd.begin(LCD_COLS, LCD_ROWS);
  // Setup outputs
  pinMode(ENLARGER_OUT, OUTPUT);
  pinMode(WARMER_OUT, OUTPUT);
  pinMode(BUZZER_OUT, OUTPUT);
  digitalWrite(BUZZER_OUT, LOW);
  digitalWrite(ENLARGER_OUT, OFF);
  digitalWrite(WARMER_OUT, OFF);
  setup_thermo();
  
  timer_running = -1;
  current_temperature = 0;  
  temperature_editing = false;
  timer_editing = false;
  interval_editing = false;
  timer_displaying = 0;
  display_timer(0);
  
  Timer1.initialize(100000);
}


void loop() 
{
  uint8_t val;
  char key = keypad.getKey();
  
  switch (key)
  {
    case '*':
      if (timer_editing || temperature_editing || interval_editing)
        break;
      // Start/Stop Timer or chrono
      if (timer_running != -1) {
        // Stop timer
        noInterrupts();
        Timer1.stop();
        Timer1.detachInterrupt();
        if (timer_running != TIMERS_NUM)
          digitalWrite(ENLARGER_OUT, OFF);
        interrupts();
        if (timer_running != TIMERS_NUM) {
          timer_displaying = timer_running;
          timer_running = -1;
          display_timer(timer_displaying);
        } else {
          timer_displaying = DISPLAY_CHRONO;
          timer_running = -1;
          display_crono();
        }
      } else if (timer_displaying != DISPLAY_TEMP && timer_displaying != DISPLAY_CHRONO && timers[timer_displaying].remaining > 0) {
        // Start timer
        noInterrupts();
        timer_running = timer_displaying;
        Timer1.attachInterrupt(tickTimer);
        if (timers[timer_running].remaining)
          digitalWrite(ENLARGER_OUT, ON);
        Timer1.start();
        interrupts();
        display_timer(timer_displaying);
      } else if (timer_displaying == DISPLAY_CHRONO) {
        timer_running = TIMERS_NUM;
        Timer1.attachInterrupt(tickChrono);
        Timer1.start();
        display_crono();
      }
      break;
    
    case '#':
      if (timer_editing || temperature_editing || interval_editing) {
        if ( inputStruct.currentChar<MAX_CHARS && !inputStruct.has_point) {
          // Insert period char
          inputStruct.inputChars[inputStruct.currentChar++] = '.';
          inputStruct.has_point = true;
          lcd.print('.');
        }
      } else if (timer_running == -1 && timer_displaying == DISPLAY_CHRONO) {
        // Reset chrono
        chrono_value = 0;
        chrono_count = 0;
        beep_countdown = chrono_interval;
        display_crono();
      } else if (timer_running == -1 && timer_displaying != DISPLAY_TEMP) {
        // Reset timer
        timers[timer_displaying].remaining = timers[timer_displaying].value;
        display_timer(timer_displaying);
      }
      break;
    
    case 'A':
      if (timer_running != timer_displaying && !timer_editing && timer_displaying != DISPLAY_CHRONO && timer_displaying != DISPLAY_TEMP) {
        // EDIT Timer
        timer_editing = true;
        memset(inputStruct.inputChars, 0, MAX_CHARS);
        inputStruct.currentChar = 0;
        inputStruct.has_point = false;
        inputStruct.decimals = 0;
        display_timer(timer_displaying);
      } else if (!interval_editing && timer_running != DISPLAY_CHRONO && timer_displaying == DISPLAY_CHRONO) {
        // EDIT Chrono
        interval_editing = true;
        memset(inputStruct.inputChars, 0, MAX_CHARS);
        inputStruct.currentChar = 0;
        inputStruct.has_point = false;
        inputStruct.decimals = 0;
        display_crono();
      } else if (!temperature_editing && timer_displaying == DISPLAY_TEMP) {
        // EDIT Temperature
        temperature_editing =true;
        memset(inputStruct.inputChars, 0, MAX_CHARS);
        inputStruct.currentChar = 0;
        inputStruct.has_point = false;
        inputStruct.decimals = 0;
        display_temperature();
      } else if (timer_editing) {
        // Edit cancel timer
        timer_editing = false;
        display_timer(timer_displaying);
      } else if (temperature_editing) {
        // Edit cancel temperature
        temperature_editing = false;
        display_temperature();
      } else if (interval_editing) {
        // Edit cancel chrono
        interval_editing = false;
        display_crono();
      }
      break;
    
    case 'B':
      if (timer_editing || temperature_editing || interval_editing) {
        int i=0;
        while (i<inputStruct.currentChar && inputStruct.inputChars[i] != '.')
          i++;
        for (;i<inputStruct.currentChar && i<MAX_CHARS; i++)
          inputStruct.inputChars[i] = inputStruct.inputChars[i+1];
        uint16_t nval = atoi(inputStruct.inputChars);
        if (timer_editing || interval_editing) {
          if (!inputStruct.has_point)
            nval *= 10 * MAX_DECIMALS;
          uint16_t address = EEPROM_ADDR;
          if (timer_editing) {
            address += timer_displaying * sizeof(timers[timer_displaying].value);
            EEPROM.put(address, (uint16_t)nval);
            timers[timer_displaying].value = (uint16_t)nval;
            timers[timer_displaying].remaining = timers[timer_displaying].value;
            timer_editing = false;
            display_timer(timer_displaying);
          } else if (interval_editing) {
            if (nval == 0 || (nval >= CHRONO_INTERVAL_MIN * 10 && nval <= CHRONO_INTERVAL_MAX * 10)) {
              address += TIMERS_NUM * sizeof(uint16_t);
              chrono_interval = (uint16_t)nval;
              EEPROM.put(address, (uint16_t)nval);
            }
            chrono_value = 0;
            interval_editing = false;
            display_crono();
          }
        } else if (temperature_editing) {
          float new_temp = (float)nval;
          if (inputStruct.has_point)
            new_temp /= 10.0 * inputStruct.decimals;
          if (!isnan(new_temp) && new_temp >= TEMP_MIN && new_temp <= TEMP_MAX) {
            temperature = new_temp;
            uint16_t address = EEPROM_ADDR;
            address += TIMERS_NUM * sizeof(uint16_t);
            address += sizeof(uint16_t); // Interval
            EEPROM.put(address, temperature);
          }
          temperature_editing = false;
          display_temperature();
        } 
        break;
      }  
    
    case 'C':
      if (timer_editing || temperature_editing || interval_editing)
        break;
      timer_displaying = DISPLAY_TEMP;
      display_temperature();
      break;
    
    case 'D':
      if (timer_editing || temperature_editing || interval_editing)
        break;
      timer_displaying = DISPLAY_CHRONO;
      display_crono();
      break;
    
    default:
      if (key < '0' || key > '9')
        break;
      if ((timer_editing || temperature_editing || interval_editing) && inputStruct.currentChar < MAX_CHARS && inputStruct.decimals < MAX_DECIMALS) {
        if (inputStruct.has_point)
          inputStruct.decimals++;
        inputStruct.inputChars[inputStruct.currentChar++] = key;
        lcd.print(key);
      } else if (!timer_editing && !temperature_editing && !interval_editing) {
        val = key - '0';
        if (val < 0 || val >= TIMERS_NUM)
          break;
        timer_displaying = val;
        display_timer(timer_displaying);
      }
      break;
  }

  unsigned long current_time = millis();
  if (current_time - last_refreshed > 50 && refreshView) {
    last_refreshed = current_time;
    refreshView = false;
    if (timer_running == timer_displaying && timer_displaying != DISPLAY_CHRONO && timer_displaying != DISPLAY_TEMP)
      display_remaining(timer_displaying);
    else if (timer_running == TIMERS_NUM && timer_displaying == DISPLAY_CHRONO)
      display_chrono_value();
    else if (timer_displaying == DISPLAY_TEMP && !temperature_editing && !timer_editing && !interval_editing)
      display_temperature();
    else if (timer_running == -1 && timer_displaying != DISPLAY_CHRONO && timer_displaying != DISPLAY_TEMP && !temperature_editing && !timer_editing && !interval_editing) {
      display_timer(timer_displaying);
    }
  }

  if (thermoPresent) {
    if (current_time - last_requested_temp > TEMP_RESPONSE_TIME_MAX && current_time - last_requested_temp < TEMP_REQUEST_INTERVAL && tempRead) {
      // Temperature ready
      float tempC = sensors.getTempC(insideThermometer);
      if(tempC != DEVICE_DISCONNECTED_C) {
        current_temperature = tempC;
        refreshView = true;
        if (current_temperature <= temperature - TEMP_DELTA_INF) {
          warming = true;
          digitalWrite(WARMER_OUT, ON);
        }
        else if (current_temperature >= temperature + TEMP_DELTA_SUP) {
          warming = false;
          digitalWrite(WARMER_OUT, OFF);
        }
      }
      tempRead = false;
    } else if (current_time - last_requested_temp > TEMP_REQUEST_INTERVAL) {
      sensors.requestTemperaturesByAddress(insideThermometer);
      last_requested_temp = millis();
      tempRead = true;
    }
  }
  
  if (do_beep && !beep) {
    do_beep = false;
    beep = true;
    digitalWrite(BUZZER_OUT, HIGH);
    beeper_up = millis();
  } 

  current_time = millis();
  if (beep && current_time - beeper_up >= BEEP_DURATION_MS) {
    beep = false;
    digitalWrite(BUZZER_OUT, LOW);
  }
}