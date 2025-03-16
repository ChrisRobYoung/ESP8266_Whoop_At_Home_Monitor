#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

#include "driver/i2c.h"

//LCD commands
// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// flags for backlight control
#define LCD_BACKLIGHT 0x08
#define LCD_NOBACKLIGHT 0x00

#define En 0x04  // Enable bit
#define Rw 0x02  // Read/Write bit
#define Rs 0x01  // Register select bit

// Local variables
uint8_t g_backlight = LCD_BACKLIGHT;
uint8_t g_display_control = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
uint8_t g_entry_mode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;

static esp_err_t i2c_master_init()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_EXAMPLE_MASTER_SDA_IO;
    conf.sda_pullup_en = 1;
    conf.scl_io_num = I2C_EXAMPLE_MASTER_SCL_IO;
    conf.scl_pullup_en = 1;
    conf.clk_stretch_tick = 100; // 300 ticks, Clock stretch is about 210us, you can make changes according to the actual situation.
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode));
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    return ESP_OK;
}

static esp_err_t i2c_set_byte(uint8_t data)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0x27 << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief test code to write mpu6050
 *
 * 1. send data
 * ___________________________________________________________________________________________________
 * | start | slave_addr + wr_bit + ack | write reg_address + ack | write data_len byte + ack  | stop |
 * --------|---------------------------|-------------------------|----------------------------|------|
 *
 * @param i2c_num I2C port number
 * @param reg_address slave reg address
 * @param data data to send
 * @param data_len data length
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_FAIL Sending command error, slave doesn't ACK the transfer.
 *     - ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
 *     - ESP_ERR_TIMEOUT Operation timeout because the bus is busy.
 */
static esp_err_t i2c_lcd_write_enable(uint8_t data)
{
    uint8_t data_to_send = data | g_backlight;
    esp_err_t ret;
    ret = i2c_set_byte(data_to_send | En);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
    ret = i2c_set_byte(data_to_send & ~En);
    vTaskDelay(pdMS_TO_TICKS(50));
    return ret;
}

//write_mode is 0 for commands and 1 for setting character
static esp_err_t i2c_lcd_write_byte(uint8_t data, uint8_t write_mode)
{
    int ret;
    uint8_t highnib=data&0xf0;
	uint8_t lownib=(data<<4)&0xf0;
    ret = i2c_lcd_write_enable((highnib)|write_mode);
    if (ret != ESP_OK) {
        return ret;
    }
	ret = i2c_lcd_write_enable((lownib)|write_mode); 
    return ret; 
}

// When the display powers up, it is configured as follows:
//
// 1. Display clear
// 2. Function set: 
//    DL = 0; 4-bit interface data 
//    N = 1; 2-line display 
//    F = 0; 5x8 dot character font 
// 3. Display on/off control: 
//    D = 1; Display on 
//    C = 0; Cursor off 
//    B = 0; Blinking off 
// 4. Entry mode set: 
//    I/D = 1; Increment by 1
//    S = 0; No shift 
int i2c_lcd_1602_init(void)
{
    uint8_t cmd_data;
    i2c_master_init();
    
    //Allow startup power on if called immediately
    vTaskDelay(pdMS_TO_TICKS(50));
  
    cmd_data = 0x30;
    i2c_lcd_write_enable( cmd_data );
    vTaskDelay(pdMS_TO_TICKS(5));
    // second try
    i2c_lcd_write_enable(cmd_data);
    vTaskDelay(pdMS_TO_TICKS(1));
   
    // third go!
    ESP_ERROR_CHECK(i2c_lcd_write_enable(cmd_data)); 
    
    // finally, set to 4-bit interface
    cmd_data = 0x20;
    ESP_ERROR_CHECK(i2c_lcd_write_enable(cmd_data)); 

	// set # lines, font size, etc.
    cmd_data = LCD_FUNCTIONSET | LCD_4BITMODE |LCD_2LINE | LCD_5x8DOTS;
    ESP_ERROR_CHECK(i2c_lcd_write_byte(cmd_data, 0));  
	
	// turn the display on with no cursor or blinking default
    i2c_lcd_1602_display(); 
	
	// clear it off
    i2c_lcd_1602_clear();
	
    // set the entry mode
    // Initialize to default text direction (for roman languages)
    i2c_lcd_1602_leftToRight();

    i2c_lcd_1602_home();
    return ESP_OK;
}

void i2c_lcd_1602_clear(void)
{
    ESP_ERROR_CHECK(i2c_lcd_write_byte(LCD_CLEARDISPLAY, 0)); 
    vTaskDelay(pdMS_TO_TICKS(2));
}
void i2c_lcd_1602_home(void)
{
    ESP_ERROR_CHECK(i2c_lcd_write_byte(LCD_RETURNHOME, 0));
    vTaskDelay(pdMS_TO_TICKS(2));
}
void i2c_lcd_1602_noDisplay(void)
{
    g_display_control &= ~LCD_DISPLAYON;
    ESP_ERROR_CHECK(i2c_lcd_write_byte(LCD_DISPLAYCONTROL | g_display_control, 0)); 
}
void i2c_lcd_1602_display(void)
{
    g_display_control |= LCD_DISPLAYON;
    ESP_ERROR_CHECK(i2c_lcd_write_byte(LCD_DISPLAYCONTROL | g_display_control, 0)); 
}
void i2c_lcd_1602_noBlink(void)
{
    g_display_control &= ~LCD_BLINKON;
    ESP_ERROR_CHECK(i2c_lcd_write_byte(LCD_DISPLAYCONTROL | g_display_control, 0)); 
}
void i2c_lcd_1602_blink(void)
{
    g_display_control |= LCD_BLINKON;
    ESP_ERROR_CHECK(i2c_lcd_write_byte(LCD_DISPLAYCONTROL | g_display_control, 0)); 
}
void i2c_lcd_1602_noCursor(void)
{
    g_display_control &= ~LCD_CURSORON;
    ESP_ERROR_CHECK(i2c_lcd_write_byte(LCD_DISPLAYCONTROL | g_display_control, 0)); 
}
void i2c_lcd_1602_cursor(void)
{
    g_display_control |= LCD_CURSORON;
    ESP_ERROR_CHECK(i2c_lcd_write_byte(LCD_DISPLAYCONTROL | g_display_control, 0)); 
}
void i2c_lcd_1602_scrollDisplayLeft(void)
{
    ESP_ERROR_CHECK(i2c_lcd_write_byte(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT, 0)); 
}
void i2c_lcd_1602_scrollDisplayRight(void)
{
    ESP_ERROR_CHECK(i2c_lcd_write_byte(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT, 0)); 
}
void i2c_lcd_1602_leftToRight(void)
{
    g_entry_mode |= LCD_MOVERIGHT;
    ESP_ERROR_CHECK(i2c_lcd_write_byte(LCD_ENTRYMODESET | g_entry_mode, 0)); 
}
void i2c_lcd_1602_rightToLeft(void)
{
    g_entry_mode &= ~LCD_MOVERIGHT;
    ESP_ERROR_CHECK(i2c_lcd_write_byte(LCD_ENTRYMODESET | g_entry_mode, 0)); 
}
void i2c_lcd_1602_noBacklight(void)
{
    g_backlight = LCD_NOBACKLIGHT;
    ESP_ERROR_CHECK(i2c_set_byte(LCD_NOBACKLIGHT));
}
void i2c_lcd_1602_backlight(void)
{
    g_backlight = LCD_BACKLIGHT;
    ESP_ERROR_CHECK(i2c_set_byte(LCD_BACKLIGHT));
}
void i2c_lcd_1602_autoscroll(void)
{
    g_entry_mode |= LCD_ENTRYSHIFTINCREMENT;
    ESP_ERROR_CHECK(i2c_lcd_write_byte(LCD_ENTRYMODESET | g_entry_mode, 0)); 
}
void i2c_lcd_1602_noAutoscroll(void)
{
    g_entry_mode &= ~LCD_ENTRYSHIFTINCREMENT;
    ESP_ERROR_CHECK(i2c_lcd_write_byte(LCD_ENTRYMODESET | g_entry_mode, 0)); 
}
void i2c_lcd_1602_setCursor(uint8_t col, uint8_t row)
{
    int row_offsets[] = { 0x00, 0x40};
	ESP_ERROR_CHECK( i2c_lcd_write_byte( LCD_SETDDRAMADDR | (col + row_offsets[row]), 0 ) );
}

void i2c_lcd_1602_print(const char *buffer, int buffer_len)
{
    for(int char_index = 0; char_index < buffer_len; char_index++)
    {
        ESP_ERROR_CHECK( i2c_lcd_write_byte( (uint8_t) buffer[char_index], 0 ) );
    }
}