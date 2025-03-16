#ifndef _I2C_LED_H_
#define _I2C_LED_H_
#include <stdint.h>


void i2c_lcd_1602_init(void);
void i2c_lcd_1602_clear(void);
void i2c_lcd_1602_home(void);
void i2c_lcd_1602_noDisplay(void);
void i2c_lcd_1602_display(void);
void i2c_lcd_1602_noBlink(void);
void i2c_lcd_1602_blink(void);
void i2c_lcd_1602_noCursor(void);
void i2c_lcd_1602_cursor(void);
void i2c_lcd_1602_scrollDisplayLeft(void);
void i2c_lcd_1602_scrollDisplayRight(void);
void i2c_lcd_1602_printLeft(void);
void i2c_lcd_1602_printRight(void);
void i2c_lcd_1602_leftToRight(void);
void i2c_lcd_1602_rightToLeft(void);
void i2c_lcd_1602_shiftIncrement(void);
void i2c_lcd_1602_shiftDecrement(void);
void i2c_lcd_1602_noBacklight(void);
void i2c_lcd_1602_backlight(void);
void i2c_lcd_1602_autoscroll(void);
void i2c_lcd_1602_noAutoscroll(void);
void i2c_lcd_1602_setCursor(uint8_t, uint8_t); 

#endif //_I2C_LED_H_