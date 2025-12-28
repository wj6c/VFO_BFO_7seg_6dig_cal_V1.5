// Si5351 VFO 40m LSB + 25 MHz CAL + Adjustable BFO
// CLK0=VFO, CLK1=CAL, CLK2=BFO (12 MHz + Offset)
// Hardware: Encoder, MAX7219 6-digit Display, STEP Button
// Platform: LGT8Fx / AVR (Arduino Nano/Uno compatible)

#include <EEPROM.h>
#include "src/Rotary.h"
#include "src/si5351.h"
#include "src/LedControl.h"

// -------------  CONFIGURATION  -------------
#define DEFAULT_RX    7000000L           // Default Startup Frequency
#define BAND_L        7000000L           // Lower Band Limit
#define BAND_H        7300000L           // Upper Band Limit
#define IF_FREQ       12000000L          // Intermediate Frequency
#define SW_STEP       A3                 // STEP Button Pin
#define ENC_INC_1     50                 // Fine Step (Hz)
#define ENC_INC_2     1000               // Coarse Step (Hz)

#define CAL_FREQ      25000000UL         // Calibration Reference Frequency
#define BFO_STEP      100                // BFO Adjust Step (Hz)
#define BFO_RANGE     8000               // BFO Limit (+/- 8kHz)
#define BFO_CENTER    (12000240L)        // BFO Center Point
#define EEPROM_CAL    0                  // EEPROM Address for CAL
#define EEPROM_BFO    8                  // EEPROM Address for BFO
#define MAGIC_CAL     0xCA               // Validation Byte for CAL
#define MAGIC_BFO     0xCB               // Validation Byte for BFO

// -------------  OBJECTS  -------------
Rotary r = Rotary(3, 2);                 // Encoder Pins (PD3, PD2)
Si5351 si5351(0x60);                     // Si5351 I2C Address
LedControl lc = LedControl(10, 12, 11, 1); // DIN, CLK, CS, # of Modules

// -------------  VARIABLES  -------------
uint32_t rx = DEFAULT_RX;
uint32_t enc_inc = ENC_INC_2;
int8_t  enc_dir = 0;
bool    ch_flag = true;
bool    step_point = false;
bool    cal_mode = false;
bool    bfo_mode = false;
int32_t crystal_correction;              // Crystal Correction in PPB
int32_t bfo_offset;
volatile bool cal_dirty = false;
volatile bool bfo_dirty = false;

// ----------  PROTOTYPES  ----------
void rotary_encoder();
void toggle_cal_mode();
void toggle_bfo_mode();
void show_cal_ppm();
void show_bfo();
void si5351_set_freq(uint32_t f);
void set_inc();
void show_inc();
void show_freq(uint32_t f);

// ====================================================================
void setup() {
  pinMode(SW_STEP, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);

  attachInterrupt(0, rotary_encoder, CHANGE);
  attachInterrupt(1, rotary_encoder, CHANGE);

  /* ------  EEPROM LOADING  ------ */
  if (EEPROM.read(MAGIC_CAL) == MAGIC_CAL)
    EEPROM.get(EEPROM_CAL, crystal_correction);
  else
    crystal_correction = 114800;         // Default calibration value (ppb)

  if (EEPROM.read(MAGIC_BFO) == MAGIC_BFO)
    EEPROM.get(EEPROM_BFO, bfo_offset);
  else
    bfo_offset = -2500;                  

  /* ------  Si5351 INITIALIZATION  ------ */
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, crystal_correction);
  si5351.output_enable(SI5351_CLK0, 1);
  si5351.output_enable(SI5351_CLK1, 1);
  si5351.output_enable(SI5351_CLK2, 1);   
  si5351_set_freq(-DEFAULT_RX + IF_FREQ);
  si5351.set_freq((BFO_CENTER + bfo_offset) * SI5351_FREQ_MULT, SI5351_CLK2);

  /* ------  DISPLAY SETUP (6-DIGIT)  ------ */
  lc.shutdown(0, false);
  lc.setIntensity(0, 1);
  lc.setScanLimit(0, 5);                 // Limit hardware to 6 digits (0-5)
  lc.clearDisplay(0);
}

// ====================================================================
void loop() {
  /* ------  ON-THE-FLY CALIBRATION UPDATE  ------ */
  if (cal_dirty) {
    cal_dirty = false;
    si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, crystal_correction);
    si5351_set_freq(-rx + IF_FREQ);      // Update VFO immediately
    show_cal_ppm(); 
  }

  if (bfo_dirty) {
    bfo_dirty = false;
    int32_t jump = (bfo_offset >= 0) ? 120 : -120;
    si5351.set_freq((BFO_CENTER + bfo_offset + jump) * SI5351_FREQ_MULT, SI5351_CLK2);
    delayMicroseconds(100);
    si5351.set_freq((BFO_CENTER + bfo_offset) * SI5351_FREQ_MULT, SI5351_CLK2);
    show_bfo();
  }

  /* ------  UI TIMEOUT (30s)  ------ */
  static uint32_t modeTimeout = 0;
  if (cal_mode || bfo_mode) {
    if (modeTimeout == 0) modeTimeout = millis();
    if (millis() - modeTimeout > 30000) {   
      modeTimeout = 0;
      if (bfo_mode) toggle_bfo_mode();
      else if (cal_mode) toggle_cal_mode();
    }
  } else modeTimeout = 0;

  /* ------  NORMAL FREQUENCY UPDATE  ------ */
  if (ch_flag && !cal_mode && !bfo_mode) {
    ch_flag = false;
    rx += enc_dir * enc_inc;
    enc_dir = 0;
    rx = constrain(rx, BAND_L, BAND_H);
    si5351_set_freq(-rx + IF_FREQ);
    show_freq(rx);
  }

  /* ------  STEP BUTTON LOGIC  ------ */
  static uint32_t btnDown = 0;
  if (digitalRead(SW_STEP) == LOW) {
    if (btnDown == 0) btnDown = millis();
  } else {
    if (btnDown) {
      uint16_t dur = millis() - btnDown;
      btnDown = 0;
      if (dur > 2000) {                  // Long press: Toggle Menu
        if (bfo_mode) toggle_bfo_mode();
        else if (cal_mode) toggle_bfo_mode();
        else toggle_cal_mode();
      } else if (dur > 30) {             // Short click: Change Step
        set_inc();
        show_inc();
      }
    }
  }
}

// ====================================================================
void rotary_encoder() {
  uint8_t result = r.process();
  if (result) {
    if (bfo_mode) {
      bfo_offset += (result == DIR_CW) ? BFO_STEP : -BFO_STEP;
      bfo_offset = constrain(bfo_offset, -BFO_RANGE, BFO_RANGE);
      bfo_dirty = true;
    } else if (cal_mode) {
      // Adjustment step for calibration (2000 for speed)
      crystal_correction += (result == DIR_CW) ? 2000 : -2000; 
      crystal_correction = constrain(crystal_correction, -150000L, 150000L);
      cal_dirty = true;
    } else {
      ch_flag = true;
      enc_dir = (result == DIR_CW) ? 1 : -1;
    }
  }
}

// ====================================================================
void toggle_cal_mode() {
  cal_mode = !cal_mode;
  if (cal_mode) {                        
    show_cal_ppm();
  } else {                               // EXIT & SAVE
    EEPROM.put(EEPROM_CAL, crystal_correction);
    EEPROM.write(MAGIC_CAL, MAGIC_CAL);
    si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, crystal_correction);
    si5351_set_freq(-rx + IF_FREQ);
    show_freq(rx);
  }
}

// ====================================================================
void toggle_bfo_mode() {
  bfo_mode = !bfo_mode;
  if (bfo_mode) {                        
    show_bfo();
  } else {                               // EXIT & SAVE
    EEPROM.put(EEPROM_BFO, bfo_offset);
    EEPROM.write(MAGIC_BFO, MAGIC_BFO);
    show_freq(rx);
  }
}

void si5351_set_freq(uint32_t f) {
  si5351.set_freq(f * SI5351_FREQ_MULT, SI5351_CLK0);
}

void set_inc() {
  if (enc_inc == ENC_INC_1) {
    enc_inc = ENC_INC_2;
    uint32_t tmp = round(rx / (float)ENC_INC_2);
    rx = tmp * ENC_INC_2;
    ch_flag = true;
    step_point = false;
  } else {
    enc_inc = ENC_INC_1;
    step_point = true;
  }
}

void show_inc() {
  lc.setDigit(0, 0, (rx / 10) % 10, step_point);
}

// ====================================================================
void show_freq(uint32_t f) {
  uint8_t d5 = (f / 1000000) % 10;       // MHz
  uint8_t d4 = (f / 100000) % 10;        // 100 kHz
  uint8_t d3 = (f / 10000) % 10;         // 10 kHz
  uint8_t d2 = (f / 1000) % 10;          // 1 kHz
  uint8_t d1 = (f / 100) % 10;           // 100 Hz
  uint8_t d0 = (f / 10) % 10;            // 10 Hz

  lc.setDigit(0, 5, d5, true);           // Period after MHz (7.)
  lc.setDigit(0, 4, d4, false);
  lc.setDigit(0, 3, d3, false);
  lc.setDigit(0, 2, d2, true);           // Period after kHz (110.)
  lc.setDigit(0, 1, d1, false);
  lc.setDigit(0, 0, d0, step_point); 
}

// ====================================================================
void show_cal_ppm() {
  lc.clearDisplay(0);
  lc.setChar(0, 5, 'C', false);
  lc.setChar(0, 4, 'A', false);
  lc.setChar(0, 3, 'L', false);
  int32_t val = abs(crystal_correction) / 100;
  lc.setDigit(0, 2, (val / 100) % 10, false);
  lc.setDigit(0, 1, (val / 10) % 10, false);
  lc.setDigit(0, 0, val % 10, false);
}

void show_bfo() {
  lc.clearDisplay(0);
  lc.setChar(0, 5, 'B', false);
  lc.setChar(0, 4, 'F', false);
  lc.setChar(0, 3, 'O', false);
  int32_t val = abs(bfo_offset);
  lc.setDigit(0, 2, (val / 1000) % 10, false);
  lc.setDigit(0, 1, (val / 100) % 10, false);
  lc.setDigit(0, 0, (val / 10) % 10, false);
}
