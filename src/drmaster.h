#include <Arduino.h>

/* LCD Pins */
#define LCD_PIN_RS 0
#define LCD_PIN_EN 1
#define LCD_PIN_D4 2
#define LCD_PIN_D5 3
#define LCD_PIN_D6 4
#define LCD_PIN_D7 5
#define LCD_COLS 16
#define LCD_ROWS 2

/* Output pins */
#define ENLARGER_OUT A4 
#define WARMER_OUT A5
#define BUZZER_OUT A1

#define OFF HIGH
#define ON  LOW

#define ONE_WIRE_BUS A3

#define BEEP_DURATION_MS    300

/* EEPROM initial address for preferences */
#define EEPROM_ADDR 0

/* Number of timers */
#define TIMERS_NUM 10

#define TEMP_DEFAULT 20.0
#define TEMP_MIN 10.0
#define TEMP_MAX 60.0

#define TEMP_DELTA_INF 0.2
#define TEMP_DELTA_SUP 0.2

#define TEMP_REQUEST_INTERVAL   5000
#define TEMP_RESPONSE_TIME_MAX  1000

#define CHRONO_INTERVAL_MIN    1
#define CHRONO_INTERVAL_MAX    600
#define CHRONO_INTERVAL_DEFAULT    60

/* Keyboard configuration */
const byte KP_ROWS = 4; //four rows
const byte KP_COLS = 4; //three columns
const char KEYS[KP_ROWS][KP_COLS] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};
byte KP_ROWPINS[KP_ROWS] = {6, 7, 8, 9}; // row pins of the keypad
byte KP_COLPINS[KP_COLS] = {13, 12, 11, 10}; // column pins of the keypad

/* Timer */
typedef struct Tmrstruct
{
    String description; // Name
    uint16_t value; 
    uint16_t remaining;
} Tmrstruct;
