// ------------------------------------------------------------------------------------------
// ST7032.hpp - a library to control ST7032 baced LCD, header file
// (c) 2025 @RR_Inyo
// ------------------------------------------------------------------------------------------

class ST7032 {
  private:
  // Constants
  const uint8_t I2C_ADDRESS       = 0x3e;
  const uint8_t COMMAND_MARKER    = 0x00;
  const uint8_t DATA_MARKER       = 0x40;
  
  const uint8_t CLEAR_DISPLAY     = 0x01;
  const uint8_t RETURN_HOME       = 0x02;
  const uint8_t ENTRY_MODE_I      = 0x06;
  const uint8_t DISPLAY_ON        = 0x0c;
  const uint8_t DISPLAY_OFF       = 0x08;
  const uint8_t TWO_LINE          = 0x38;
  const uint8_t TWO_LINE_IS1      = 0x39;
  const uint8_t INTOSC_BIAS       = 0x14;
  const uint8_t FOLLOWER_CTRL     = 0x6c;
  const uint8_t SET_DDRAM_ADDRESS = 0x80;
  const uint8_t SET_CONTRAST_H    = 0x54;
  const uint8_t SET_CONTRAST_L    = 0x70;

  const uint8_t DEFAULT_CONTRAST  = 0x1f;

  const uint8_t N_BUF             = 20;

  // Internal functions
  void writeCommand(uint8_t cmd);
  void writeData(uint8_t data);

  public:
  ST7032(void);
  ~ST7032(void);
  void begin(void);
  void clear(void);
  void setContrast(uint8_t contr);
  void setCursor(uint8_t cur);
  void putChar(char c);
  void putString(String mes);
};