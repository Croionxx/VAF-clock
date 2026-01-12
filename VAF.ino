#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

// =====================================================
// DEBUG (optional)
// =====================================================
#define DEBUG 1
#if DEBUG
  #define DBG(x) Serial.println(x)
#else
  #define DBG(x)
#endif

// =====================================================
// I2C (DS3231)
// =====================================================
#define I2C_SDA 13
#define I2C_SCL 14
RTC_DS3231 rtc;

// =====================================================
// DISPLAY PINS
// =====================================================
const uint8_t P0 = 16, P1 = 17, P2 = 18, P3 = 19;
const uint8_t SEG_PINS[8] = {21,22,23,25,26,27,32,33};
#define BUTTON_PIN 4   // button to GND (INPUT_PULLUP)

// =====================================================
// FONT (A B C D E F G DP) – common cathode
// =====================================================
const uint8_t FONT[] = {
  // 0–9
  0b00111111, 0b00000110, 0b01011011, 0b01001111,
  0b01100110, 0b01101101, 0b01111101, 0b00000111,
  0b01111111, 0b01101111,
  // h, n, S, d, y
  0b01110110, // h
  0b01010100, // n
  0b01101101, // S
  0b01011110, // d
  0b00111001  // y
};

#define CHAR_H 10
#define CHAR_N 11
#define CHAR_S 12
#define CHAR_D 13
#define CHAR_Y 14

// =====================================================
// MODES
// =====================================================
uint8_t mode = 0;
// 0 = TIME, 1 = DATE, 2 = TIMER

// =====================================================
// TIME / DATE (RTC)
// =====================================================
uint8_t hh, mm, ss;
uint8_t day, month;
uint8_t year;

// =====================================================
// TIMER (count-up)
// =====================================================
uint8_t t_hh = 0, t_mm = 0, t_ss = 0;

// =====================================================
// DISPLAY BUFFER
// =====================================================
uint8_t buf[9];

// =====================================================
// MULTIPLEX CONTROL
// =====================================================
uint8_t curDigit = 0;
uint32_t lastMux = 0;
const uint16_t mux_us = 700;

// =====================================================
// HELPERS
// =====================================================
inline void selectDigit(uint8_t d){
  digitalWrite(P0, d & 1);
  digitalWrite(P1, d & 2);
  digitalWrite(P2, d & 4);
  digitalWrite(P3, d & 8);
}

inline void setSeg(uint8_t p){
  for(int i=0;i<8;i++)
    digitalWrite(SEG_PINS[i], (p >> i) & 1);
}

// =====================================================
// RTC READ
// =====================================================
void readRTC(){
  DateTime now = rtc.now();
  hh = now.hour();
  mm = now.minute();
  ss = now.second();
  day = now.day();
  month = now.month();
  year = now.year() % 100;

#if DEBUG
  Serial.printf("RTC %02d:%02d:%02d  %02d/%02d/%02d\n",
                hh, mm, ss, day, month, year);
#endif
}

// =====================================================
// DISPLAY UPDATE
// =====================================================
void updateDisplay(){

  // -------- TIME --------
  if(mode == 0){
    buf[0] = hh / 10; buf[1] = hh % 10; buf[2] = CHAR_H;
    buf[3] = mm / 10; buf[4] = mm % 10; buf[5] = CHAR_N;
    buf[6] = ss / 10; buf[7] = ss % 10; buf[8] = CHAR_S;
  }

  // -------- DATE --------
  else if(mode == 1){
    buf[0] = day / 10;   buf[1] = day % 10;   buf[2] = CHAR_D;
    buf[3] = month / 10; buf[4] = month % 10; buf[5] = CHAR_N;
    buf[6] = year / 10;  buf[7] = year % 10;  buf[8] = CHAR_Y;
  }

  // -------- TIMER --------
  else{
    buf[0] = t_hh / 10; buf[1] = t_hh % 10; buf[2] = CHAR_H;
    buf[3] = t_mm / 10; buf[4] = t_mm % 10; buf[5] = CHAR_N;
    buf[6] = t_ss / 10; buf[7] = t_ss % 10; buf[8] = CHAR_S;
  }
}

// =====================================================
// BUTTON (cycle mode)
// =====================================================
void handleButton(){
  static bool last = HIGH;
  static uint8_t lastMode = 0;
  bool now = digitalRead(BUTTON_PIN);

  if(last == HIGH && now == LOW){
    lastMode = mode;
    mode = (mode + 1) % 3;

    // ---- entering TIMER mode ----
    if(mode == 2 && lastMode != 2){
      t_hh = t_mm = t_ss = 0;
      DBG("Timer RESET & START");
    }

    // ---- leaving TIMER mode ----
    if(lastMode == 2 && mode != 2){
      t_hh = t_mm = t_ss = 0;
      DBG("Timer STOP & RESET");
    }

    updateDisplay();
    delay(200); // debounce
  }
  last = now;
}


// =====================================================
// SETUP
// =====================================================
void setup(){
  Serial.begin(115200);
  delay(300);
  DBG("BOOT");

  pinMode(P0, OUTPUT); pinMode(P1, OUTPUT);
  pinMode(P2, OUTPUT); pinMode(P3, OUTPUT);
  for(int i=0;i<8;i++) pinMode(SEG_PINS[i], OUTPUT);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  rtc.begin();

  // ---- SET RTC ONCE IF NEEDED ----
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  readRTC();
  updateDisplay();
}

// =====================================================
// LOOP
// =====================================================
void loop(){

  // ---- Multiplex display ----
  if(micros() - lastMux >= mux_us){
    lastMux = micros();
    setSeg(0);
    selectDigit(curDigit);
    setSeg(FONT[buf[curDigit]]);
    curDigit = (curDigit + 1) % 9;
  }

  // ---- 1-second tick ----
  static uint32_t lastSec = 0;
  if(millis() - lastSec >= 1000){
    lastSec += 1000;

    // RTC
    readRTC();

    // Timer count-up ONLY in timer mode
  if(mode == 2){
    t_ss++;
    if(t_ss == 60){ t_ss = 0; t_mm++; }
    if(t_mm == 60){ t_mm = 0; t_hh++; }
    if(t_hh == 100) t_hh = 0;
  }

    updateDisplay();
  }

  handleButton();
}
