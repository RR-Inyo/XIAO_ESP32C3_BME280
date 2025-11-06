// ------------------------------------------------------------------------------------------
// ST7032.cpp - a library to control ST7032 baced LCD, C++ source file
// (c) 2025 @RR_Inyo
// ------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <Wire.h>
#include "ST7032.hpp"

// ST7032 constructor
ST7032::ST7032(void) {}

// ST7032 destructor
ST7032::~ST7032(void) {}

// ST7032 intializer
void ST7032::begin(void) {
  delay(50);
  writeCommand(TWO_LINE);
  writeCommand(TWO_LINE_IS1);
  writeCommand(INTOSC_BIAS);
  writeCommand(SET_CONTRAST_L | (0b00001111 & DEFAULT_CONTRAST));
  writeCommand(SET_CONTRAST_H | (0b00110000 & DEFAULT_CONTRAST) >> 4);
  writeCommand(FOLLOWER_CTRL);
  delay(200);
  writeCommand(TWO_LINE);
  writeCommand(DISPLAY_ON);
  writeCommand(RETURN_HOME);
  writeCommand(ENTRY_MODE_I);
  writeCommand(CLEAR_DISPLAY);
  delay(10);
}

// ST7032 I2C transmission, command
void ST7032::writeCommand(uint8_t cmd) {
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(COMMAND_MARKER);
  Wire.write(cmd);
  Wire.endTransmission();
  delayMicroseconds(800);
}

// ST7032 I2C transmission, data
void ST7032::writeData(uint8_t data) {
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(DATA_MARKER);
  Wire.write(data);
  Wire.endTransmission();
  delayMicroseconds(800);
}

// ST7032 Clear display
void ST7032::clear(void) {
  writeCommand(CLEAR_DISPLAY);
}

// ST7032 Set constrast
void ST7032::setContrast(uint8_t contr) {
  uint8_t contr_H = (0b00110000 & contr) >> 4;
  uint8_t contr_L = 0b00001111 & contr;

  writeCommand(TWO_LINE_IS1);
  writeCommand(SET_CONTRAST_H | contr_H);
  writeCommand(SET_CONTRAST_L | contr_L);
  writeCommand(TWO_LINE);
}

void ST7032::setCursor(uint8_t cur) {
  writeCommand(SET_DDRAM_ADDRESS | cur);
}

void ST7032::putChar(char c) {
  writeData(c);
}

void ST7032::putString(String mes) {
  char buf[N_BUF];
  mes.toCharArray(buf, N_BUF);
  for(int i = 0; i < N_BUF; i++) {
    if (buf[i] != '\0') {
      putChar(buf[i]);
    }
    else {
      break;
    }
  }
}